// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <string.h>

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#define LOG_LOCAL_LEVEL 3
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp_transport_utils.h"
#include "esp_transport.h"
#include "esp_transport_internal.h"
#include "esp_transport_ssl_internal.h"
#include "esp_tls.h"

static const char *TAG = "TRANS_TCP";

typedef struct err_desc_t
{
#define ERR_DESC_SIZE 80
    char buf[ERR_DESC_SIZE];
} err_desc_t;

typedef enum {
    TRANS_TCP_INIT = 0,
    TRANS_TCP_CONNECT,
    TRANS_TCP_CONNECTING,
    TRANS_TCP_CONNECTED,
    TRANS_TCP_FAIL,
    TRANS_TCP_HOSTNAME_RESOLVING,
} transport_tcp_conn_state_t;

typedef struct {
    int sock;
    transport_tcp_conn_state_t conn_state;
    bool non_block; /*!< Configure non-blocking mode. If set to true the
                         underneath socket will be configured in non
                         blocking mode after tls session is established */
    fd_set rset;    /*!< read file descriptors */
    fd_set wset;    /*!< write file descriptors */
    TickType_t timer_start;
    int timer_read_initialized;
    int timer_write_initialized;
    ip_addr_t remote_ip;
    StaticSemaphore_t dns_mutex_mem;
    SemaphoreHandle_t dns_mutex;
    ip_addr_t dns_cb_remote_ip;
    bool dns_cb_status;
    bool dns_cb_ready;
} transport_tcp_t;

static int resolve_dns(const char *host, struct sockaddr_in *ip)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;

    int err = getaddrinfo(host, NULL, &hints, &res);
    if(err != 0 || res == NULL) {
        const char* p_err_desc = "Unknown";
        err_desc_t err_desc;
        switch (err)
        {
            case EAI_NONAME:
                p_err_desc = "EAI_NONAME";
                break;
            case EAI_SERVICE:
                p_err_desc = "EAI_SERVICE";
                break;
            case EAI_FAIL:
                p_err_desc = "EAI_FAIL";
                break;
            case EAI_MEMORY:
                p_err_desc = "EAI_MEMORY";
                break;
            case EAI_FAMILY:
                p_err_desc = "EAI_FAMILY";
                break;
            case HOST_NOT_FOUND:
                p_err_desc = "HOST_NOT_FOUND";
                break;
            case NO_DATA:
                p_err_desc = "NO_DATA";
                break;
            case NO_RECOVERY:
                p_err_desc = "NO_RECOVERY";
                break;
            case TRY_AGAIN:
                p_err_desc = "TRY_AGAIN";
                break;
            default:
                esp_err_to_name_r(err, err_desc.buf, sizeof(err_desc.buf));
                p_err_desc = err_desc.buf;
                break;
        }
        ESP_LOGE(TAG, "DNS lookup failed err=%d (%s) res=%p", err, p_err_desc, res);
        return ESP_FAIL;
    }
    ip->sin_family = AF_INET;
    memcpy(&ip->sin_addr, &((struct sockaddr_in *)(res->ai_addr))->sin_addr, sizeof(ip->sin_addr));
    freeaddrinfo(res);
    return ESP_OK;
}

static int tcp_enable_keep_alive(int fd, esp_transport_keep_alive_t *keep_alive_cfg)
{
    int keep_alive_enable = 1;
    int keep_alive_idle = keep_alive_cfg->keep_alive_idle;
    int keep_alive_interval = keep_alive_cfg->keep_alive_interval;
    int keep_alive_count = keep_alive_cfg->keep_alive_count;

    ESP_LOGD(TAG, "Enable TCP keep alive. idle: %d, interval: %d, count: %d", keep_alive_idle, keep_alive_interval, keep_alive_count);
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive_enable, sizeof(keep_alive_enable)) != 0) {
        ESP_LOGE(TAG, "Fail to setsockopt SO_KEEPALIVE");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_alive_idle, sizeof(keep_alive_idle)) != 0) {
        ESP_LOGE(TAG, "Fail to setsockopt TCP_KEEPIDLE");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval)) != 0) {
        ESP_LOGE(TAG, "Fail to setsockopt TCP_KEEPINTVL");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive_count, sizeof(keep_alive_count)) != 0) {
        ESP_LOGE(TAG, "Fail to setsockopt TCP_KEEPCNT");
        return -1;
    }

    return 0;
}

