/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "esp_tls.h"
#include "esp_log.h"

#include "esp_transport.h"
#include "esp_transport_ssl.h"
#include "esp_transport_internal.h"

#define INVALID_SOCKET (-1)

#define GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t)         \
    transport_esp_tls_t *ssl = ssl_get_context_data(t);  \
    if (!ssl) { return; }

static const char *TAG = "transport_base";

typedef enum {
    TRANS_SSL_INIT = 0,
    TRANS_SSL_CONNECTING,
} transport_ssl_conn_state_t;

/**
 *  mbedtls specific transport data
 */
typedef struct transport_esp_tls {
    esp_tls_t                *tls;
    esp_tls_cfg_t            cfg;
    bool                     ssl_initialized;
    transport_ssl_conn_state_t conn_state;
    int                      sockfd;
} transport_esp_tls_t;

typedef struct saved_session_info_t {
    transport_esp_tls_t *locked_by_ssl;
    esp_tls_client_session_t* p_saved_session;
} saved_session_info_t;

#define ESP_TLS_MAX_NUM_SAVED_SESSIONS (2)

static saved_session_info_t g_saved_sessions[ESP_TLS_MAX_NUM_SAVED_SESSIONS];
static int32_t g_saved_session_last_used_idx;
static SemaphoreHandle_t g_saved_sessions_sema;
static StaticSemaphore_t g_saved_sessions_sema_mem;

static void
saved_sessions_sema_init(void) {
    if (NULL == g_saved_sessions_sema) {
        g_saved_sessions_sema = xSemaphoreCreateMutexStatic(&g_saved_sessions_sema_mem);
    }
}

static esp_tls_client_session_t*
get_saved_session_info_for_host(transport_esp_tls_t* const ssl, const char* const hostname) {
    saved_sessions_sema_init();
    esp_tls_client_session_t* p_saved_session = NULL;
    xSemaphoreTake(g_saved_sessions_sema, portMAX_DELAY);
    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        saved_session_info_t* const p_info = &g_saved_sessions[i];
        if ((NULL != p_info->p_saved_session)
            && (0 == strcmp(p_info->p_saved_session->saved_session.hostname, hostname)))
        {
            p_info->locked_by_ssl = ssl;
            p_saved_session = p_info->p_saved_session;
            break;
        }
    }
    xSemaphoreGive(g_saved_sessions_sema);
    return p_saved_session;
}

static void
unlock_saved_session(transport_esp_tls_t* const ssl) {
    saved_sessions_sema_init();
    ESP_LOGD(TAG, "Unlock TLS saved session for ssl=%p", ssl);
    xSemaphoreTake(g_saved_sessions_sema, portMAX_DELAY);
    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        saved_session_info_t* const p_info = &g_saved_sessions[i];
        if (ssl == p_info->locked_by_ssl)
        {
            p_info->locked_by_ssl = NULL;
            break;
        }
    }
    xSemaphoreGive(g_saved_sessions_sema);
}

static void save_new_session_ticket(transport_esp_tls_t *ssl)
{
    esp_tls_client_session_t* const p_session = esp_tls_get_client_session(ssl->tls);
    if (NULL == p_session) {
        ESP_LOGE(TAG, "Can't allocate memory for new session ticket");
        return;
    }

    saved_sessions_sema_init();
    xSemaphoreTake(g_saved_sessions_sema, portMAX_DELAY);
    int found_idx = -1;
    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        saved_session_info_t* const p_info = &g_saved_sessions[i];
        if (NULL != p_info->locked_by_ssl) {
            continue;
        }
        if (NULL == p_info->p_saved_session) {
            continue;
        }
        if (0 == strcmp(p_info->p_saved_session->saved_session.hostname, p_session->saved_session.hostname)) {
            found_idx = i;
            break;
        }
    }
    if (found_idx >= 0) {
        ESP_LOGI(TAG, "Got new TLS session ticket for host: %s, replace existing one", p_session->saved_session.hostname);
        esp_tls_free_client_session(g_saved_sessions[found_idx].p_saved_session);
        g_saved_sessions[found_idx].p_saved_session = p_session;
    } else {
        g_saved_session_last_used_idx += 1;
        g_saved_session_last_used_idx %= ESP_TLS_MAX_NUM_SAVED_SESSIONS;
        if (NULL == g_saved_sessions[g_saved_session_last_used_idx].locked_by_ssl) {
            found_idx = g_saved_session_last_used_idx;
        } else {
            g_saved_session_last_used_idx += 1;
            g_saved_session_last_used_idx %= ESP_TLS_MAX_NUM_SAVED_SESSIONS;
            if (NULL == g_saved_sessions[g_saved_session_last_used_idx].locked_by_ssl)
            {
                found_idx = g_saved_session_last_used_idx;
            }
        }
        if (found_idx >= 0) {
            ESP_LOGI(
                TAG,
                "Got new TLS session ticket for host: %s, save it at slot idx=%d",
                p_session->saved_session.hostname,
                g_saved_session_last_used_idx);
            if (NULL != g_saved_sessions[found_idx].p_saved_session) {
                esp_tls_free_client_session(g_saved_sessions[found_idx].p_saved_session);
            }
            g_saved_sessions[found_idx].p_saved_session = p_session;
        } else {
            ESP_LOGE(
                TAG,
                "Got new TLS session ticket for host: %s, but there are no free slots",
                p_session->saved_session.hostname);
        }
    }
    xSemaphoreGive(g_saved_sessions_sema);
}

