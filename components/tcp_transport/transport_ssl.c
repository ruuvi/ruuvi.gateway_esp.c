/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <esp_attr.h>
#include "esp_tls.h"
#define LOG_LOCAL_LEVEL 3
#include "esp_log.h"

#include "esp_heap_caps.h"
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
    bool                     flag_timer_started;
    TickType_t               timer_start;
} transport_esp_tls_t;

typedef struct transport_ssl_saved_session_ticket_slot_t {
    mbedtls_ssl_hostname_t   hostname;
    esp_tls_client_session_t session;
    bool                     is_valid;
    bool                     is_locked;
} transport_ssl_saved_session_ticket_slot_t;

static transport_ssl_saved_session_ticket_slot_t g_saved_session_tickets[ESP_TLS_MAX_NUM_SAVED_SESSIONS];
static SemaphoreHandle_t IRAM_ATTR g_saved_sessions_sema;
static StaticSemaphore_t g_saved_sessions_sema_mem;

static void log_heap_info(void) {
    ESP_LOGI(TAG,
        "Cur free heap: default: max block %u, free %u; IRAM: max block %u, free %u",
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT | MALLOC_CAP_EXEC),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT | MALLOC_CAP_EXEC));
}

static void
saved_sessions_sema_lock(void) {
    xSemaphoreTake(g_saved_sessions_sema, portMAX_DELAY);
}

static void
saved_sessions_sema_unlock(void) {
    xSemaphoreGive(g_saved_sessions_sema);
}

static transport_ssl_saved_session_ticket_slot_t* find_session_ticket_slot_for_host(const char* const hostname,
                                                                                    int32_t* const p_slot_idx) {
    int32_t slot_idx = -1;
    for (int32_t i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        if ('\0' == g_saved_session_tickets[i].hostname.buf[0]) {
            continue;
        }
        if (0 == strcmp(g_saved_session_tickets[i].hostname.buf, hostname)) {
            slot_idx = i;
            break;
        }
    }
    if (NULL != p_slot_idx) {
        *p_slot_idx = slot_idx;
    }
    return (-1 != slot_idx) ? &g_saved_session_tickets[slot_idx] : NULL;
}

static transport_ssl_saved_session_ticket_slot_t* find_session_ticket_slot_for_session(
        const esp_tls_client_session_t* const p_session) {
    transport_ssl_saved_session_ticket_slot_t* p_session_ticket_slot = NULL;
    for (uint32_t i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        if (&g_saved_session_tickets[i].session == p_session) {
            p_session_ticket_slot = &g_saved_session_tickets[i];
            break;
        }
    }
    return p_session_ticket_slot;
}

void esp_transport_ssl_init_saved_tickets_storage(void)
{
    g_saved_sessions_sema = xSemaphoreCreateMutexStatic(&g_saved_sessions_sema_mem);

    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; i++) {
        transport_ssl_saved_session_ticket_slot_t* const p_slot = &g_saved_session_tickets[i];
        memset(p_slot, 0, sizeof(*p_slot));
        mbedtls_ssl_session_init(&p_slot->session.saved_session);
        p_slot->hostname.buf[0] = '\0';
        p_slot->is_valid = false;
        p_slot->is_locked = false;
    }
}

bool esp_transport_ssl_set_saved_ticket_hostname(
    const uint32_t idx,
    const mbedtls_ssl_hostname_t* const p_hostname)
{
    if (idx >= ESP_TLS_MAX_NUM_SAVED_SESSIONS) {
        ESP_LOGE(TAG, "Invalid index %" PRIu32 " for setting saved ticket hostname", idx);
        return false;
    }
    saved_sessions_sema_lock();
    if (NULL == p_hostname) {
        ESP_LOGI(TAG, "Clear hostname for saved TLS session ticket at idx %" PRIu32, idx);
        g_saved_session_tickets[idx].hostname.buf[0] = '\0';
    } else {
        ESP_LOGI(TAG, "Set hostname for saved TLS session ticket at idx %" PRIu32 ": '%s'", idx, p_hostname->buf);
        g_saved_session_tickets[idx].hostname = *p_hostname;
    }
    saved_sessions_sema_unlock();
    return true;
}

static void
log_saved_session_tickets(void) {
    ESP_TRANSPORT_LOGD("LOG: List of saved TLS session tickets");
    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        const transport_ssl_saved_session_ticket_slot_t* const p_slot = &g_saved_session_tickets[i];
        ESP_TRANSPORT_LOGD("LOG: TLS session ticket[%d]: hostname='%s': is_valid=%d, is_locked=%d: %p",
            i, p_slot->hostname.buf, p_slot->is_valid, p_slot->is_locked, &p_slot->session);
    }
}

