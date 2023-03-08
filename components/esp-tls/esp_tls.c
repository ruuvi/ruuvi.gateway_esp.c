// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "lwip/dns.h"

#include <http_parser.h>
#include "esp_tls.h"
#include "esp_tls_error_capture_internal.h"
#include <errno.h>
static const char *TAG = "esp-tls";

#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
#include "esp_tls_mbedtls.h"
#elif CONFIG_ESP_TLS_USING_WOLFSSL
#include "esp_tls_wolfssl.h"
#endif

#define LOG_LOCAL_LEVEL 3
#ifdef ESP_PLATFORM
#include <esp_log.h>
#else
#define ESP_LOGD(TAG, ...) //printf(__VA_ARGS__);
#define ESP_LOGE(TAG, ...) printf(__VA_ARGS__);
#endif

#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
#define _esp_create_ssl_handle              esp_create_mbedtls_handle
#define _esp_tls_handshake                  esp_mbedtls_handshake
#define _esp_tls_read                       esp_mbedtls_read
#define _esp_tls_write                      esp_mbedtls_write
#define _esp_tls_conn_delete                esp_mbedtls_conn_delete
#ifdef CONFIG_ESP_TLS_SERVER
#define _esp_tls_server_session_create      esp_mbedtls_server_session_create
#define _esp_tls_server_session_delete      esp_mbedtls_server_session_delete
#endif  /* CONFIG_ESP_TLS_SERVER */
#define _esp_tls_get_bytes_avail            esp_mbedtls_get_bytes_avail
#define _esp_tls_init_global_ca_store       esp_mbedtls_init_global_ca_store
#define _esp_tls_set_global_ca_store        esp_mbedtls_set_global_ca_store                 /*!< Callback function for setting global CA store data for TLS/SSL */
#define _esp_tls_get_global_ca_store        esp_mbedtls_get_global_ca_store
#define _esp_tls_free_global_ca_store       esp_mbedtls_free_global_ca_store                /*!< Callback function for freeing global ca store for TLS/SSL */
#elif CONFIG_ESP_TLS_USING_WOLFSSL /* CONFIG_ESP_TLS_USING_MBEDTLS */
#define _esp_create_ssl_handle              esp_create_wolfssl_handle
#define _esp_tls_handshake                  esp_wolfssl_handshake
#define _esp_tls_read                       esp_wolfssl_read
#define _esp_tls_write                      esp_wolfssl_write
#define _esp_tls_conn_delete                esp_wolfssl_conn_delete
#ifdef CONFIG_ESP_TLS_SERVER
#define _esp_tls_server_session_create      esp_wolfssl_server_session_create
#define _esp_tls_server_session_delete      esp_wolfssl_server_session_delete
#endif  /* CONFIG_ESP_TLS_SERVER */
#define _esp_tls_get_bytes_avail            esp_wolfssl_get_bytes_avail
#define _esp_tls_init_global_ca_store       esp_wolfssl_init_global_ca_store
#define _esp_tls_set_global_ca_store        esp_wolfssl_set_global_ca_store                 /*!< Callback function for setting global CA store data for TLS/SSL */
#define _esp_tls_free_global_ca_store       esp_wolfssl_free_global_ca_store                /*!< Callback function for freeing global ca store for TLS/SSL */
#else   /* ESP_TLS_USING_WOLFSSL */
#error "No TLS stack configured"
#endif

typedef struct err_desc_t
{
#define ERR_DESC_SIZE 80
    char buf[ERR_DESC_SIZE];
} err_desc_t;

static esp_err_t create_ssl_handle(const char *hostname, size_t hostlen, const void *cfg, esp_tls_t *tls)
{
    return _esp_create_ssl_handle(hostname, hostlen, cfg, tls);
}

static esp_err_t esp_tls_handshake(esp_tls_t *tls, const esp_tls_cfg_t *cfg)
{
    return _esp_tls_handshake(tls, cfg);
}

static ssize_t tcp_read(esp_tls_t *tls, char *data, size_t datalen)
{
    return recv(tls->sockfd, data, datalen, 0);
}

