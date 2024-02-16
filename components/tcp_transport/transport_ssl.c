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
#define LOG_LOCAL_LEVEL 3
#include "esp_log.h"

#include "esp_transport.h"
#include "esp_transport_ssl.h"
#include "esp_transport_internal.h"
#include "snprintf_with_esp_err_desc.h"

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

#define ESP_TLS_MAX_NUM_SAVED_SESSIONS (2)

static esp_tls_client_session_t* g_saved_sessions[ESP_TLS_MAX_NUM_SAVED_SESSIONS];
static int32_t g_saved_session_last_used_idx;
static SemaphoreHandle_t g_saved_sessions_sema;
static StaticSemaphore_t g_saved_sessions_sema_mem;

static void
saved_sessions_sema_init(void) {
    if (NULL == g_saved_sessions_sema) {
        g_saved_sessions_sema = xSemaphoreCreateMutexStatic(&g_saved_sessions_sema_mem);
    }
}

static void
log_saved_session_tickets(void) {
    ESP_TRANSPORT_LOGD("LOG: List of saved TLS session tickets");
    int32_t last_used_idx = g_saved_session_last_used_idx;
    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        last_used_idx += 1;
        last_used_idx %= ESP_TLS_MAX_NUM_SAVED_SESSIONS;
        const esp_tls_client_session_t* const p_saved_session = g_saved_sessions[last_used_idx];
        if (NULL == p_saved_session) {
            ESP_TRANSPORT_LOGD("LOG: TLS session ticket[%d]: NULL", last_used_idx);
        } else {
            ESP_TRANSPORT_LOGD("LOG: TLS session ticket[%d]: %s",
                               last_used_idx, g_saved_sessions[last_used_idx]->saved_session.hostname);
        }
    }
}

static esp_tls_client_session_t*
get_saved_session_info_for_host(transport_esp_tls_t* const ssl, const char* const hostname) {
    saved_sessions_sema_init();
    esp_tls_client_session_t* p_found_saved_session = NULL;
    xSemaphoreTake(g_saved_sessions_sema, portMAX_DELAY);
    ESP_TRANSPORT_LOGD_FUNC("hostname=%s", hostname);
    log_saved_session_tickets();
    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        esp_tls_client_session_t* const p_saved_session = g_saved_sessions[i];
        if ((NULL != p_saved_session)
            && (0 == strcmp(p_saved_session->saved_session.hostname, hostname)))
        {
            p_found_saved_session = p_saved_session;
            g_saved_sessions[i] = NULL;
            ESP_TRANSPORT_LOGI("[%s] Get TLS saved session for ssl=%p: found at idx=%d", hostname, ssl, i);
            break;
        }
    }
    if (NULL == p_found_saved_session) {
        ESP_TRANSPORT_LOGI("[%s] Get TLS saved session for ssl=%p: not found", hostname, ssl);
    }
    xSemaphoreGive(g_saved_sessions_sema);
    return p_found_saved_session;
}

static void
unlock_saved_session(transport_esp_tls_t* const ssl) {
    saved_sessions_sema_init();
    ESP_TRANSPORT_LOGI("[%s] Unlock TLS saved session for ssl=%p", esp_tls_get_hostname(ssl->tls), ssl);
    xSemaphoreTake(g_saved_sessions_sema, portMAX_DELAY);
    if (NULL != ssl->cfg.client_session)
    {
        ESP_TRANSPORT_LOGI("Free TLS saved session for ssl=%p, session=%p: %s",
                           ssl, ssl->cfg.client_session, ssl->cfg.client_session->saved_session.hostname);
        esp_tls_free_client_session(ssl->cfg.client_session);
        ssl->cfg.client_session = NULL;
    }
    log_saved_session_tickets();
    xSemaphoreGive(g_saved_sessions_sema);
}