static esp_tls_client_session_t*
get_saved_session_info_for_host(const char* const hostname) {
    if (NULL == hostname) {
        ESP_TRANSPORT_LOGE_FUNC("hostname is NULL");
        return NULL;
    }
    if ('\0' == hostname[0]) {
        ESP_TRANSPORT_LOGE_FUNC("hostname is empty");
        return NULL;
    }

    saved_sessions_sema_lock();
    ESP_TRANSPORT_LOGD_FUNC("hostname=%s", hostname);
    log_saved_session_tickets();

    int32_t slot_idx = -1;
    transport_ssl_saved_session_ticket_slot_t* const p_slot = find_session_ticket_slot_for_host(hostname, &slot_idx);
    if (NULL == p_slot) {
        ESP_TRANSPORT_LOGW_FUNC("No slot found for hostname=%s", hostname);
        saved_sessions_sema_unlock();
        return NULL;
    }
    if (!p_slot->is_valid) {
        ESP_TRANSPORT_LOGI_FUNC("Slot %d found for hostname=%s, but it is not valid", slot_idx, hostname);
        saved_sessions_sema_unlock();
        return NULL;
    }
    if (p_slot->is_locked) {
        ESP_TRANSPORT_LOGE_FUNC("Slot %d found for hostname=%s, but it is locked", slot_idx, hostname);
        saved_sessions_sema_unlock();
        return NULL;
    }
    ESP_TRANSPORT_LOGD("[%s] Lock TLS saved session %p at slot %d", hostname, &p_slot->session, slot_idx);
    p_slot->is_locked = true;
    saved_sessions_sema_unlock();
    return &p_slot->session;
}

static void
unlock_saved_session(transport_esp_tls_t* const ssl) {
    saved_sessions_sema_lock();
    if (NULL != ssl->cfg.client_session) {
        transport_ssl_saved_session_ticket_slot_t* const p_slot =
            find_session_ticket_slot_for_session(ssl->cfg.client_session);
        if (NULL == p_slot) {
            ESP_TRANSPORT_LOGE_FUNC("[%s] Unlock TLS saved session for ssl=%p, session=%p: Can't find slot",
                esp_tls_get_hostname(ssl->tls), ssl, ssl->cfg.client_session);
        } else {
            ESP_TRANSPORT_LOGI("[%s] Unlock and free TLS saved session slot for ssl=%p, session=%p",
                               esp_tls_get_hostname(ssl->tls), ssl, &p_slot->session);
            mbedtls_ssl_session_free(&p_slot->session.saved_session);
            mbedtls_ssl_session_init(&p_slot->session.saved_session);
            p_slot->is_locked = false;
            p_slot->is_valid = false;
        }
        ssl->cfg.client_session = NULL;
    } else {
        ESP_TRANSPORT_LOGW("[%s] Unlock TLS saved session for ssl=%p, session=%p: client_session is NULL",
                           esp_tls_get_hostname(ssl->tls), ssl, ssl->cfg.client_session);
    }
    log_saved_session_tickets();
    log_heap_info();
    saved_sessions_sema_unlock();
}

static void save_new_session_ticket(transport_esp_tls_t *ssl)
{
    saved_sessions_sema_lock();

    log_saved_session_tickets();

    int32_t slot_idx = -1;
    transport_ssl_saved_session_ticket_slot_t* const p_slot = find_session_ticket_slot_for_host(
        esp_tls_get_hostname(ssl->tls), &slot_idx);
    if (NULL == p_slot) {
        ESP_TRANSPORT_LOGW_FUNC("[%s] Can't find slot for ticket for hostname", esp_tls_get_hostname(ssl->tls));
        saved_sessions_sema_unlock();
        return;
    }

    if (p_slot->is_valid) {
        ESP_TRANSPORT_LOGI("[%s] Got new TLS session ticket, replace existing one at slot %d (ssl=%p, session=%p)",
            p_slot->hostname.buf, (int)slot_idx, ssl, &p_slot->session);
        mbedtls_ssl_session_free(&p_slot->session.saved_session);
        mbedtls_ssl_session_init(&p_slot->session.saved_session);
        p_slot->is_valid = false;
    } else {
        ESP_TRANSPORT_LOGI("[%s] Got new TLS session ticket, save it to an empty slot %d (ssl=%p, session=%p)",
            p_slot->hostname.buf, slot_idx, ssl, &p_slot->session);
    }

    if (!esp_tls_copy_client_session(ssl->tls, &p_slot->session)) {
        ESP_TRANSPORT_LOGE_FUNC("[%s] Error while copying the client ssl session", esp_tls_get_hostname(ssl->tls));
        mbedtls_ssl_session_free(&p_slot->session.saved_session);
        mbedtls_ssl_session_init(&p_slot->session.saved_session);
        saved_sessions_sema_unlock();
        return;
    }
    p_slot->is_valid = true;
    log_saved_session_tickets();
    saved_sessions_sema_unlock();
}