/**
 * @brief      Destroys esp-tls transport used in the foundation transport
 *
 * @param[in]  transport esp-tls handle
 */
void esp_transport_esp_tls_destroy(struct transport_esp_tls* transport_esp_tls);

static inline transport_esp_tls_t *ssl_get_context_data(esp_transport_handle_t t)
{
    if (!t) {
        return NULL;
    }
    return (transport_esp_tls_t *)t->data;
}

static int esp_tls_connect_async(esp_transport_handle_t t, const char *host, int port, int timeout_ms, bool is_plain_tcp)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);
    if (ssl->conn_state == TRANS_SSL_INIT) {
        ssl->cfg.timeout_ms = timeout_ms;
        ssl->cfg.is_plain_tcp = is_plain_tcp;
        ssl->cfg.non_block = true;
        ssl->cfg.client_session = get_saved_session_info_for_host(ssl, host);
        if (NULL != ssl->cfg.client_session) {
            ESP_LOGI(TAG, "Reuse saved TLS session ticket for host: %s", host);
        } else {
            ESP_LOGI(TAG, "There is no saved TLS session ticket for host: %s", host);
        }
        ssl->ssl_initialized = true;
        ssl->tls = esp_tls_init();
        if (!ssl->tls) {
            return -1;
        }
        ssl->conn_state = TRANS_SSL_CONNECTING;
        ssl->sockfd = INVALID_SOCKET;
    }
    if (ssl->conn_state == TRANS_SSL_CONNECTING) {
        int progress = esp_tls_conn_new_async(host, strlen(host), port, &ssl->cfg, ssl->tls);
        if (progress >= 0) {
            if (esp_tls_get_conn_sockfd(ssl->tls, &ssl->sockfd) != ESP_OK) {
                ESP_LOGE(TAG, "Error in obtaining socket fd for the session");
                esp_tls_conn_destroy(ssl->tls);
                return -1;
            }
        }
        return progress;

    }
    return 0;
}

static inline int ssl_connect_async(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    const int ret = esp_tls_connect_async(t, host, port, timeout_ms, false);
    if (0 != ret) {
        // Unlock saved session if connection failed or successful handshake
        transport_esp_tls_t *ssl = ssl_get_context_data(t);
        unlock_saved_session(ssl);
    }
    return ret;
}

static inline int tcp_connect_async(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    return esp_tls_connect_async(t, host, port, timeout_ms, true);
}

static int ssl_connect(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    ssl->cfg.timeout_ms = timeout_ms;

    ssl->ssl_initialized = true;
    ssl->tls = esp_tls_init();
    if (ssl->tls == NULL) {
        ESP_LOGE(TAG, "Failed to initialize new connection object");
        capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_NO_MEM);
        return -1;
    }
    if (esp_tls_conn_new_sync(host, strlen(host), port, &ssl->cfg, ssl->tls) <= 0) {
        ESP_LOGE(TAG, "Failed to open a new connection");
        unlock_saved_session(ssl);
        esp_tls_error_handle_t esp_tls_error_handle;
        esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle);
        esp_transport_set_errors(t, esp_tls_error_handle);
        goto exit_failure;
    }
    unlock_saved_session(ssl);

    if (esp_tls_get_conn_sockfd(ssl->tls, &ssl->sockfd) != ESP_OK) {
        ESP_LOGE(TAG, "Error in obtaining socket fd for the session");
        goto exit_failure;
    }
    return 0;