static ssize_t tcp_write(esp_tls_t *tls, const char *data, size_t datalen)
{
    return send(tls->sockfd, data, datalen, 0);
}

/**
 * @brief      Close the TLS connection and free any allocated resources.
 */
void esp_tls_conn_delete(esp_tls_t *tls)
{
    esp_tls_conn_destroy(tls);
}

static esp_tls_async_dns_req_info_t g_esp_tls_dns_req[DNS_TABLE_SIZE];

static esp_tls_async_dns_req_info_t* esp_tls_find_free_dns_req_info(void)
{
    for (uint32_t i = 0; i < sizeof(g_esp_tls_dns_req) / sizeof(*g_esp_tls_dns_req); ++i)
    {
        esp_tls_async_dns_req_info_t* const p_info = &g_esp_tls_dns_req[i];
        if (!p_info->flag_busy)
        {
            return p_info;
        }
    }
    return NULL;
}


int esp_tls_conn_destroy(esp_tls_t *tls)
{
    if (tls != NULL) {
        ESP_LOGI(
            TAG,
            "[%s] %s: tls=%p",
            pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
            __func__,
            tls);

        SemaphoreHandle_t p_dns_mutex = tls->dns_mutex;
        xSemaphoreTake(p_dns_mutex, portMAX_DELAY);
        tls->dns_mutex = NULL;

        if (ESP_TLS_HOSTNAME_RESOLVING == tls->conn_state) {
            // Unfortunately, LWIP doesn't allow to overwrite previous requests,
            // otherwise we could call dns_gethostbyname with NULL as callback ptr to undo the callback call.
            ESP_LOGW(
                TAG,
                "[%s] %s: conn_state is ESP_TLS_HOSTNAME_RESOLVING, abort hostname resolving",
                pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
                __func__);
            tls->p_async_dns_req_info->tls = NULL;
            xSemaphoreGive(p_dns_mutex);
        } else {
            tls->p_async_dns_req_info = NULL;
            xSemaphoreGive(p_dns_mutex);
            vSemaphoreDelete(p_dns_mutex);
        }

        int ret = 0;
        _esp_tls_conn_delete(tls);
        if (tls->sockfd >= 0) {
            ret = close(tls->sockfd);
        }
        esp_tls_internal_event_tracker_destroy(tls->error_handle);

        free(tls);
        return ret;
    }
    ESP_LOGW(TAG, "[%s] %s: tls=%p", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __func__, tls);
    return -1; // invalid argument
}

esp_tls_t *esp_tls_init(void)
{
    esp_tls_t *tls = (esp_tls_t *)calloc(1, sizeof(esp_tls_t));
    if (!tls) {
        return NULL;
    }
    tls->error_handle = esp_tls_internal_event_tracker_create();
    if (!tls->error_handle) {
        free(tls);
        return NULL;
    }
    tls->dns_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(
        TAG,
        "[%s] %s: tls=%p",
        pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
        __func__,
        tls);
    tls->dns_cb_status = false;
    tls->dns_cb_ready = false;
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
    tls->server_fd.fd = -1;
#endif
    tls->sockfd = -1;
    return tls;
}

static void ms_to_timeval(int timeout_ms, struct timeval *tv)
{
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (timeout_ms % 1000) * 1000;
}

static int esp_tls_tcp_enable_keep_alive(int fd, tls_keep_alive_cfg_t *cfg)
{
    int keep_alive_enable = 1;
    int keep_alive_idle = cfg->keep_alive_idle;
    int keep_alive_interval = cfg->keep_alive_interval;
    int keep_alive_count = cfg->keep_alive_count;

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

static esp_err_t esp_tcp_connect(const ip_addr_t* const p_remote_ip, int port, int *sockfd, const esp_tls_t *tls, const esp_tls_cfg_t *cfg)
{
    struct sockaddr_in sa4 = { 0 };
    inet_addr_from_ip4addr(&sa4.sin_addr, ip_2_ip4(p_remote_ip));
    sa4.sin_family = AF_INET;
    sa4.sin_len = sizeof(struct sockaddr_in);
    sa4.sin_port = lwip_htons((u16_t)port);

    esp_err_t ret = ESP_OK;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_SYSTEM, errno);
        ret = ESP_ERR_ESP_TLS_CANNOT_CREATE_SOCKET;
        goto err_freeaddr;
    }