static int tcp_connect(esp_transport_handle_t t, const char *host_or_ip, int port, int timeout_ms)
{
    struct sockaddr_in remote_ip;
    struct timeval tv = { 0 };
    transport_tcp_t *tcp = esp_transport_get_context_data(t);

    ESP_LOGD(TAG, "tcp_connect: %s:%d, timeout=%d ms, non_block=%d", host_or_ip, port, timeout_ms, tcp->non_block);

    bzero(&remote_ip, sizeof(struct sockaddr_in));

    //if stream_host is not ip address, resolve it AF_INET,servername,&serveraddr.sin_addr
    if (inet_pton(AF_INET, host_or_ip, &remote_ip.sin_addr) != 1) {
        if (resolve_dns(host_or_ip, &remote_ip) < 0) {
            ESP_LOGE(TAG, "%s: failed to resolve hostname '%s'", __func__, host_or_ip);
            esp_tls_last_error_t last_err = {
                .last_error = ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME,
                .esp_tls_error_code = 0,
                .esp_tls_flags = 0,
            };
            esp_transport_set_errors(t, &last_err);
            return -1;
        }
    }

    tcp->sock = socket(PF_INET, SOCK_STREAM, 0);

    if (tcp->sock < 0) {
        ESP_LOGE(TAG, "Error create socket");
        return -1;
    }

    remote_ip.sin_family = AF_INET;
    remote_ip.sin_port = htons(port);

    esp_transport_utils_ms_to_timeval(timeout_ms, &tv); // if timeout=-1, tv is unchanged, 0, i.e. waits forever

    setsockopt(tcp->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(tcp->sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    // Set socket keep-alive option
    if (t->keep_alive_cfg && t->keep_alive_cfg->keep_alive_enable) {
        if (tcp_enable_keep_alive(tcp->sock, t->keep_alive_cfg) < 0) {
            ESP_LOGE(TAG, "Error to set tcp [socket=%d] keep-alive", tcp->sock);
            goto error;
        }
    }
    // Set socket to non-blocking
    int flags;
    if ((flags = fcntl(tcp->sock, F_GETFL, NULL)) < 0) {
        err_desc_t err_desc;
        esp_err_to_name_r(errno, err_desc.buf, sizeof(err_desc.buf));
        ESP_LOGE(TAG, "[sock=%d] get file flags error: %d (%s)", tcp->sock, errno, err_desc.buf);
        goto error;
    }
    if (fcntl(tcp->sock, F_SETFL, flags |= O_NONBLOCK) < 0) {
        err_desc_t err_desc;
        esp_err_to_name_r(errno, err_desc.buf, sizeof(err_desc.buf));
        ESP_LOGE(TAG, "[sock=%d] set nonblocking error: %d (%s)", tcp->sock, errno, err_desc.buf);
        goto error;
    }

    char remote_ip4_str[IP4ADDR_STRLEN_MAX];
    ip4addr_ntoa_r((const ip4_addr_t*)&remote_ip.sin_addr.s_addr, remote_ip4_str, sizeof(remote_ip4_str));
    ESP_LOGD(TAG, "[sock=%d] Connecting to server. IP: %s, Port: %d", tcp->sock, remote_ip4_str, port);

    if (connect(tcp->sock, (struct sockaddr *)(&remote_ip), sizeof(struct sockaddr)) < 0) {
        if (!tcp->non_block && (errno == EINPROGRESS)) {
            fd_set fdset;

            esp_transport_utils_ms_to_timeval(timeout_ms, &tv);
            FD_ZERO(&fdset);
            FD_SET(tcp->sock, &fdset);

            int res = select(tcp->sock+1, NULL, &fdset, NULL, &tv);
            if (res < 0) {
                err_desc_t err_desc;
                esp_err_to_name_r(errno, err_desc.buf, sizeof(err_desc.buf));
                ESP_LOGE(TAG, "[sock=%d] select() error: %d (%s)", tcp->sock, errno, err_desc.buf);
                esp_transport_capture_errno(t, errno);
                goto error;
            }
            else if (res == 0) {
                ESP_LOGE(TAG, "[sock=%d] select() timeout", tcp->sock);
                esp_transport_capture_errno(t, EINPROGRESS);    // errno=EINPROGRESS indicates connection timeout
                goto error;
            } else {
                int sockerr;
                socklen_t len = (socklen_t)sizeof(int);

                if (getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len) < 0) {
                    err_desc_t err_desc;
                    esp_err_to_name_r(errno, err_desc.buf, sizeof(err_desc.buf));
                    ESP_LOGE(TAG, "[sock=%d] getsockopt() error: %d (%s)", tcp->sock, errno, err_desc.buf);
                    goto error;
                }
                else if (sockerr) {
                    esp_transport_capture_errno(t, sockerr);
                    err_desc_t err_desc;
                    esp_err_to_name_r(sockerr, err_desc.buf, sizeof(err_desc.buf));
                    ESP_LOGE(TAG, "[sock=%d] delayed connect error: %d (%s)", tcp->sock, sockerr, err_desc.buf);
                    goto error;
                }
            }
        } else if (!tcp->non_block || (errno != EINPROGRESS)) {
            err_desc_t err_desc;
            esp_err_to_name_r(errno, err_desc.buf, sizeof(err_desc.buf));
            ESP_LOGE(TAG, "[sock=%d] connect() error: %d (%s)", tcp->sock, errno, err_desc.buf);
            goto error;
        }
    }
    if (!tcp->non_block) {
        // Reset socket to blocking
        if ((flags = fcntl(tcp->sock, F_GETFL, NULL)) < 0) {
            err_desc_t err_desc;
            esp_err_to_name_r(errno, err_desc.buf, sizeof(err_desc.buf));
            ESP_LOGE(TAG, "[sock=%d] get file flags error: %d (%s)", tcp->sock, errno, err_desc.buf);
            goto error;
        }
        if (fcntl(tcp->sock, F_SETFL, flags & ~O_NONBLOCK) < 0) {
            err_desc_t err_desc;
            esp_err_to_name_r(errno, err_desc.buf, sizeof(err_desc.buf));
            ESP_LOGE(TAG, "[sock=%d] reset blocking error: %d (%s)", tcp->sock, errno, err_desc.buf);
            goto error;
        }
    }
    return tcp->sock;
error:
    close(tcp->sock);
    tcp->sock = -1;
    return -1;
}

static void
esp_transport_tcp_connect_async_callback_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    transport_tcp_t *tcp = arg;
    LWIP_UNUSED_ARG(hostname);

    if (NULL == tcp)
    {
        ESP_LOGE(TAG, "[%s] %s: arg is NULL", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __func__);
        return;
    }

    xSemaphoreTake(tcp->dns_mutex, portMAX_DELAY);
    if (ipaddr != NULL) {
        ESP_LOGI(
            TAG,
            "[%s] %s: resolved successfully",
            pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
            __func__);
        tcp->dns_cb_remote_ip = *ipaddr;
        tcp->dns_cb_status = true;
    } else {
        ESP_LOGE(
            TAG,
            "[%s] %s: resolved unsuccessfully",
            pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
            __func__);
        tcp->dns_cb_remote_ip = (ip_addr_t){0};
        tcp->dns_cb_status = false;
    }
    tcp->dns_cb_ready = true;
    xSemaphoreGive(tcp->dns_mutex);
}