exit_failure:
        esp_tls_conn_destroy(ssl->tls);
        ssl->tls = NULL;
        ssl->sockfd = INVALID_SOCKET;
        return -1;
}

static int tcp_connect(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);
    esp_tls_last_error_t *err_handle = esp_transport_get_error_handle(t);

    ssl->cfg.timeout_ms = timeout_ms;
    esp_err_t err = esp_tls_plain_tcp_connect(host, strlen(host), port, &ssl->cfg, err_handle, &ssl->sockfd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open a new connection: %d", err);
        err_handle->last_error = err;
        ssl->sockfd = INVALID_SOCKET;
        return -1;
    }
    return 0;
}

static int base_poll_read(esp_transport_handle_t t, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);
    int ret = -1;
    int remain = 0;
    struct timeval timeout;
    fd_set readset;
    fd_set errset;
    FD_ZERO(&readset);
    FD_ZERO(&errset);
    FD_SET(ssl->sockfd, &readset);
    FD_SET(ssl->sockfd, &errset);

    if (ssl->tls && (remain = esp_tls_get_bytes_avail(ssl->tls)) > 0) {
        ESP_LOGD(TAG, "remain data in cache, need to read again");
        return remain;
    }
    ret = select(ssl->sockfd + 1, &readset, NULL, &errset, esp_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(ssl->sockfd, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(ssl->sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        esp_transport_capture_errno(t, sock_errno);
        ESP_LOGE(TAG, "poll_read select error %d, errno = %s, fd = %d", sock_errno, strerror(sock_errno), ssl->sockfd);
        ret = -1;
    } else if (ret == 0) {
        ESP_LOGV(TAG, "poll_read: select - Timeout before any socket was ready!");
    }
    return ret;
}

static int base_poll_write(esp_transport_handle_t t, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);
    int ret = -1;
    struct timeval timeout;
    fd_set writeset;
    fd_set errset;
    FD_ZERO(&writeset);
    FD_ZERO(&errset);
    FD_SET(ssl->sockfd, &writeset);
    FD_SET(ssl->sockfd, &errset);
    ret = select(ssl->sockfd + 1, NULL, &writeset, &errset, esp_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(ssl->sockfd, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(ssl->sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        esp_transport_capture_errno(t, sock_errno);
        ESP_LOGE(TAG, "poll_write select error %d, errno = %s, fd = %d", sock_errno, strerror(sock_errno), ssl->sockfd);
        ret = -1;
    } else if (ret == 0) {
        ESP_LOGD(TAG, "poll_write: select - Timeout before any socket was ready!");
    }
    return ret;
}

static int ssl_write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll;
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    if ((poll = esp_transport_poll_write(t, timeout_ms)) <= 0) {
        ESP_LOGW(TAG, "Poll timeout or error, errno=%s, fd=%d, timeout_ms=%d", strerror(errno), ssl->sockfd, timeout_ms);
        return poll;
    }
    int ret = esp_tls_conn_write(ssl->tls, (const unsigned char *) buffer, len);
    if (ret < 0) {
        ESP_LOGE(TAG, "esp_tls_conn_write error, errno=%s", strerror(errno));
        esp_tls_error_handle_t esp_tls_error_handle;
        if (esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle) == ESP_OK) {
            esp_transport_set_errors(t, esp_tls_error_handle);
        } else {
            ESP_LOGE(TAG, "Error in obtaining the error handle");
        }
    }
    return ret;
}

static int tcp_write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll;
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    if ((poll = esp_transport_poll_write(t, timeout_ms)) <= 0) {
        ESP_LOGW(TAG, "Poll timeout or error, errno=%s, fd=%d, timeout_ms=%d", strerror(errno), ssl->sockfd, timeout_ms);
        return poll;
    }
    int ret = send(ssl->sockfd, (const unsigned char *) buffer, len, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "tcp_write error, errno=%s", strerror(errno));
        esp_transport_capture_errno(t, errno);
    }
    return ret;
}