void esp_transport_ssl_clear_saved_session_tickets(void)
{
    saved_sessions_sema_lock();

    ESP_TRANSPORT_LOGD_FUNC("Clear all saved session tickets");
    log_saved_session_tickets();

    for (int i = 0; i < ESP_TLS_MAX_NUM_SAVED_SESSIONS; ++i) {
        transport_ssl_saved_session_ticket_slot_t* const p_slot = &g_saved_session_tickets[i];
        if (!p_slot->is_valid) {
            continue;
        }
        ESP_TRANSPORT_LOGI("[%s] Clear TLS session ticket (slot %d)", p_slot->session.saved_session.ticket_hostname.buf, i);
        mbedtls_ssl_session_free(&p_slot->session.saved_session);
        mbedtls_ssl_session_init(&p_slot->session.saved_session);
        p_slot->is_valid = false;
    }
    log_saved_session_tickets();
    log_heap_info();
    saved_sessions_sema_unlock();
}

static inline transport_esp_tls_t *ssl_get_context_data(esp_transport_handle_t t)
{
    if (!t) {
        return NULL;
    }
    return (transport_esp_tls_t *)t->data;
}

/**
 * @brief Perform asynchronous TLS/TCP connection establishment
 *
 * Manages the state machine for non-blocking connection establishment to a remote host.
 * Supports both plain TCP and TLS connections with optional session resumption.
 *
 * @param[in] t             ESP transport handle for the connection
 * @param[in] host          Remote host name or IP address
 * @param[in] port          Remote port number
 * @param[in] timeout_ms    Connection timeout in milliseconds
 * @param[in] is_plain_tcp  If true, establishes plain TCP connection; if false, establishes TLS connection
 *
 * @return  1 if connection successfully established
 * @return  0 if connection is in progress (call again to continue)
 * @return <0 on error (connection failed)
 *
 * @note This function maintains internal state across multiple calls to perform non-blocking connection
 * @note For TLS connections, attempts to reuse saved session tickets if available
 * @note Socket descriptor is obtained and stored once connection progresses past initial state
 */
static int esp_tls_connect_async(esp_transport_handle_t t, const char *host, int port, int timeout_ms, bool is_plain_tcp)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);
    if (ssl->conn_state == TRANS_SSL_INIT) {
        ESP_TRANSPORT_LOGD_FUNC("TRANS_SSL_INIT[%s:%d] timeout=%d ms, is_plain_tcp=%d", host, port, timeout_ms, is_plain_tcp);
        ssl->cfg.timeout_ms = timeout_ms;
        ssl->cfg.is_plain_tcp = is_plain_tcp;
        ssl->cfg.non_block = true;
        if (!is_plain_tcp) {
            ssl->cfg.client_session = get_saved_session_info_for_host(host);
            if (NULL != ssl->cfg.client_session) {
                ESP_TRANSPORT_LOGI("[%s:%d] Reuse saved TLS session ticket for host (ssl=%p): %p", host, port, ssl, ssl->cfg.client_session);
            } else {
                ESP_TRANSPORT_LOGI("[%s:%d] There is no saved TLS session ticket for host (ssl=%p)", host, port, ssl);
            }
        }
        ssl->ssl_initialized = true;
        ssl->tls = esp_tls_init();
        ESP_LOGD(TAG, "%s: esp_tls_init, tls=%p", __func__, ssl->tls);
        if (!ssl->tls) {
            ESP_TRANSPORT_LOGE_FUNC("[%s:%d] esp_tls_init failed", host, port);
            return -1;
        }
        ssl->conn_state = TRANS_SSL_CONNECTING;
        ssl->sockfd = INVALID_SOCKET;
    }
    if (ssl->conn_state == TRANS_SSL_CONNECTING) {
        ESP_TRANSPORT_LOGD_FUNC("TRANS_SSL_CONNECTING[%s:%d]", host, port);
        int progress = esp_tls_conn_new_async(host, strlen(host), port, &ssl->cfg, ssl->tls);
        ESP_TRANSPORT_LOGD_FUNC("esp_tls_conn_new_async: progress=%d", progress);
        if (progress >= 0) {
            if (esp_tls_get_conn_sockfd(ssl->tls, &ssl->sockfd) != ESP_OK) {
                ESP_TRANSPORT_LOGE_FUNC("[%s:%d] Error in obtaining socket fd for the session", host, port);
                esp_tls_conn_destroy(ssl->tls);
                return -1;
            }
            if (0 == progress) {
                ESP_TRANSPORT_LOGD_FUNC("TRANS_SSL_CONNECTING[%s:%d] esp_tls_conn_new_async: In progress", host, port);
            } else {
                ESP_TRANSPORT_LOGD_FUNC("TRANS_SSL_CONNECTING[%s:%d] esp_tls_conn_new_async: Connected", host, port);
            }
        } else {
            esp_tls_error_handle_t esp_tls_error_handle;
            esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle);
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(esp_tls_error_handle->esp_tls_error_code);
            ESP_TRANSPORT_LOGE_FUNC("[%s:%d] esp_tls_conn_new_async: Failed, res=%d, last_error=%d, esp_tls_error_code=-0x%04X(%d) (%s)",
                host, port, progress, esp_tls_error_handle->last_error, -esp_tls_error_handle->esp_tls_error_code,
                esp_tls_error_handle->esp_tls_error_code, (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
            ESP_LOGD(TAG, "%s: esp_transport_set_errors", __func__);
            esp_transport_set_errors(t, esp_tls_error_handle);
        }
        return progress;

    }
    return 0;
}