static void save_new_session_ticket(transport_esp_tls_t *ssl)
{
    esp_tls_client_session_t* const p_session = esp_tls_get_client_session(ssl->tls);
    if (NULL == p_session) {
        ESP_TRANSPORT_LOGE_FUNC("[%s] Can't allocate memory for new session ticket", esp_tls_get_hostname(ssl->tls));
        return;
    }

    saved_sessions_sema_init();
    xSemaphoreTake(g_saved_sessions_sema, portMAX_DELAY);

    ESP_TRANSPORT_LOGD_FUNC("[%s] Got new TLS session ticket, session=%p", p_session->saved_session.hostname, p_session);
    log_saved_session_tickets();

    int found_idx = -1;
    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        const esp_tls_client_session_t* const p_saved_session = g_saved_sessions[i];
        if (NULL == p_saved_session) {
            continue;
        }
        if (0 == strcmp(p_saved_session->saved_session.hostname, p_session->saved_session.hostname)) {
            found_idx = i;
            break;
        }
    }
    if (found_idx >= 0) {
        ESP_TRANSPORT_LOGI(
            "[%s] Got new TLS session ticket, replace existing one (slot %d)",
            p_session->saved_session.hostname,
            found_idx);
        ESP_TRANSPORT_LOGI("Free TLS saved session for ssl=%p, session=%p: %s",
                           ssl, g_saved_sessions[found_idx], g_saved_sessions[found_idx]->saved_session.hostname);
        esp_tls_free_client_session(g_saved_sessions[found_idx]);
        g_saved_sessions[found_idx] = p_session;
    } else {
        for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
            g_saved_session_last_used_idx += 1;
            g_saved_session_last_used_idx %= ESP_TLS_MAX_NUM_SAVED_SESSIONS;
            const esp_tls_client_session_t* const p_saved_session = g_saved_sessions[g_saved_session_last_used_idx];
            if (NULL == p_saved_session) {
                found_idx = g_saved_session_last_used_idx;
                break;
            }
        }
        if (found_idx >= 0) {
            ESP_TRANSPORT_LOGI(
                "[%s] Got new TLS session ticket, save it to an empty slot (slot %d)",
                p_session->saved_session.hostname,
                found_idx);
            g_saved_sessions[found_idx] = p_session;
        } else {
            g_saved_session_last_used_idx += 1;
            g_saved_session_last_used_idx %= ESP_TLS_MAX_NUM_SAVED_SESSIONS;

            ESP_TRANSPORT_LOGI("Free TLS saved session for ssl=%p, session=%p: %s",
                               ssl, g_saved_sessions[g_saved_session_last_used_idx],
                               g_saved_sessions[g_saved_session_last_used_idx]->saved_session.hostname);
            esp_tls_free_client_session(g_saved_sessions[g_saved_session_last_used_idx]);
            g_saved_sessions[g_saved_session_last_used_idx] = p_session;
            ESP_TRANSPORT_LOGI("[%s] Save new TLS session ticket (slot %d)",
                               p_session->saved_session.hostname, g_saved_session_last_used_idx);
        }
    }

    log_saved_session_tickets();

    xSemaphoreGive(g_saved_sessions_sema);
}