static int ssl_read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    int poll = esp_transport_poll_read(t, timeout_ms);
    if (poll == -1) {
        return ERR_TCP_TRANSPORT_CONNECTION_FAILED;
    }
    if (poll == 0) {
        return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
    }

    int ret = esp_tls_conn_read(ssl->tls, (unsigned char *)buffer, len);
    if (ret < 0) {
        if (ret == ESP_TLS_ERR_SSL_WANT_READ || ret == ESP_TLS_ERR_SSL_TIMEOUT)
        {
            ESP_LOGW(TAG, "esp_tls_conn_read error - no data available (ret=-0x%x, errno=%s)", -ret, strerror(errno));
            ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
        } else if (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
            ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
            ESP_LOGI(TAG, "Cur free heap: %u", (unsigned)esp_get_free_heap_size());
            save_new_session_ticket(ssl);
            ESP_LOGI(TAG, "Cur free heap: %u", (unsigned)esp_get_free_heap_size());
        } else {
            ESP_LOGE(TAG, "esp_tls_conn_read error, ret=-0x%x, errno=%s", -ret, strerror(errno));
        }

        esp_tls_error_handle_t esp_tls_error_handle;
        if (esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle) == ESP_OK) {
            esp_transport_set_errors(t, esp_tls_error_handle);
        } else {
            ESP_LOGE(TAG, "Error in obtaining the error handle");
        }
    } else if (ret == 0) {
        if (poll > 0) {
            // no error, socket reads 0 while previously detected as readable -> connection has been closed cleanly
            capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN);
        }
        ret = ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN;
    }
    return ret;
}

static int tcp_read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    int poll = esp_transport_poll_read(t, timeout_ms);
    if (poll == -1) {
        return ERR_TCP_TRANSPORT_CONNECTION_FAILED;
    }
    if (poll == 0) {
        return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
    }

    int ret = recv(ssl->sockfd, (unsigned char *)buffer, len, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "tcp_read error, errno=%s", strerror(errno));
        esp_transport_capture_errno(t, errno);
        if (errno == EAGAIN) {
            ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
        } else {
            ret = ERR_TCP_TRANSPORT_CONNECTION_FAILED;
        }
    } else if (ret == 0) {
        if (poll > 0) {
            // no error, socket reads 0 while previously detected as readable -> connection has been closed cleanly
            capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN);
        }
        ret = ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN;
    }
    return ret;
}

static int base_close(esp_transport_handle_t t)
{
    int ret = -1;
    transport_esp_tls_t *ssl = ssl_get_context_data(t);
    if (ssl && ssl->ssl_initialized) {
        ret = esp_tls_conn_destroy(ssl->tls);
        ssl->tls = NULL;
        ssl->conn_state = TRANS_SSL_INIT;
        ssl->ssl_initialized = false;
        ssl->sockfd = INVALID_SOCKET;
    } else if (ssl && ssl->sockfd >= 0) {
        ret = close(ssl->sockfd);
        ssl->sockfd = INVALID_SOCKET;
    }
    return ret;
}

static int base_destroy(esp_transport_handle_t transport)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(transport);
    if (ssl) {
        esp_transport_close(transport);
        esp_transport_destroy_foundation_transport(transport->foundation);

        esp_transport_esp_tls_destroy(transport->data); // okay to pass NULL
    }
    return 0;
}

void esp_transport_ssl_enable_global_ca_store(esp_transport_handle_t t)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.use_global_ca_store = true;
}

#ifdef CONFIG_ESP_TLS_PSK_VERIFICATION
void esp_transport_ssl_set_psk_key_hint(esp_transport_handle_t t, const psk_hint_key_t *psk_hint_key)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.psk_hint_key =  psk_hint_key;
}
#endif

void esp_transport_ssl_set_cert_data(esp_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.cacert_pem_buf = (void *)data;
    ssl->cfg.cacert_pem_bytes = len + 1;
}

void esp_transport_ssl_set_cert_data_der(esp_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.cacert_buf = (void *)data;
    ssl->cfg.cacert_bytes = len;
}

void esp_transport_ssl_set_client_cert_data(esp_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientcert_pem_buf = (void *)data;
    ssl->cfg.clientcert_pem_bytes = len + 1;
}

void esp_transport_ssl_set_client_cert_data_der(esp_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientcert_buf = (void *)data;
    ssl->cfg.clientcert_bytes = len;
}

void esp_transport_ssl_set_client_key_data(esp_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientkey_pem_buf = (void *)data;
    ssl->cfg.clientkey_pem_bytes = len + 1;
}

void esp_transport_ssl_set_client_key_password(esp_transport_handle_t t, const char *password, int password_len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientkey_password = (void *)password;
    ssl->cfg.clientkey_password_len = password_len;
}

void esp_transport_ssl_set_client_key_data_der(esp_transport_handle_t t, const char *data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientkey_buf = (void *)data;
    ssl->cfg.clientkey_bytes = len;
}