static int ssl_connect_async(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    const int ret = esp_tls_connect_async(t, host, port, timeout_ms, false);
    if (0 != ret) {
        // Unlock the saved session if connection failed or successful handshake
        transport_esp_tls_t *ssl = ssl_get_context_data(t);
        ESP_TRANSPORT_LOGD_FUNC("[%s] unlock_saved_session", host);
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
        unlock_saved_session(ssl);
        log_heap_info();
        esp_tls_error_handle_t esp_tls_error_handle;
        esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle);
        esp_transport_set_errors(t, esp_tls_error_handle);
        goto exit_failure;
    }
    ESP_TRANSPORT_LOGD_FUNC("[%s] unlock_saved_session", host);
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

    ESP_TRANSPORT_LOGD_FUNC("tls=%p, timeout=%d ms", ssl->tls, timeout_ms);

    if (ssl->tls) {
        remain = esp_tls_get_bytes_avail(ssl->tls);
        if (remain > 0) {
            ESP_LOGD(TAG, "remain data in cache, need to read again");
            return remain;
        }
    }
    ESP_LOGD(TAG, "%s: select timeout=%d ms", __func__, timeout_ms);
    ret = select(ssl->sockfd + 1, &readset, NULL, &errset, esp_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    ESP_LOGD(TAG, "%s: select, ret=%d", __func__, ret);
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
        errno = sock_errno;
        ret = -1;
    } else if (ret == 0) {
        ESP_LOGD(TAG, "poll_write: select - Timeout before any socket was ready!");
    }
    return ret;
}