void esp_transport_ssl_clear_saved_session_tickets(void)
{
    saved_sessions_sema_init();
    xSemaphoreTake(g_saved_sessions_sema, portMAX_DELAY);

    ESP_TRANSPORT_LOGD_FUNC("Clear all saved session tickets");
    log_saved_session_tickets();

    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        esp_tls_client_session_t* const p_saved_session = g_saved_sessions[i];
        if (NULL == p_saved_session) {
            continue;
        }
        ESP_TRANSPORT_LOGI("[%s] Clear TLS session ticket (slot %d)", p_saved_session->saved_session.hostname, i);
        ESP_TRANSPORT_LOGI("Free TLS saved session, session=%p: %s", p_saved_session, p_saved_session->saved_session.hostname);
        esp_tls_free_client_session(p_saved_session);
        g_saved_sessions[i] = NULL;
    }
    log_saved_session_tickets();
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
        ESP_TRANSPORT_LOGD_FUNC("TRANS_SSL_INIT[%s:%d] timeout=%d ms, is_plain_tcp=%d", host, port, timeout_ms, is_plain_tcp);
        ssl->cfg.timeout_ms = timeout_ms;
        ssl->cfg.is_plain_tcp = is_plain_tcp;
        ssl->cfg.non_block = true;
        if (!is_plain_tcp) {
            ssl->cfg.client_session = get_saved_session_info_for_host(ssl, host);
            if (NULL != ssl->cfg.client_session) {
                ESP_TRANSPORT_LOGI("[%s] Reuse saved TLS session ticket for host", host);
            } else {
                ESP_TRANSPORT_LOGI("[%s] There is no saved TLS session ticket for host", host);
            }
        }
        ssl->ssl_initialized = true;
        ssl->tls = esp_tls_init();
        ESP_LOGD(TAG, "%s: esp_tls_init, tls=%p", __func__, ssl->tls);
        if (!ssl->tls) {
            return -1;
        }
        ssl->conn_state = TRANS_SSL_CONNECTING;
        ssl->sockfd = INVALID_SOCKET;
    }
    if (ssl->conn_state == TRANS_SSL_CONNECTING) {
        ESP_TRANSPORT_LOGD_FUNC("TRANS_SSL_CONNECTING[%s:%d]", host, port);
        ESP_LOGD(TAG, "%s: esp_tls_conn_new_async", __func__);
        int progress = esp_tls_conn_new_async(host, strlen(host), port, &ssl->cfg, ssl->tls);
        if (progress >= 0) {
            if (esp_tls_get_conn_sockfd(ssl->tls, &ssl->sockfd) != ESP_OK) {
                ESP_TRANSPORT_LOGE_FUNC("[%s] Error in obtaining socket fd for the session",
                        esp_tls_get_hostname(ssl->tls));
                esp_tls_conn_destroy(ssl->tls);
                return -1;
            }
            if (0 == progress) {
                ESP_TRANSPORT_LOGD_FUNC("TRANS_SSL_CONNECTING[%s:%d] esp_tls_conn_new_async: In progress", host, port);
            } else {
                ESP_TRANSPORT_LOGD_FUNC("TRANS_SSL_CONNECTING[%s:%d] esp_tls_conn_new_async: Connected", host, port);
            }
        } else {
            ESP_TRANSPORT_LOGE_FUNC("[%s:%d] esp_tls_conn_new_async: Failed, res=%d", host, port, progress);
            ESP_LOGD(TAG, "%s: esp_transport_set_errors", __func__);
            esp_tls_error_handle_t esp_tls_error_handle;
            esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle);
            esp_transport_set_errors(t, esp_tls_error_handle);
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
    ESP_LOGD(TAG, "%s: esp_tls_init, tls=%p", __func__, ssl->tls);
    if (ssl->tls == NULL) {
        ESP_TRANSPORT_LOGE_FUNC("[%s] Failed to initialize new connection object", host);
        capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_NO_MEM);
        return -1;
    }
    if (esp_tls_conn_new_sync(host, strlen(host), port, &ssl->cfg, ssl->tls) <= 0) {
        ESP_TRANSPORT_LOGE_FUNC("[%s] Failed to open a new connection", host);
        ESP_LOGW(TAG, "Cur free heap: %u", (unsigned)esp_get_free_heap_size());
        unlock_saved_session(ssl);
        esp_tls_error_handle_t esp_tls_error_handle;
        esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle);
        esp_transport_set_errors(t, esp_tls_error_handle);
        goto exit_failure;
    }
    unlock_saved_session(ssl);

    if (esp_tls_get_conn_sockfd(ssl->tls, &ssl->sockfd) != ESP_OK) {
        ESP_TRANSPORT_LOGE_FUNC("[%s] Error in obtaining socket fd for the session", host);
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

    ESP_TRANSPORT_LOGI_FUNC("[%s:%d] Connect to host with timeout %d ms", host, port, timeout_ms);

    ssl->cfg.timeout_ms = timeout_ms;
    esp_err_t err = esp_tls_plain_tcp_connect(host, strlen(host), port, &ssl->cfg, err_handle, &ssl->sockfd);
    if (err != ESP_OK) {
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(err);
        ESP_TRANSPORT_LOGE_FUNC("[%s:%d] failed to connect: %d (%s)",
                                host, port, err, err_desc.buf ? err_desc.buf : "");
        str_buf_free_buf(&err_desc);

        err_handle->last_error = err;
        esp_transport_capture_errno(t, err);
        ssl->sockfd = INVALID_SOCKET;
        return -1;
    }
    ESP_TRANSPORT_LOGI_FUNC("[%s:%d] connected successfully, sock=%d", host, port, ssl->sockfd);
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

    if (ssl->tls) {
        remain = esp_tls_get_bytes_avail(ssl->tls);
        if (remain > 0) {
            ESP_LOGD(TAG, "remain data in cache, need to read again");
            return remain;
        }
    }
    ret = select(ssl->sockfd + 1, &readset, NULL, &errset, esp_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(ssl->sockfd, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(ssl->sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        esp_transport_capture_errno(t, sock_errno);
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(sock_errno);
        ESP_TRANSPORT_LOGE_FUNC("[%s] poll_read select error %d (%s), fd = %d",
                 esp_tls_get_hostname(ssl->tls),
                 sock_errno, (NULL != err_desc.buf) ? err_desc.buf : "", ssl->sockfd);
        str_buf_free_buf(&err_desc);
        errno = sock_errno;
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
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(sock_errno);
        ESP_TRANSPORT_LOGE_FUNC("[%s] poll_write select error %d (%s), fd = %d",
                 esp_tls_get_hostname(ssl->tls),
                 sock_errno, (NULL != err_desc.buf) ? err_desc.buf : "", ssl->sockfd);
        str_buf_free_buf(&err_desc);
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
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
        ESP_TRANSPORT_LOGW_FUNC("[%s] Poll timeout or error, errno=%d (%s), fd=%d, timeout_ms=%d",
                 esp_tls_get_hostname(ssl->tls),
                 errno,
                 (NULL != err_desc.buf) ? err_desc.buf : "",
                 ssl->sockfd, timeout_ms);
        str_buf_free_buf(&err_desc);
        return poll;
    }
    int ret = esp_tls_conn_write(ssl->tls, (const unsigned char *) buffer, len);
    if (ret < 0) {
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
        ESP_LOGD(TAG, "esp_tls_conn_write error, errno=%d (%s)", errno, (NULL != err_desc.buf) ? err_desc.buf : "");
        str_buf_free_buf(&err_desc);
        esp_tls_error_handle_t esp_tls_error_handle;
        if (esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle) == ESP_OK) {
            esp_transport_set_errors(t, esp_tls_error_handle);
        } else {
            ESP_TRANSPORT_LOGE_FUNC("[%s] Error in obtaining the error handle", esp_tls_get_hostname(ssl->tls));
        }
    }
    return ret;
}

static int tcp_write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll;
    const transport_esp_tls_t *ssl = ssl_get_context_data(t);

    if ((poll = esp_transport_poll_write(t, timeout_ms)) <= 0) {
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
        ESP_TRANSPORT_LOGW_FUNC("[sock=%d] Poll timeout or error, errno=%d (%s), timeout_ms=%d",
                ssl->sockfd, errno, (NULL != err_desc.buf) ? err_desc.buf : "", timeout_ms);
        str_buf_free_buf(&err_desc);
        return poll;
    }
    int ret = send(ssl->sockfd, (const unsigned char *) buffer, len, 0);
    if (ret < 0) {
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
        ESP_TRANSPORT_LOGE_FUNC("[%s] tcp_write error, errno=%d (%s)",
                esp_tls_get_hostname(ssl->tls), errno, (NULL != err_desc.buf) ? err_desc.buf : "");
        str_buf_free_buf(&err_desc);
        esp_transport_capture_errno(t, errno);
    }
    return ret;
}