#if defined(CONFIG_MBEDTLS_SSL_ALPN) || defined(CONFIG_WOLFSSL_HAVE_ALPN)
void esp_transport_ssl_set_alpn_protocol(esp_transport_handle_t t, const char **alpn_protos)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.alpn_protos = alpn_protos;
}
#endif

void esp_transport_ssl_skip_common_name_check(esp_transport_handle_t t)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.skip_common_name = true;
}

void esp_transport_ssl_set_common_name(esp_transport_handle_t t, const char *common_name)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.common_name = common_name;
}

#ifdef CONFIG_ESP_TLS_USE_SECURE_ELEMENT
void esp_transport_ssl_use_secure_element(esp_transport_handle_t t)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.use_secure_element = true;
}
#endif

#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
void esp_transport_ssl_crt_bundle_attach(esp_transport_handle_t t, esp_err_t ((*crt_bundle_attach)(void *conf)))
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.crt_bundle_attach = crt_bundle_attach;
}
#endif

static int base_get_socket(esp_transport_handle_t t)
{
    transport_esp_tls_t *ctx = ssl_get_context_data(t);
    if (ctx) {
        return ctx->sockfd;
    }
    return INVALID_SOCKET;
}

#ifdef CONFIG_ESP_TLS_USE_DS_PERIPHERAL
void esp_transport_ssl_set_ds_data(esp_transport_handle_t t, void *ds_data)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.ds_data = ds_data;
}
#endif

void esp_transport_ssl_set_keep_alive(esp_transport_handle_t t, esp_transport_keep_alive_t *keep_alive_cfg)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.keep_alive_cfg = (tls_keep_alive_cfg_t *) keep_alive_cfg;
}

void esp_transport_ssl_set_interface_name(esp_transport_handle_t t, struct ifreq *if_name)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.if_name = if_name;
}

static transport_esp_tls_t *esp_transport_esp_tls_create(void)
{
    transport_esp_tls_t *transport_esp_tls = calloc(1, sizeof(transport_esp_tls_t));
    if (transport_esp_tls == NULL) {
        return NULL;
    }
    transport_esp_tls->sockfd = INVALID_SOCKET;
    return transport_esp_tls;
}

static esp_transport_handle_t esp_transport_base_init(void)
{
    esp_transport_handle_t transport = esp_transport_init();
    if (transport == NULL) {
        return NULL;
    }
    transport->foundation = esp_transport_init_foundation_transport();
    ESP_TRANSPORT_MEM_CHECK(TAG, transport->foundation,
                            free(transport);
                            return NULL);

    transport->data = esp_transport_esp_tls_create();
    ESP_TRANSPORT_MEM_CHECK(TAG, transport->data,
                            free(transport->foundation);
                            free(transport);
                            return NULL)
    return transport;
}

esp_transport_handle_t esp_transport_ssl_init(void)
{
    esp_transport_handle_t ssl_transport = esp_transport_base_init();
    if (ssl_transport == NULL) {
        return NULL;
    }
    ((transport_esp_tls_t *)ssl_transport->data)->cfg.is_plain_tcp = false;
    esp_transport_set_func(ssl_transport, ssl_connect, ssl_read, ssl_write, base_close, base_poll_read, base_poll_write, base_destroy);
    esp_transport_set_async_connect_func(ssl_transport, ssl_connect_async);
    ssl_transport->_get_socket = base_get_socket;
    return ssl_transport;
}


void esp_transport_esp_tls_destroy(struct transport_esp_tls *transport_esp_tls)
{
    free(transport_esp_tls);
}

esp_transport_handle_t esp_transport_tcp_init(void)
{
    esp_transport_handle_t tcp_transport = esp_transport_base_init();
    if (tcp_transport == NULL) {
        return NULL;
    }
    ((transport_esp_tls_t *)tcp_transport->data)->cfg.is_plain_tcp = true;
    esp_transport_set_func(tcp_transport, tcp_connect, tcp_read, tcp_write, base_close, base_poll_read, base_poll_write, base_destroy);
    esp_transport_set_async_connect_func(tcp_transport, tcp_connect_async);
    tcp_transport->_get_socket = base_get_socket;
    return tcp_transport;
}

void esp_transport_tcp_set_keep_alive(esp_transport_handle_t t, esp_transport_keep_alive_t *keep_alive_cfg)
{
    return esp_transport_ssl_set_keep_alive(t, keep_alive_cfg);
}

void esp_transport_tcp_set_interface_name(esp_transport_handle_t t, struct ifreq *if_name)
{
    return esp_transport_ssl_set_interface_name(t, if_name);
}