static int ssl_write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] len=%d, non_block=%d, timeout=%d ms",
                            ssl->sockfd, esp_tls_get_hostname(ssl->tls), len, ssl->cfg.non_block, timeout_ms);
    if (!ssl->cfg.non_block) {
        ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] esp_transport_poll_write: timeout=%d ms",
                                ssl->sockfd, esp_tls_get_hostname(ssl->tls), timeout_ms);
        const int flag_poll = esp_transport_poll_write(t, timeout_ms);
        if (flag_poll <= 0) {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
            ESP_TRANSPORT_LOGE_FUNC(
                "[sock=%d][%s] Poll timeout or error, errno=%d (%s), timeout_ms=%d",
                ssl->sockfd,
                esp_tls_get_hostname(ssl->tls),
                errno,
                (NULL != err_desc.buf) ? err_desc.buf : "",
                timeout_ms);
            str_buf_free_buf(&err_desc);
            return flag_poll;
        }
    } else {
        if (!ssl->flag_timer_started) {
            ssl->timer_start = xTaskGetTickCount();
            ssl->flag_timer_started = true;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] start timer: %u",
                                    ssl->sockfd, esp_tls_get_hostname(ssl->tls), ssl->timer_start);
        }
    }
    int ret = -1;
    while (1)
    {
        ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] esp_tls_conn_write: %d bytes",
                                ssl->sockfd, esp_tls_get_hostname(ssl->tls), len);
        ret = esp_tls_conn_write(ssl->tls, (const unsigned char*)buffer, len);
        if (ret < 0)
        {
            if (ret == ESP_TLS_ERR_SSL_WANT_WRITE || ret == ESP_TLS_ERR_SSL_TIMEOUT) {
                if (!ssl->cfg.non_block) {
                    if (ret == ESP_TLS_ERR_SSL_WANT_WRITE) {
                        ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] esp_tls_conn_write error, ret=-0x%x (ESP_TLS_ERR_SSL_WANT_READ)",
                                                ssl->sockfd, esp_tls_get_hostname(ssl->tls), -ret);
                        continue;
                    }
                    ESP_TRANSPORT_LOGE_FUNC("[sock=%d][%s] esp_tls_conn_write error, ret=-0x%x (ESP_TLS_ERR_SSL_TIMEOUT)",
                                            ssl->sockfd, esp_tls_get_hostname(ssl->tls), -ret);
                    errno = ETIMEDOUT;
                    return ret;
                }
                const uint32_t delta_ticks = xTaskGetTickCount() - ssl->timer_start;
                ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] timer delta_ticks: %u, timeout: %d",
                                        ssl->sockfd, esp_tls_get_hostname(ssl->tls), delta_ticks, pdMS_TO_TICKS(timeout_ms));
                if (delta_ticks > pdMS_TO_TICKS(timeout_ms)) {
                    ESP_TRANSPORT_LOGE_FUNC("[sock=%d][%s] timeout: delta_ticks=%u, timeout=%d",
                                            ssl->sockfd, esp_tls_get_hostname(ssl->tls), delta_ticks, pdMS_TO_TICKS(timeout_ms));
                    errno = ETIMEDOUT;
                    ret = ETIMEDOUT;
                } else {
                    str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(ret);
                    ESP_TRANSPORT_LOGD_FUNC(
                        "[sock=%d][%s] esp_tls_conn_write error, ret=-0x%x (%s)",
                        ssl->sockfd, esp_tls_get_hostname(ssl->tls), -ret, (NULL != err_desc.buf) ? err_desc.buf : "");
                    str_buf_free_buf(&err_desc);
                }
            } else {
                str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(ret);
                ESP_TRANSPORT_LOGE_FUNC(
                    "[sock=%d][%s] esp_tls_conn_write error, ret=-0x%x (%s)",
                    ssl->sockfd, esp_tls_get_hostname(ssl->tls), -ret, (NULL != err_desc.buf) ? err_desc.buf : "");
                str_buf_free_buf(&err_desc);
                esp_tls_error_handle_t esp_tls_error_handle;
                if (esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle) == ESP_OK) {
                    esp_transport_set_errors(t, esp_tls_error_handle);
                } else {
                    ESP_TRANSPORT_LOGE_FUNC(
                        "[sock=%d][%s] Error in obtaining the error handle",
                        ssl->sockfd, esp_tls_get_hostname(ssl->tls));
                }
            }
        } else {
            if (ret == len) {
                ssl->flag_timer_started = false;
                ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] stop timer", ssl->sockfd, esp_tls_get_hostname(ssl->tls));
            } else {
                ssl->timer_start = xTaskGetTickCount();
                ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] restart timer: %u",
                                        ssl->sockfd, esp_tls_get_hostname(ssl->tls), ssl->timer_start);
            }
        }
        break;
    }
    ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] esp_tls_conn_write: %d bytes were written",
                            ssl->sockfd, esp_tls_get_hostname(ssl->tls), len);
    return ret;
}

static int tcp_write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    ESP_TRANSPORT_LOGD_FUNC("[sock=%d] len=%d, non_block=%d, timeout=%d ms", ssl->sockfd, len, ssl->cfg.non_block, timeout_ms);
    if (!ssl->cfg.non_block) {
        ESP_TRANSPORT_LOGD_FUNC("[sock=%d] esp_transport_poll_write: timeout=%d ms", ssl->sockfd, timeout_ms);
        const int poll = esp_transport_poll_write(t, timeout_ms);
        if (poll <= 0) {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
            ESP_TRANSPORT_LOGE_FUNC("[sock=%d] Poll timeout or error, errno=%d (%s), timeout_ms=%d",
                                    ssl->sockfd, errno, (NULL != err_desc.buf) ? err_desc.buf : "", timeout_ms);
            str_buf_free_buf(&err_desc);
            return poll;
        }
    } else {
        if (!ssl->flag_timer_started) {
            ssl->timer_start = xTaskGetTickCount();
            ssl->flag_timer_started = true;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] start timer: %u", ssl->sockfd, ssl->timer_start);
        }
    }
    ESP_TRANSPORT_LOGD_FUNC("[sock=%d] send: len=%d", ssl->sockfd, len);
    int ret = send(ssl->sockfd, (const unsigned char *) buffer, len, 0);
    ESP_TRANSPORT_LOGD_FUNC("[sock=%d] send: ret=%d", ssl->sockfd, len);
    if (ret < 0) {
        if (errno == EAGAIN) {
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] tcp_write error, errno=%d (EAGAIN)", ssl->sockfd, errno);
            const uint32_t delta_ticks = xTaskGetTickCount() - ssl->timer_start;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] timer delta_ticks: %u, timeout: %d", ssl->sockfd, delta_ticks, pdMS_TO_TICKS(timeout_ms));
            if (delta_ticks > pdMS_TO_TICKS(timeout_ms))
            {
                ESP_TRANSPORT_LOGE_FUNC("[sock=%d] timeout: delta_ticks=%u, timeout=%d",
                                        ssl->sockfd, delta_ticks, pdMS_TO_TICKS(timeout_ms));
                errno = ETIMEDOUT;
            }
        } else {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
            ESP_TRANSPORT_LOGE_FUNC("[sock=%d] tcp_write error, errno=%d (%s)",
                                    ssl->sockfd, errno, (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
        }
        esp_transport_capture_errno(t, errno);
    } else {
        if (ret == len) {
            ssl->flag_timer_started = false;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] stop timer", ssl->sockfd);
        } else {
            ssl->timer_start = xTaskGetTickCount();
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] restart timer: %u", ssl->sockfd, ssl->timer_start);
        }
    }
    return ret;
}