static int ssl_read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    int poll = esp_transport_poll_read(t, timeout_ms);
    if (poll == -1) {
        ESP_TRANSPORT_LOGE_FUNC("[%s] esp_transport_poll_read: ERR_TCP_TRANSPORT_CONNECTION_FAILED",
                                esp_tls_get_hostname(ssl->tls));
        return ERR_TCP_TRANSPORT_CONNECTION_FAILED;
    }
    if (poll == 0) {
        ESP_TRANSPORT_LOGD_FUNC("[%s] esp_transport_poll_read: ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT",
                                esp_tls_get_hostname(ssl->tls));
        return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
    }

    int ret = esp_tls_conn_read(ssl->tls, (unsigned char *)buffer, len);
    if (ret < 0) {
        if (ret == ESP_TLS_ERR_SSL_WANT_READ || ret == ESP_TLS_ERR_SSL_TIMEOUT) {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(ret);
            ESP_TRANSPORT_LOGW_FUNC("[%s] esp_tls_conn_read error - no data available, ret=-0x%x (%s)",
                     esp_tls_get_hostname(ssl->tls), -ret, (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
            ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
        } else if (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
            ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
            ESP_LOGI(TAG, "Cur free heap: %u", (unsigned)esp_get_free_heap_size());
            save_new_session_ticket(ssl);
            ESP_LOGI(TAG, "Cur free heap: %u", (unsigned)esp_get_free_heap_size());
        } else {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(ret);
            ESP_TRANSPORT_LOGE_FUNC("[%s] esp_tls_conn_read error, ret=-0x%x (%s)",
                     esp_tls_get_hostname(ssl->tls), -ret, (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
        }

        esp_tls_error_handle_t esp_tls_error_handle;
        if (esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle) == ESP_OK) {
            esp_transport_set_errors(t, esp_tls_error_handle);
        } else {
            ESP_TRANSPORT_LOGE_FUNC("[%s] Error in obtaining the error handle", esp_tls_get_hostname(ssl->tls));
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
    const transport_esp_tls_t *ssl = ssl_get_context_data(t);

    int poll = esp_transport_poll_read(t, timeout_ms);
    if (poll == -1) {
        ESP_TRANSPORT_LOGE_FUNC("[%s] esp_transport_poll_read: ERR_TCP_TRANSPORT_CONNECTION_FAILED",
                                esp_tls_get_hostname(ssl->tls));
        return ERR_TCP_TRANSPORT_CONNECTION_FAILED;
    }
    if (poll == 0) {
        ESP_TRANSPORT_LOGE_FUNC("[%s] esp_transport_poll_read: ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT",
                                esp_tls_get_hostname(ssl->tls));
        return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
    }

    int ret = recv(ssl->sockfd, (unsigned char *)buffer, len, 0);
    if (ret < 0) {
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
        ESP_TRANSPORT_LOGE_FUNC("[sock=%d] tcp_read error, errno=%d (%s)",
                ssl->sockfd, errno, (NULL != err_desc.buf) ? err_desc.buf : "");
        str_buf_free_buf(&err_desc);
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
    ESP_LOGD(TAG, "%s: ssl=%p", __func__, ssl);
    if (ssl && ssl->ssl_initialized) {
        ESP_LOGD(TAG, "%s: tls=%p, host=%s", __func__, ssl->tls, esp_tls_get_hostname(ssl->tls));
        if (NULL == ssl->tls)
        {
            ESP_TRANSPORT_LOGW_FUNC("[%s] tls=NULL", esp_tls_get_hostname(ssl->tls));
            ret = 0;
        }
        else
        {
            ESP_TRANSPORT_LOGI_FUNC("[%s] tls=%p", esp_tls_get_hostname(ssl->tls), ssl->tls);
            ret = esp_tls_conn_destroy(ssl->tls);
        }
        ssl->tls = NULL;
        ssl->conn_state = TRANS_SSL_INIT;
        ssl->ssl_initialized = false;
        ssl->sockfd = INVALID_SOCKET;
    } else if (ssl && ssl->sockfd >= 0) {
        ESP_LOGD(TAG, "%s: tls=%p, host=%s, ssl->sockfd=%d", __func__, ssl->tls, esp_tls_get_hostname(ssl->tls), ssl->sockfd);
        ESP_LOGD(TAG, "%s: Close socket %d", __func__, ssl->sockfd);
        ret = close(ssl->sockfd);
        ssl->sockfd = INVALID_SOCKET;
    }
    else {
        ESP_LOGD(TAG, "%s: tls=%p, host=%s", __func__, ssl->tls, esp_tls_get_hostname(ssl->tls));
        ESP_LOGD(TAG, "%s: ssl->sockfd=%d", __func__, ssl->sockfd);
    }
    return ret;
}

static int base_destroy(esp_transport_handle_t transport)
{
    const transport_esp_tls_t * const ssl = ssl_get_context_data(transport);
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

void esp_transport_ssl_set_buffer_size(esp_transport_handle_t t, const size_t ssl_in_content_len, const size_t ssl_out_content_len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.ssl_in_content_len = (0 != ssl_in_content_len) ? ssl_in_content_len : CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN;
    ssl->cfg.ssl_out_content_len = (0 != ssl_out_content_len) ? ssl_out_content_len : CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN;
    ESP_TRANSPORT_LOGI("[%s] Configure size of TLS I/O buffers: in_content_len=%u, out_content_len=%u",
                       esp_tls_get_hostname(ssl->tls), (unsigned)ssl->cfg.ssl_in_content_len, (unsigned)ssl->cfg.ssl_out_content_len);
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