#if CONFIG_LWIP_IPV6
#error not supported
#endif

    if (cfg) {
        if (cfg->timeout_ms >= 0) {
            struct timeval tv;
            ms_to_timeval(cfg->timeout_ms, &tv);
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            if (cfg->keep_alive_cfg && cfg->keep_alive_cfg->keep_alive_enable) {
                if (esp_tls_tcp_enable_keep_alive(fd, cfg->keep_alive_cfg) < 0) {
                    ESP_LOGE(TAG, "Error setting keep-alive");
                    goto err_freesocket;
                }
            }
        }
        if (cfg->non_block) {
            int flags = fcntl(fd, F_GETFL, 0);
            ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            if (ret < 0) {
                ESP_LOGE(TAG, "Failed to configure the socket as non-blocking (errno %d)", errno);
                goto err_freesocket;
            }
        }
    }

    ret = connect(fd, (struct sockaddr*)&sa4, sa4.sin_len);
    if (ret < 0 && !(errno == EINPROGRESS && cfg && cfg->non_block)) {

        err_desc_t err_desc;
        esp_err_to_name_r(errno, err_desc.buf, sizeof(err_desc.buf));
        ESP_LOGE(TAG, "Failed to connnect to host, error=%d (%s)", errno, err_desc.buf);
        ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_SYSTEM, errno);
        ret = ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST;
        goto err_freesocket;
    }

    *sockfd = fd;
    return ESP_OK;

err_freesocket:
    close(fd);
err_freeaddr:
    return ret;
}

static void
esp_tls_low_level_conn_callback_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    esp_tls_async_dns_req_info_t * const p_req_info = arg;
    LWIP_UNUSED_ARG(hostname);

    if (NULL == p_req_info)
    {
        ESP_LOGE(TAG, "[%s] %s: arg is NULL", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __func__);
        return;
    }

    xSemaphoreTake(p_req_info->p_dns_mutex, portMAX_DELAY);
    p_req_info->flag_busy = false;
    if (NULL == p_req_info->tls)
    {
        // tls object was already destroyed by esp_tls_conn_destroy
        ESP_LOGW(
            TAG,
            "[%s] %s: callback is called for the tls object which was already destroyed",
            pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
            __func__);
        xSemaphoreGive(p_req_info->p_dns_mutex);
        vSemaphoreDelete(p_req_info->p_dns_mutex);
        p_req_info->p_dns_mutex = NULL;
        return;
    }

    if (ipaddr != NULL) {
        ESP_LOGI(
            TAG,
            "[%s] %s: resolved successfully",
            pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
            __func__);
        p_req_info->tls->dns_cb_remote_ip = *ipaddr;
        p_req_info->tls->dns_cb_status = true;
    } else {
        ESP_LOGE(
            TAG,
            "[%s] %s: resolved unsuccessfully, tls=%p, p_dns_mutex=%p",
            pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
            __func__,
            p_req_info->tls,
            p_req_info->p_dns_mutex);
        p_req_info->tls->dns_cb_remote_ip = (ip_addr_t){0};
        p_req_info->tls->dns_cb_status = false;
    }
    p_req_info->tls->dns_cb_ready = true;
    xSemaphoreGive(p_req_info->p_dns_mutex);
}