static int ssl_read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] non_block=%d, timeout=%d ms",
                            ssl->sockfd, esp_tls_get_hostname(ssl->tls), ssl->cfg.non_block, timeout_ms);
    if (!ssl->cfg.non_block) {
        ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] esp_transport_poll_read: timeout=%d ms",
                                ssl->sockfd, esp_tls_get_hostname(ssl->tls), timeout_ms);
        const int flag_poll = esp_transport_poll_read(t, timeout_ms);
        if (flag_poll == -1) {
            ESP_TRANSPORT_LOGE_FUNC("[sock=%d][%s] esp_transport_poll_read: ERR_TCP_TRANSPORT_CONNECTION_FAILED",
                                    ssl->sockfd, esp_tls_get_hostname(ssl->tls));
            return ERR_TCP_TRANSPORT_CONNECTION_FAILED;
        }
        if (flag_poll == 0) {
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] esp_transport_poll_read: ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT",
                                    ssl->sockfd, esp_tls_get_hostname(ssl->tls));
            return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
        }
    } else {
        if (!ssl->flag_timer_started) {
            ssl->timer_start = xTaskGetTickCount();
            ssl->flag_timer_started = true;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] start timer: %u",
                                    ssl->sockfd, esp_tls_get_hostname(ssl->tls), ssl->timer_start);
        }
    }

    int ret = -1;
    while (1) {
        ret = esp_tls_conn_read(ssl->tls, (unsigned char*)buffer, len);
        if (ret < 0) {
            if (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
                ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
                log_heap_info();
                save_new_session_ticket(ssl);
                log_heap_info();
                continue;
            }

            if (ret == ESP_TLS_ERR_SSL_WANT_READ || ret == ESP_TLS_ERR_SSL_TIMEOUT) {
                if (!ssl->cfg.non_block) {
                    if (ret == ESP_TLS_ERR_SSL_WANT_READ) {
                        ESP_TRANSPORT_LOGW_FUNC(
                            "[sock=%d][%s] esp_tls_conn_read error - no data available, ret=-0x%x (ESP_TLS_ERR_SSL_WANT_READ)",
                            ssl->sockfd, esp_tls_get_hostname(ssl->tls), -ret);
                        continue;
                    }
                    ESP_TRANSPORT_LOGE_FUNC(
                        "[sock=%d][%s] esp_tls_conn_read error - no data available, ret=-0x%x (ESP_TLS_ERR_SSL_TIMEOUT)",
                        ssl->sockfd, esp_tls_get_hostname(ssl->tls), -ret);
                    errno = ETIMEDOUT;
                    return ret;
                }
                const uint32_t delta_ticks = xTaskGetTickCount() - ssl->timer_start;
                ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] timer delta_ticks: %u, timeout: %d",
                                        ssl->sockfd, esp_tls_get_hostname(ssl->tls), delta_ticks, pdMS_TO_TICKS(timeout_ms));
                if (delta_ticks > pdMS_TO_TICKS(timeout_ms))
                {
                    ESP_TRANSPORT_LOGE_FUNC("[sock=%d][%s] timeout: delta_ticks=%u, timeout=%d",
                                            ssl->sockfd, esp_tls_get_hostname(ssl->tls), delta_ticks, pdMS_TO_TICKS(timeout_ms));
                    errno = ETIMEDOUT;
                    ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
                } else {
                    str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(ret);
                    ESP_TRANSPORT_LOGD_FUNC(
                        "[sock=%d][%s] esp_tls_conn_read error - no data available, ret=-0x%x (%s)",
                        ssl->sockfd, esp_tls_get_hostname(ssl->tls), -ret, (NULL != err_desc.buf) ? err_desc.buf : "");
                    str_buf_free_buf(&err_desc);
                    ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
                }
            } else {
                str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(ret);
                ESP_TRANSPORT_LOGE_FUNC(
                    "[sock=%d][%s] esp_tls_conn_read error, ret=-0x%x (%s)",
                    ssl->sockfd, esp_tls_get_hostname(ssl->tls), -ret, (NULL != err_desc.buf) ? err_desc.buf : "");
                str_buf_free_buf(&err_desc);
            }

            esp_tls_error_handle_t esp_tls_error_handle;
            if (esp_tls_get_error_handle(ssl->tls, &esp_tls_error_handle) == ESP_OK) {
                esp_transport_set_errors(t, esp_tls_error_handle);
            } else {
                ESP_TRANSPORT_LOGE_FUNC("[sock=%d][%s] Error in obtaining the error handle",
                                        ssl->sockfd, esp_tls_get_hostname(ssl->tls));
            }
        } else if (ret == 0) {
            // no error, socket reads 0 while previously detected as readable -> connection has been closed cleanly
            capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN);
            ret = ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s]  CONNECTION_CLOSED_BY_FIN",
                                    ssl->sockfd, esp_tls_get_hostname(ssl->tls));
        } else {
            if (ret == len) {
                ssl->flag_timer_started = false;
                ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] stop timer", ssl->sockfd, esp_tls_get_hostname(ssl->tls));
            } else {
                ssl->timer_start = xTaskGetTickCount();
                ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] restart timer: %u",
                                        ssl->sockfd, esp_tls_get_hostname(ssl->tls), ssl->timer_start);
            }
        }
        break;
    }
    return ret;
}