static int esp_transport_tcp_connect_async(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    /* These states are used to keep a tab on connection progress in case of non-blocking connect,
    and in case of blocking connect these cases will get executed one after the other */
    switch (tcp->conn_state) {
        case TRANS_TCP_INIT:
        {
            tcp->non_block = true;

            err_t err = dns_gethostbyname(
                host,
                &tcp->remote_ip,
                &esp_transport_tcp_connect_async_callback_dns_found,
                tcp);
            if (err == ERR_INPROGRESS) {
                /* DNS request sent, wait for esp_transport_tcp_connect_async_callback_dns_found being called */
                ESP_LOGD(TAG, "dns_gethostbyname got ERR_INPROGRESS");
                tcp->conn_state = TRANS_TCP_HOSTNAME_RESOLVING;
                return 0; // Connection has not yet established
            }
            if (err != ERR_OK) {
                ESP_LOGE(TAG, "Failed to resolve hostname '%s', error %d", host, err);
                esp_tls_last_error_t last_err = {
                    .last_error = ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME,
                    .esp_tls_error_code = 0,
                    .esp_tls_flags = 0,
                };
                esp_transport_set_errors(t, &last_err);
                return -1;
            }

            char remote_ip4_str[IP4ADDR_STRLEN_MAX];
            ip4addr_ntoa_r(&tcp->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
            ESP_LOGI(
                TAG,
                "[%s] hostname '%s' resolved to %s",
                pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
                host,
                remote_ip4_str);
            tcp->conn_state = TRANS_TCP_CONNECT;
#if defined(__GNUC__) && (__GNUC__ >= 7)
            __attribute__((fallthrough));
#endif
        }

        case TRANS_TCP_CONNECT:
        {
            char remote_ip4_str[IP4ADDR_STRLEN_MAX];
            ip4addr_ntoa_r(&tcp->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
            ESP_LOGD(TAG, "Connect to IP: %s", remote_ip4_str);
            tcp->sock = tcp_connect(t, remote_ip4_str, port, timeout_ms);
            if (tcp->sock < 0) {
                return -1;
            }
            tcp->timer_start = xTaskGetTickCount();
            tcp->conn_state = TRANS_TCP_CONNECTING;
            return 0; // Connection has not yet established
        }

        case TRANS_TCP_CONNECTING:
        {
            struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
            FD_ZERO(&tcp->rset);
            FD_SET(tcp->sock, &tcp->rset);
            tcp->wset = tcp->rset;
            if (select(tcp->sock + 1, &tcp->rset, &tcp->wset, NULL, &tv) < 0)
            {
                ESP_LOGD(TAG, "Non blocking connecting failed");
                tcp->conn_state = TRANS_TCP_FAIL;
                return -1;
            }

            if (FD_ISSET(tcp->sock, &tcp->rset) || FD_ISSET(tcp->sock, &tcp->wset))
            {
                int       error = 0;
                socklen_t len   = sizeof(error);
                /* pending error check */
                if (getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
                {
                    ESP_LOGD(TAG, "Non blocking connect failed");
                    tcp->conn_state = TRANS_TCP_FAIL;
                    return -1;
                }
            }
            else
            {
                const TickType_t now = xTaskGetTickCount();
                const uint32_t delta_ticks = now - tcp->timer_start;
                if (delta_ticks > pdMS_TO_TICKS(timeout_ms))
                {
                    ESP_LOGE(TAG, "connection timeout");
                    tcp->conn_state = TRANS_TCP_FAIL;
                    return -1;
                }
                return 0; // Connection has not yet established
            }
            /* By now, the connection has been established */
            tcp->conn_state = TRANS_TCP_CONNECTED;

#if defined(__GNUC__) && (__GNUC__ >= 7)
            __attribute__((fallthrough));
#endif
        }
        case TRANS_TCP_CONNECTED:
            ESP_LOGD(TAG, "%s: connected", __func__);
            return 1;
        case TRANS_TCP_FAIL:
            ESP_LOGE(TAG, "%s: failed to open a new connection", __func__);
            break;
        case TRANS_TCP_HOSTNAME_RESOLVING:
        {
            ESP_LOGD(TAG, "TRANS_TCP_HOSTNAME_RESOLVING");
            xSemaphoreTake(tcp->dns_mutex, portMAX_DELAY);
            if (tcp->dns_cb_ready) {
                tcp->dns_cb_ready = false;
                tcp->remote_ip = tcp->dns_cb_remote_ip;
                if (tcp->dns_cb_status)
                {
                    char remote_ip4_str[IP4ADDR_STRLEN_MAX];
                    ip4addr_ntoa_r(&tcp->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
                    ESP_LOGI(TAG, "[%s] hostname '%s' resolved to %s", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", host, remote_ip4_str);
                    tcp->conn_state = TRANS_TCP_CONNECT;
                } else {
                    ESP_LOGE(TAG, "[%s] hostname '%s' resolving failed", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", host);
                    tcp->conn_state = TRANS_TCP_FAIL;
                }
            }
            xSemaphoreGive(tcp->dns_mutex);
            if (TRANS_TCP_FAIL == tcp->conn_state) {
                ESP_LOGE(TAG, "%s: failed to open a new connection", __func__);
                esp_tls_last_error_t last_err = {
                    .last_error = ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME,
                    .esp_tls_error_code = 0,
                    .esp_tls_flags = 0,
                };
                esp_transport_set_errors(t, &last_err);
                break;
            }
            return 0;
        }
        default:
            ESP_LOGE(TAG, "%s: invalid TCP conn-state", __func__);
            break;
    }
    return -1;
}

static int tcp_write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    if (!tcp->non_block) {
        int poll;
        if ((poll = esp_transport_poll_write(t, timeout_ms)) <= 0) {
            return poll;
        }
    } else {
        tcp->timer_read_initialized = false;
        if (!tcp->timer_write_initialized) {
            ESP_LOGD(TAG, "%s: start timer", __func__);
            tcp->timer_start = xTaskGetTickCount();
            tcp->timer_write_initialized = true;
        }
    }
    const int wlen = write(tcp->sock, buffer, len);
    if (wlen < 0) {
        return -1;
    }
    if (tcp->non_block) {
        if (wlen > 0) {
            if (tcp->non_block) {
                ESP_LOGD(TAG, "%s: restart timer", __func__);
                tcp->timer_start = xTaskGetTickCount();
            }
        } else {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                const TickType_t delta_ticks = xTaskGetTickCount() - tcp->timer_start;
                if (delta_ticks > pdMS_TO_TICKS(timeout_ms)) {
                    ESP_LOGE(TAG, "%s: timeout", __func__);
                    errno = -1;
                    tcp->timer_write_initialized = false;
                    return -1;
                }
            } else {
                tcp->timer_write_initialized = false;
            }
        }
    }
    return wlen;
}

static int tcp_read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    if (!tcp->non_block) {
        int poll = -1;
        if ((poll = esp_transport_poll_read(t, timeout_ms)) <= 0) {
            return poll;
        }
    } else {
        tcp->timer_write_initialized = false;
        if (!tcp->timer_read_initialized) {
            ESP_LOGD(TAG, "%s: start timer", __func__);
            tcp->timer_start = xTaskGetTickCount();
            tcp->timer_read_initialized = true;
        }
    }
    int read_len = read(tcp->sock, buffer, len);
    if (!tcp->non_block)
    {
        if (read_len == 0) {
            return -1;
        }
    } else {
        if (read_len > 0) {
            if (tcp->non_block) {
                ESP_LOGD(TAG, "%s: restart timer", __func__);
                tcp->timer_start = xTaskGetTickCount();
            }
        } else {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                const TickType_t delta_ticks = xTaskGetTickCount() - tcp->timer_start;
                if (delta_ticks > pdMS_TO_TICKS(timeout_ms)) {
                    ESP_LOGE(TAG, "%s: timeout", __func__);
                    errno = -1;
                    tcp->timer_read_initialized = false;
                    return -1;
                }
            } else {
                tcp->timer_read_initialized = false;
            }
        }
    }
    return read_len;
}

static int tcp_poll_read(esp_transport_handle_t t, int timeout_ms)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    int ret = -1;
    struct timeval timeout;
    fd_set readset;
    fd_set errset;
    FD_ZERO(&readset);
    FD_ZERO(&errset);
    FD_SET(tcp->sock, &readset);
    FD_SET(tcp->sock, &errset);

    ret = select(tcp->sock + 1, &readset, NULL, &errset, esp_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(tcp->sock, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        esp_transport_capture_errno(t, sock_errno);
        err_desc_t err_desc;
        esp_err_to_name_r(sock_errno, err_desc.buf, sizeof(err_desc.buf));
        ESP_LOGE(TAG, "tcp_poll_read select error %d (%s), fd = %d", sock_errno, err_desc.buf, tcp->sock);
        ret = -1;
    }
    return ret;
}

static int tcp_poll_write(esp_transport_handle_t t, int timeout_ms)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    int ret = -1;
    struct timeval timeout;
    fd_set writeset;
    fd_set errset;
    FD_ZERO(&writeset);
    FD_ZERO(&errset);
    FD_SET(tcp->sock, &writeset);
    FD_SET(tcp->sock, &errset);

    ret = select(tcp->sock + 1, NULL, &writeset, &errset, esp_transport_utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(tcp->sock, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(tcp->sock, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        esp_transport_capture_errno(t, sock_errno);
        err_desc_t err_desc;
        esp_err_to_name_r(sock_errno, err_desc.buf, sizeof(err_desc.buf));
        ESP_LOGE(TAG, "tcp_poll_write select error %d (%s), fd = %d", sock_errno, err_desc.buf, tcp->sock);
        ret = -1;
    }
    return ret;
}

static int tcp_close(esp_transport_handle_t t)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    int ret = -1;
    if (tcp->sock >= 0) {
        ret = close(tcp->sock);
        tcp->sock = -1;
    }
    return ret;
}

static esp_err_t tcp_destroy(esp_transport_handle_t t)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    esp_transport_close(t);
    vSemaphoreDelete(tcp->dns_mutex);
    tcp->dns_mutex = NULL;
    free(tcp);
    return 0;
}

static int tcp_get_socket(esp_transport_handle_t t)
{
    if (t) {
        transport_tcp_t *tcp = t->data;
        if (tcp) {
            return tcp->sock;
        }
    }
    return -1;
}

void esp_transport_tcp_set_keep_alive(esp_transport_handle_t t, esp_transport_keep_alive_t *keep_alive_cfg)
{
    if (t && keep_alive_cfg) {
        t->keep_alive_cfg = keep_alive_cfg;
    }
}

esp_transport_handle_t esp_transport_tcp_init(void)
{
    ESP_LOGD(TAG, "%s", __func__);
    esp_transport_handle_t t = esp_transport_init();
    transport_tcp_t *tcp = calloc(1, sizeof(transport_tcp_t));
    ESP_TRANSPORT_MEM_CHECK(TAG, tcp, {
        esp_transport_destroy(t);
        return NULL;
    });

    tcp->sock = -1;
    tcp->non_block = false;
    tcp->dns_mutex = xSemaphoreCreateMutexStatic(&tcp->dns_mutex_mem);
    tcp->dns_cb_status = false;
    tcp->dns_cb_ready = false;
    esp_transport_set_func(t, tcp_connect, tcp_read, tcp_write, tcp_close, tcp_poll_read, tcp_poll_write, tcp_destroy);
    esp_transport_set_context_data(t, tcp);
    esp_transport_set_async_connect_func(t, &esp_transport_tcp_connect_async);
    t->_get_socket = tcp_get_socket;

    return t;
}