static int esp_tls_low_level_conn(const char *hostname, int hostlen, int port, const esp_tls_cfg_t *cfg, esp_tls_t *tls)
{
    if (!tls) {
        ESP_LOGE(TAG, "empty esp_tls parameter");
        return -1;
    }
    esp_err_t esp_ret;
    /* These states are used to keep a tab on connection progress in case of non-blocking connect,
    and in case of blocking connect these cases will get executed one after the other */
    switch (tls->conn_state) {
    case ESP_TLS_INIT:
    {
        xSemaphoreTake(tls->dns_mutex, portMAX_DELAY);
        esp_tls_async_dns_req_info_t* const p_dns_req_info = esp_tls_find_free_dns_req_info();
        if (NULL == p_dns_req_info)
        {
            xSemaphoreGive(tls->dns_mutex);
            ESP_LOGE(TAG, "Failed to resolve hostname '%s', there is no free slot for DNS query", hostname);
            return -1;
        }
        p_dns_req_info->tls = tls;
        p_dns_req_info->p_dns_mutex = tls->dns_mutex;
        p_dns_req_info->flag_busy = true;
        tls->p_async_dns_req_info = p_dns_req_info;
        xSemaphoreGive(tls->dns_mutex);

        tls->sockfd = -1;
        if (cfg != NULL) {
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
            mbedtls_net_init(&tls->server_fd);
#endif
            tls->is_tls = true;
        }
        err_t err = dns_gethostbyname(
            hostname,
            &tls->remote_ip,
            &esp_tls_low_level_conn_callback_dns_found, // This callback can be called after the connection is closed,
                                                        // at the moment when the tls object has already been destroyed,
                                                        // so we can't pass pointer to the tls object as an argument.
                                                        // Instead, we can pass a pointer to statically allocated memory.
            p_dns_req_info);
        if (err == ERR_INPROGRESS) {
            /* DNS request sent, wait for esp_tls_low_level_conn_callback_dns_found being called */
            ESP_LOGD(TAG, "dns_gethostbyname got ERR_INPROGRESS");
            tls->conn_state = ESP_TLS_HOSTNAME_RESOLVING;
            return 0; // Connection has not yet established
        }
        p_dns_req_info->flag_busy = false;
        p_dns_req_info->tls = NULL;
        p_dns_req_info->p_dns_mutex = NULL;

        if (err != ERR_OK) {
            ESP_LOGE(TAG, "Failed to resolve hostname '%s', error %d", hostname, err);
            return -1;
        }
        char remote_ip4_str[IP4ADDR_STRLEN_MAX];
        ip4addr_ntoa_r(&tls->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
        ESP_LOGI(TAG, "[%s] hostname '%s' resolved to %s", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", hostname, remote_ip4_str);

        tls->conn_state = ESP_TLS_CONNECT;
#if defined(__GNUC__) && (__GNUC__ >= 7)
        __attribute__((fallthrough));
#endif
    }
    /* falls through */
    case ESP_TLS_CONNECT:
    {
        char remote_ip4_str[IP4ADDR_STRLEN_MAX];
        ip4addr_ntoa_r(&tls->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
        ESP_LOGD(TAG, "Connect to IP: %s", remote_ip4_str);
        if ((esp_ret = esp_tcp_connect(&tls->remote_ip, port, &tls->sockfd, tls, cfg)) != ESP_OK) {
            err_desc_t err_desc;
            esp_err_to_name_r(esp_ret, err_desc.buf, sizeof(err_desc.buf));
            ESP_LOGE(TAG, "esp_tcp_connect failed, err=%d (%s)", esp_ret, err_desc.buf);
            ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, esp_ret);
            return -1;
        }
        if (!cfg) {
            tls->read = tcp_read;
            tls->write = tcp_write;
            ESP_LOGD(TAG, "non-tls connection established");
            return 1;
        }
        if (cfg->non_block) {
            tls->timer_start = xTaskGetTickCount();
            ESP_LOGD(TAG, "%s: ESP_TLS_INIT: start timer: %u", __func__, tls->timer_start);
        }
        tls->conn_state = ESP_TLS_CONNECTING;
#if defined(__GNUC__) && (__GNUC__ >= 7)
        __attribute__((fallthrough));
#endif
        /* falls through */
    }
    case ESP_TLS_CONNECTING:
        if (cfg->non_block) {
            ESP_LOGD(TAG, "connecting...");
            struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };

            FD_ZERO(&tls->rset);
            FD_SET(tls->sockfd, &tls->rset);
            tls->wset = tls->rset;

            /* In case of non-blocking I/O, we use the select() API to check whether
               connection has been established or not*/
            if (select(tls->sockfd + 1, &tls->rset, &tls->wset, NULL, &tv) < 0) {
                ESP_LOGD(TAG, "Non blocking connecting failed");
                tls->conn_state = ESP_TLS_FAIL;
                close(tls->sockfd);
                tls->sockfd = -1;
                return -1;
            }
            if (FD_ISSET(tls->sockfd, &tls->rset) || FD_ISSET(tls->sockfd, &tls->wset)) {
                int error;
                socklen_t len = sizeof(error);
                /* pending error check */
                if (getsockopt(tls->sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                    ESP_LOGD(TAG, "Non blocking connect failed");
                    ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_SYSTEM, errno);
                    ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED);
                    tls->conn_state = ESP_TLS_FAIL;
                    close(tls->sockfd);
                    tls->sockfd = -1;
                    return -1;
                }
            } else {
                const TickType_t now = xTaskGetTickCount();
                const uint32_t delta_ticks = now - tls->timer_start;
                ESP_LOGD(TAG, "%s: ESP_TLS_CONNECTING: timer delta_ticks: %u", __func__, delta_ticks);
                if (delta_ticks > pdMS_TO_TICKS(cfg->timeout_ms))
                {
                    ESP_LOGE(TAG, "connection timeout");
                    tls->conn_state = ESP_TLS_FAIL;
                    close(tls->sockfd);
                    tls->sockfd = -1;
                    return -1;
                }
                return 0; // Connection has not yet established
            }
        }
        /* By now, the connection has been established */
        esp_ret = create_ssl_handle(hostname, hostlen, cfg, tls);
        if (esp_ret != ESP_OK) {
            ESP_LOGE(TAG, "create_ssl_handle failed");
            ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, esp_ret);
            tls->conn_state = ESP_TLS_FAIL;
            close(tls->sockfd);
            tls->sockfd = -1;
            return -1;
        }
        tls->read = _esp_tls_read;
        tls->write = _esp_tls_write;
        tls->conn_state = ESP_TLS_HANDSHAKE;
        if (cfg->non_block) {
            tls->timer_start = xTaskGetTickCount();
            ESP_LOGD(TAG, "%s: ESP_TLS_CONNECTING: start timer: %u", __func__, tls->timer_start);
        }
#if defined(__GNUC__) && (__GNUC__ >= 7)
          __attribute__((fallthrough));
#endif
    /* falls through */
    case ESP_TLS_HANDSHAKE:
    {
        ESP_LOGD(TAG, "handshake in progress...");
        const int res = esp_tls_handshake(tls, cfg);
        if (res == 0) {
            const TickType_t now = xTaskGetTickCount();
            const uint32_t delta_ticks = now - tls->timer_start;
            ESP_LOGD(TAG, "%s: ESP_TLS_HANDSHAKE: timer delta_ticks: %u", __func__, delta_ticks);
            if (delta_ticks > pdMS_TO_TICKS(cfg->timeout_ms))
            {
                ESP_LOGE(TAG, "connection timeout");
                tls->conn_state = ESP_TLS_FAIL;
                // after create_ssl_handle we don't need to close the socket manually
                return -1;
            }
            return 0; // Connection has not yet established
        }
        return res;
    }
    case ESP_TLS_FAIL:
        ESP_LOGE(TAG, "failed to open a new connection");
        break;
    case ESP_TLS_HOSTNAME_RESOLVING:
    {
        ESP_LOGD(TAG, "%s: ESP_TLS_HOSTNAME_RESOLVING", __func__);
        xSemaphoreTake(tls->dns_mutex, portMAX_DELAY);
        if (tls->dns_cb_ready) {
            tls->dns_cb_ready = false;
            tls->remote_ip = tls->dns_cb_remote_ip;
            tls->conn_state = tls->dns_cb_status ? ESP_TLS_CONNECT : ESP_TLS_FAIL;
            if (tls->dns_cb_status) {
                char remote_ip4_str[IP4ADDR_STRLEN_MAX];
                ip4addr_ntoa_r(&tls->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
                ESP_LOGI(TAG, "[%s] hostname '%s' resolved to %s", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", hostname, remote_ip4_str);
            } else {
                ESP_LOGE(TAG, "[%s] hostname '%s' resolving failed", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", hostname);
                ESP_LOGD(TAG, "[%s] %s: tls->error_handle=%p", pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __func__, tls->error_handle);
            }
        }
        xSemaphoreGive(tls->dns_mutex);
        return 0;
    }
    default:
        ESP_LOGE(TAG, "invalid esp-tls state");
        break;
    }
    return -1;
}

/**
 * @brief      Create a new TLS/SSL connection
 */
esp_tls_t *esp_tls_conn_new(const char *hostname, int hostlen, int port, const esp_tls_cfg_t *cfg)
{
    esp_tls_t *tls = esp_tls_init();
    ESP_LOGD(TAG, "%s: esp_tls_init, tls=%p, tls->error_handle=%p", __func__, tls, tls->error_handle);
    if (!tls) {
        return NULL;
    }
    /* esp_tls_conn_new() API establishes connection in a blocking manner thus this loop ensures that esp_tls_conn_new()
       API returns only after connection is established unless there is an error*/
    size_t start = xTaskGetTickCount();
    while (1) {
        int ret = esp_tls_low_level_conn(hostname, hostlen, port, cfg, tls);
        if (ret == 1) {
            return tls;
        } else if (ret == -1) {
            esp_tls_conn_delete(tls);
            ESP_LOGE(TAG, "Failed to open new connection");
            return NULL;
        } else if (ret == 0 && cfg->timeout_ms >= 0) {
            size_t timeout_ticks = pdMS_TO_TICKS(cfg->timeout_ms);
            uint32_t expired = xTaskGetTickCount() - start;
            if (expired >= timeout_ticks) {
                esp_tls_conn_delete(tls);
                ESP_LOGE(TAG, "Failed to open new connection in specified timeout");
                return NULL;
            }
        }
    }
    return NULL;
}

int esp_tls_conn_new_sync(const char *hostname, int hostlen, int port, const esp_tls_cfg_t *cfg, esp_tls_t *tls)
{
    /* esp_tls_conn_new_sync() is a sync alternative to esp_tls_conn_new_async() with symmetric function prototype
    it is an alternative to esp_tls_conn_new() which is left for compatibility reasons */
    size_t start = xTaskGetTickCount();
    while (1) {
        int ret = esp_tls_low_level_conn(hostname, hostlen, port, cfg, tls);
        if (ret == 1) {
            return ret;
        } else if (ret == -1) {
            ESP_LOGE(TAG, "Failed to open new connection");
            return -1;
        } else if (ret == 0 && cfg->timeout_ms >= 0) {
            size_t timeout_ticks = pdMS_TO_TICKS(cfg->timeout_ms);
            uint32_t expired = xTaskGetTickCount() - start;
            if (expired >= timeout_ticks) {
                ESP_LOGW(
                    TAG,
                    "[%s] Failed to open new connection in specified timeout (%u ms)",
                    pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
                    cfg->timeout_ms);
                ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT);
                return 0;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    return 0;
}

/*
 * @brief      Create a new TLS/SSL non-blocking connection
 */
int esp_tls_conn_new_async(const char *hostname, int hostlen, int port, const esp_tls_cfg_t *cfg, esp_tls_t *tls)
{
    ESP_LOGD(TAG, "%s: esp_tls_low_level_conn", __func__);
    const int ret = esp_tls_low_level_conn(hostname, hostlen, port, cfg, tls);
    ESP_LOGD(TAG, "%s: esp_tls_low_level_conn, ret=%d", __func__, ret);
    return ret;
}

static int get_port(const char *url, struct http_parser_url *u)
{
    if (u->field_data[UF_PORT].len) {
        return strtol(&url[u->field_data[UF_PORT].off], NULL, 10);
    } else {
        if (strncasecmp(&url[u->field_data[UF_SCHEMA].off], "http", u->field_data[UF_SCHEMA].len) == 0) {
            return 80;
        } else if (strncasecmp(&url[u->field_data[UF_SCHEMA].off], "https", u->field_data[UF_SCHEMA].len) == 0) {
            return 443;
        }
    }
    return 0;
}

/**
 * @brief      Create a new TLS/SSL connection with a given "HTTP" url
 */
esp_tls_t *esp_tls_conn_http_new(const char *url, const esp_tls_cfg_t *cfg)
{
    /* Parse URI */
    struct http_parser_url u;
    http_parser_url_init(&u);
    http_parser_parse_url(url, strlen(url), 0, &u);
    esp_tls_t *tls = esp_tls_init();
    ESP_LOGD(TAG, "%s: esp_tls_init, tls=%p, tls->error_handle=%p", __func__, tls, tls->error_handle);
    if (!tls) {
        return NULL;
    }
    /* Connect to host */
    if (esp_tls_conn_new_sync(&url[u.field_data[UF_HOST].off], u.field_data[UF_HOST].len,
                              get_port(url, &u), cfg, tls) == 1) {
        return tls;
    }
    esp_tls_conn_delete(tls);
    return NULL;
}

/**
 * @brief      Create a new non-blocking TLS/SSL connection with a given "HTTP" url
 */
int esp_tls_conn_http_new_async(const char *url, const esp_tls_cfg_t *cfg, esp_tls_t *tls)
{
    /* Parse URI */
    struct http_parser_url u;
    http_parser_url_init(&u);
    http_parser_parse_url(url, strlen(url), 0, &u);

    /* Connect to host */
    return esp_tls_conn_new_async(&url[u.field_data[UF_HOST].off], u.field_data[UF_HOST].len,
                                  get_port(url, &u), cfg, tls);
}

#ifdef CONFIG_ESP_TLS_USING_MBEDTLS

mbedtls_x509_crt *esp_tls_get_global_ca_store(void)
{
    return _esp_tls_get_global_ca_store();
}

#endif /* CONFIG_ESP_TLS_USING_MBEDTLS */
#ifdef CONFIG_ESP_TLS_SERVER
/**
 * @brief      Create a server side TLS/SSL connection
 */
int esp_tls_server_session_create(esp_tls_cfg_server_t *cfg, int sockfd, esp_tls_t *tls)
{
    return _esp_tls_server_session_create(cfg, sockfd, tls);
}
/**
 * @brief      Close the server side TLS/SSL connection and free any allocated resources.
 */
void esp_tls_server_session_delete(esp_tls_t *tls)
{
    return _esp_tls_server_session_delete(tls);
}
#endif /* CONFIG_ESP_TLS_SERVER */

ssize_t esp_tls_get_bytes_avail(esp_tls_t *tls)
{
    return _esp_tls_get_bytes_avail(tls);
}

esp_err_t esp_tls_get_conn_sockfd(esp_tls_t *tls, int *sockfd)
{
    if (!tls || !sockfd) {
        ESP_LOGE(TAG, "Invalid arguments passed");
        return ESP_ERR_INVALID_ARG;
    }
    *sockfd = tls->sockfd;
    return ESP_OK;
}

esp_err_t esp_tls_get_and_clear_last_error(esp_tls_error_handle_t h, int *esp_tls_code, int *esp_tls_flags)
{
    if (!h) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t last_err = h->last_error;
    if (esp_tls_code) {
        *esp_tls_code = h->esp_tls_error_code;
    }
    if (esp_tls_flags) {
        *esp_tls_flags = h->esp_tls_flags;
    }
    memset(h, 0, sizeof(esp_tls_last_error_t));
    return last_err;
}

esp_err_t esp_tls_init_global_ca_store(void)
{
    return _esp_tls_init_global_ca_store();
}

esp_err_t esp_tls_set_global_ca_store(const unsigned char *cacert_pem_buf, const unsigned int cacert_pem_bytes)
{
    return _esp_tls_set_global_ca_store(cacert_pem_buf, cacert_pem_bytes);
}

void esp_tls_free_global_ca_store(void)
{
    return _esp_tls_free_global_ca_store();
}