static int tcp_read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    transport_esp_tls_t *ssl = ssl_get_context_data(t);

    ESP_TRANSPORT_LOGD_FUNC("[sock=%d] non_block=%d, timeout=%d ms", ssl->sockfd, ssl->cfg.non_block, timeout_ms);
    if (!ssl->cfg.non_block) {
        ESP_TRANSPORT_LOGD_FUNC("[sock=%d] esp_transport_poll_read: timeout=%d ms", ssl->sockfd, timeout_ms);
        int flag_poll = esp_transport_poll_read(t, timeout_ms);
        if (flag_poll == -1) {
            ESP_TRANSPORT_LOGE_FUNC("[sock=%d] esp_transport_poll_read: ERR_TCP_TRANSPORT_CONNECTION_FAILED", ssl->sockfd);
            return ERR_TCP_TRANSPORT_CONNECTION_FAILED;
        }
        if (flag_poll == 0) {
            ESP_TRANSPORT_LOGD_FUNC("[%s] esp_transport_poll_read: ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT",
                                    esp_tls_get_hostname(ssl->tls));
            return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
        }
    } else {
        if (!ssl->flag_timer_started) {
            ssl->timer_start = xTaskGetTickCount();
            ssl->flag_timer_started = true;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] start timer: %u", ssl->sockfd, ssl->timer_start);
        }
    }

    ESP_TRANSPORT_LOGD_FUNC("[sock=%d] recv: len=%d", ssl->sockfd, len);
    int ret = recv(ssl->sockfd, (unsigned char *)buffer, len, 0);
    ESP_TRANSPORT_LOGD_FUNC("[sock=%d] recv: ret=%d", ssl->sockfd, ret);
    if (ret < 0) {
        if (errno == EAGAIN) {
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] tcp_read error, errno=%d (EAGAIN)", ssl->sockfd, errno);
            const uint32_t delta_ticks = xTaskGetTickCount() - ssl->timer_start;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] timer delta_ticks: %u, timeout: %d", ssl->sockfd, delta_ticks, pdMS_TO_TICKS(timeout_ms));
            if (delta_ticks > pdMS_TO_TICKS(timeout_ms))
            {
                ESP_TRANSPORT_LOGE_FUNC("[sock=%d] timeout: delta_ticks=%u, timeout=%d",
                                        ssl->sockfd, delta_ticks, pdMS_TO_TICKS(timeout_ms));
                errno = ETIMEDOUT;
            }
            ret = ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
        } else {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
            ESP_TRANSPORT_LOGE_FUNC("[sock=%d] tcp_read error, errno=%d (%s)",
                                    ssl->sockfd, errno, (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
            ret = ERR_TCP_TRANSPORT_CONNECTION_FAILED;
        }
        esp_transport_capture_errno(t, errno);
    } else if (ret == 0) {
        // no error, socket reads 0 while previously detected as readable -> connection has been closed cleanly
        capture_tcp_transport_error(t, ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN);
        ret = ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN;
        ESP_TRANSPORT_LOGD_FUNC("[sock=%d] CONNECTION_CLOSED_BY_FIN", ssl->sockfd);
    } else {
        if (ret == len) {
            ssl->flag_timer_started = false;
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] stop timer", ssl->sockfd);
        } else {
            ssl->timer_start = xTaskGetTickCount();
            ESP_TRANSPORT_LOGD_FUNC("[sock=%d] restart timer: %u", ssl->sockfd, ssl->timer_start);
        }
    }
    return ret;
}

static int base_close(esp_transport_handle_t t)
{
    int ret = -1;
    transport_esp_tls_t *ssl = ssl_get_context_data(t);
    ESP_TRANSPORT_LOGD_FUNC("ssl=%p", ssl);
    if (ssl && ssl->ssl_initialized) {
        ESP_TRANSPORT_LOGD_FUNC("tls=%p, host=%s", ssl->tls, esp_tls_get_hostname(ssl->tls));
        if (NULL == ssl->tls)
        {
            ESP_TRANSPORT_LOGW_FUNC("[%s] tls=NULL", esp_tls_get_hostname(ssl->tls));
            ret = 0;
        }
        else
        {
            ESP_TRANSPORT_LOGD_FUNC("[%s] tls=%p", esp_tls_get_hostname(ssl->tls), ssl->tls);
            ret = esp_tls_conn_destroy(ssl->tls);
        }
        ssl->tls = NULL;
        ssl->conn_state = TRANS_SSL_INIT;
        ssl->ssl_initialized = false;
        ssl->sockfd = INVALID_SOCKET;
    } else if (ssl && ssl->sockfd >= 0) {
        ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] tls=%p", ssl->sockfd, esp_tls_get_hostname(ssl->tls), ssl->tls);
        ESP_TRANSPORT_LOGD_FUNC("[sock=%d][%s] Close socket", ssl->sockfd, esp_tls_get_hostname(ssl->tls));
        ret = close(ssl->sockfd);
        ssl->sockfd = INVALID_SOCKET;
    } else {
        ESP_TRANSPORT_LOGD_FUNC("[%s] tls=%p", esp_tls_get_hostname(ssl->tls), ssl->tls);
        ESP_TRANSPORT_LOGD_FUNC("[%s] ssl->sockfd=%d", esp_tls_get_hostname(ssl->tls), ssl->sockfd);
    }
    return ret;
}

static int base_destroy(esp_transport_handle_t transport)
{
    const transport_esp_tls_t * const ssl = ssl_get_context_data(transport);
    if (ssl) {
        esp_transport_close(transport);
        esp_transport_destroy_foundation_transport(transport->foundation);

        if (NULL != transport->data) {
            free(transport->data);
            transport->data = NULL;
        }
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
    /* Treat NULL as "clear" / fallback to hostname as per API docs. */
    if (common_name == NULL) {
        ssl->cfg.common_name = NULL;
        return;
    }
    if (strlen(common_name) > MBEDTLS_SSL_MAX_HOST_NAME_LEN) {
        ESP_TRANSPORT_LOGE_FUNC("The length of common_name '%s' exceeds the maximum limit of %u characters.",
                                common_name,
                                MBEDTLS_SSL_MAX_HOST_NAME_LEN);
        return;
    }
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

#if defined(CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
void esp_transport_ssl_set_buffer_size(esp_transport_handle_t t,
                                       const size_t ssl_in_content_len,
                                       const size_t ssl_out_content_len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.ssl_in_content_len = (0 != ssl_in_content_len) ? ssl_in_content_len : CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN;
    ssl->cfg.ssl_out_content_len = (0 != ssl_out_content_len) ? ssl_out_content_len : CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN;
    ESP_TRANSPORT_LOGI("[%s] Configure size of TLS I/O buffers: in_content_len=%u, out_content_len=%u",
                       esp_tls_get_hostname(ssl->tls),
                       (unsigned)ssl->cfg.ssl_in_content_len,
                       (unsigned)ssl->cfg.ssl_out_content_len);
}
#endif

void esp_transport_ssl_set_buffer(esp_transport_handle_t t,
                                  uint8_t *const p_ssl_in_buf,
                                  uint8_t *const p_ssl_out_buf)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.p_ssl_in_buf = p_ssl_in_buf;
    ssl->cfg.p_ssl_out_buf = p_ssl_out_buf;
    ESP_TRANSPORT_LOGD("[%s] Configure TLS I/O buffers: in_buf=%p, out_buf=%p",
                       esp_tls_get_hostname(ssl->tls),
                       ssl->cfg.p_ssl_in_buf,
                       ssl->cfg.p_ssl_out_buf);
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
