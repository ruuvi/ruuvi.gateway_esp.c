/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <http_parser.h>
#include "sdkconfig.h"
#include "esp_tls.h"
#include "esp_tls_private.h"
#include "esp_tls_error_capture_internal.h"
#include <errno.h>
#include "lwip/dns.h"
#include "snprintf_with_esp_err_desc.h"

#if CONFIG_IDF_TARGET_LINUX
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <linux/if.h>
#include <sys/time.h>

typedef struct in_addr ip_addr_t;
typedef struct in6_addr ip6_addr_t;
#define ipaddr_ntoa(ipaddr)     inet_ntoa(*ipaddr)

static inline char *ip6addr_ntoa(const ip6_addr_t *addr)
{
  static char str[40];
  return (char *)inet_ntop(AF_INET6, addr->s6_addr, str, 40);
}

#endif

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

#define ESP_TLS_LOGE(fmt, ...) \
    ESP_LOGE(TAG, "[%s] " fmt, pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", ##__VA_ARGS__)

#define ESP_TLS_LOGE_FUNC(fmt, ...) \
    ESP_LOGE(TAG, "[%s] %s: " fmt, pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __func__, ##__VA_ARGS__)

#define ESP_TLS_LOGE_LINE(fmt, ...) \
    ESP_LOGE(TAG, "[%s] %s:%d: " fmt, pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __FILE__, __LINE__, ##__VA_ARGS__)

#define ESP_TLS_LOGW_FUNC(fmt, ...) \
    ESP_LOGW(TAG, "[%s] %s: " fmt, pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __func__, ##__VA_ARGS__)

#define ESP_TLS_LOGI(fmt, ...) \
    ESP_LOGI(TAG, "[%s] " fmt, pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", ##__VA_ARGS__)

#define ESP_TLS_LOGI_FUNC(fmt, ...) \
    ESP_LOGI(TAG, "[%s] %s: " fmt, pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __func__, ##__VA_ARGS__)

#define ESP_TLS_LOGD(fmt, ...) \
    ESP_LOGD(TAG, "[%s] " fmt, pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", ##__VA_ARGS__)

#define ESP_TLS_LOGD_FUNC(fmt, ...) \
    ESP_LOGD(TAG, "[%s] %s: " fmt, pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???", __func__, ##__VA_ARGS__)

#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
#define _esp_create_ssl_handle              esp_create_mbedtls_handle
#define _esp_tls_handshake                  esp_mbedtls_handshake
#define _esp_tls_read                       esp_mbedtls_read
#define _esp_tls_write                      esp_mbedtls_write
#define _esp_tls_conn_delete                esp_mbedtls_conn_delete
#define _esp_tls_net_init                   esp_mbedtls_net_init
#define _esp_tls_copy_client_session        esp_mbedtls_copy_client_session
#define _esp_tls_get_client_session         esp_mbedtls_get_client_session
#define _esp_tls_free_client_session        esp_mbedtls_free_client_session
#define _esp_tls_get_ssl_context            esp_mbedtls_get_ssl_context
#ifdef CONFIG_ESP_TLS_SERVER
#define _esp_tls_server_session_create      esp_mbedtls_server_session_create
#define _esp_tls_server_session_delete      esp_mbedtls_server_session_delete
#define _esp_tls_server_session_ticket_ctx_init    esp_mbedtls_server_session_ticket_ctx_init
#define _esp_tls_server_session_ticket_ctx_free    esp_mbedtls_server_session_ticket_ctx_free
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
#define _esp_tls_net_init                   esp_wolfssl_net_init
#ifdef CONFIG_ESP_TLS_SERVER
#define _esp_tls_server_session_create      esp_wolfssl_server_session_create
#define _esp_tls_server_session_delete      esp_wolfssl_server_session_delete
#endif  /* CONFIG_ESP_TLS_SERVER */
#define _esp_tls_get_bytes_avail            esp_wolfssl_get_bytes_avail
#define _esp_tls_init_global_ca_store       esp_wolfssl_init_global_ca_store
#define _esp_tls_set_global_ca_store        esp_wolfssl_set_global_ca_store                 /*!< Callback function for setting global CA store data for TLS/SSL */
#define _esp_tls_free_global_ca_store       esp_wolfssl_free_global_ca_store                /*!< Callback function for freeing global ca store for TLS/SSL */
#define _esp_tls_get_ssl_context            esp_wolfssl_get_ssl_context
#else   /* ESP_TLS_USING_WOLFSSL */
#error "No TLS stack configured"
#endif

#if CONFIG_IDF_TARGET_LINUX
#define IPV4_ENABLED    1
#define IPV6_ENABLED    1
#else   // CONFIG_IDF_TARGET_LINUX
#define IPV4_ENABLED    CONFIG_LWIP_IPV4
#define IPV6_ENABLED    CONFIG_LWIP_IPV6
#endif  // !CONFIG_IDF_TARGET_LINUX

#define ESP_TLS_DEFAULT_CONN_TIMEOUT  (10)  /*!< Default connection timeout in seconds */

static esp_tls_async_dns_req_info_t g_esp_tls_dns_req[DNS_TABLE_SIZE];

static esp_tls_async_dns_req_info_t* esp_tls_find_free_dns_req_info(void)
{
    for (uint32_t i = 0; i < sizeof(g_esp_tls_dns_req) / sizeof(*g_esp_tls_dns_req); ++i) {
        esp_tls_async_dns_req_info_t* const p_info = &g_esp_tls_dns_req[i];
        if (!p_info->flag_busy) {
            return p_info;
        }
    }
    return NULL;
}

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

ssize_t esp_tls_conn_read(esp_tls_t *tls, void  *data, size_t datalen)
{
    return tls->read(tls, (char *)data, datalen);

}

ssize_t esp_tls_conn_write(esp_tls_t *tls, const void  *data, size_t datalen)
{
    return tls->write(tls, (char *)data, datalen);

}

/**
 * @brief      Close the TLS connection and free any allocated resources.
 */
int esp_tls_conn_destroy(esp_tls_t *tls)
{
    if (tls != NULL) {
        ESP_TLS_LOGD_FUNC("[sock=%d][%s] tls=%p", tls->sockfd, esp_tls_get_hostname(tls), tls);

        SemaphoreHandle_t p_dns_mutex = tls->dns_mutex;
        if (p_dns_mutex) {
            xSemaphoreTake(p_dns_mutex, portMAX_DELAY);
            tls->dns_mutex = NULL;

            if (ESP_TLS_HOSTNAME_RESOLVING == tls->conn_state) {
                // Unfortunately, LWIP doesn't allow to overwrite previous requests,
                // otherwise we could call dns_gethostbyname with NULL as callback ptr to undo the callback call.
                ESP_TLS_LOGW_FUNC("[sock=%d][%s] conn_state is ESP_TLS_HOSTNAME_RESOLVING, abort hostname resolving",
                                  tls->sockfd, esp_tls_get_hostname(tls));
                tls->p_async_dns_req_info->tls = NULL;
                xSemaphoreGive(p_dns_mutex);
            } else {
                tls->p_async_dns_req_info = NULL;
                xSemaphoreGive(p_dns_mutex);
                vSemaphoreDelete(p_dns_mutex);
            }
        }

        int ret = 0;
        _esp_tls_conn_delete(tls);
        if (tls->sockfd >= 0) {
            ESP_TLS_LOGI_FUNC("[sock=%d][%s] Close socket", tls->sockfd, esp_tls_get_hostname(tls));
            ret = close(tls->sockfd);
            tls->sockfd = -1;
        }
        esp_tls_internal_event_tracker_destroy(tls->error_handle);
        if (tls->hostname) {
            ESP_TLS_LOGD_FUNC("Free memory for hostname: %s", tls->hostname);
            free(tls->hostname);
            tls->hostname = NULL;
        }
        free(tls);
        return ret;
    }
    ESP_TLS_LOGW_FUNC("tls=%p", tls);
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
    ESP_TLS_LOGI_FUNC("tls=%p", tls);
    tls->dns_cb_status = false;
    tls->dns_cb_ready = false;
    _esp_tls_net_init(tls);
    tls->sockfd = -1;
    return tls;
}

static void ms_to_timeval(int timeout_ms, struct timeval *tv)
{
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (timeout_ms % 1000) * 1000;
}

static esp_err_t esp_tls_set_socket_options(int fd, const esp_tls_cfg_t *cfg)
{
    if (cfg) {
        struct timeval tv = {};
        if (cfg->timeout_ms > 0) {
            ms_to_timeval(cfg->timeout_ms, &tv);
        } else {
            tv.tv_sec = ESP_TLS_DEFAULT_CONN_TIMEOUT;
            tv.tv_usec = 0;
        }
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
            ESP_TLS_LOGE_FUNC("[sock=%d] Fail to setsockopt SO_RCVTIMEO", fd);
            return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
        }
        if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
            ESP_TLS_LOGE_FUNC("[sock=%d] Fail to setsockopt SO_SNDTIMEO", fd);
            return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
        }

        if (cfg->keep_alive_cfg && cfg->keep_alive_cfg->keep_alive_enable) {
            int keep_alive_enable = 1;
            int keep_alive_idle = cfg->keep_alive_cfg->keep_alive_idle;
            int keep_alive_interval = cfg->keep_alive_cfg->keep_alive_interval;
            int keep_alive_count = cfg->keep_alive_cfg->keep_alive_count;

            ESP_LOGD(TAG, "Enable TCP keep alive. idle: %d, interval: %d, count: %d", keep_alive_idle, keep_alive_interval, keep_alive_count);
            if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive_enable, sizeof(keep_alive_enable)) != 0) {
                ESP_TLS_LOGE_FUNC("[sock=%d] Fail to setsockopt SO_KEEPALIVE", fd);
                return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_alive_idle, sizeof(keep_alive_idle)) != 0) {
                ESP_TLS_LOGE_FUNC("[sock=%d] Fail to setsockopt TCP_KEEPIDLE", fd);
                return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval)) != 0) {
                ESP_TLS_LOGE_FUNC("[sock=%d] Fail to setsockopt TCP_KEEPINTVL", fd);
                return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive_count, sizeof(keep_alive_count)) != 0) {
                ESP_TLS_LOGE_FUNC("[sock=%d] Fail to setsockopt TCP_KEEPCNT", fd);
                return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
            }
        }
        if (cfg->if_name) {
            if (cfg->if_name->ifr_name[0] != 0) {
                ESP_LOGD(TAG, "Bind [sock=%d] to interface %s", fd, cfg->if_name->ifr_name);
                if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,  cfg->if_name, sizeof(struct ifreq)) != 0) {
                    ESP_TLS_LOGE_FUNC("Bind [sock=%d] to interface %s fail", fd, cfg->if_name->ifr_name);
                    return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
                }
            }
        }
    }
    return ESP_OK;
}

static esp_err_t esp_tls_set_socket_non_blocking(int fd, bool non_blocking)
{
    int flags;
    if ((flags = fcntl(fd, F_GETFL, NULL)) < 0) {
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
        ESP_TLS_LOGE_FUNC("[sock=%d] get file flags error: %d (%s)", fd, errno, (NULL != err_desc.buf) ? err_desc.buf : "");
        str_buf_free_buf(&err_desc);
        return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
    }

    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, flags) < 0) {
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
        ESP_TLS_LOGE_FUNC("[sock=%d] set blocking/nonblocking error: %d (%s)",
                 fd, errno, (NULL != err_desc.buf) ? err_desc.buf : "");
        str_buf_free_buf(&err_desc);
        return ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
    }
    return ESP_OK;
}

static esp_err_t esp_tls_tcp_connect(const ip_addr_t* const p_remote_ip, int port, const esp_tls_cfg_t *cfg,
            esp_tls_error_handle_t error_handle, int *sockfd)
{
    char remote_ip_str[IP4ADDR_STRLEN_MAX];
    ip4addr_ntoa_r(&p_remote_ip->u_addr.ip4, remote_ip_str, IP4ADDR_STRLEN_MAX);

    struct sockaddr_in sa4 = { 0 };
    inet_addr_from_ip4addr(&sa4.sin_addr, ip_2_ip4(p_remote_ip));
    sa4.sin_family = AF_INET;
    sa4.sin_len = sizeof(struct sockaddr_in);
    sa4.sin_port = lwip_htons((u16_t)port);

    esp_err_t ret = ESP_OK;

#if CONFIG_LWIP_IPV4
    int fd = socket(AF_INET, SOCK_STREAM, AF_UNSPEC);
    if (fd < 0) {
        str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
        ESP_TLS_LOGE_FUNC("[%s:%d] Failed to create socket, err %d (%s)",
                          remote_ip_str, port, errno, err_desc.buf ? err_desc.buf : "");
        str_buf_free_buf(&err_desc);
        ESP_INT_EVENT_TRACKER_CAPTURE(error_handle, ESP_TLS_ERR_TYPE_SYSTEM, errno);
        ret = ESP_ERR_ESP_TLS_CANNOT_CREATE_SOCKET;
        goto err_exit;
    }
#else
#error not supported
#endif
    ESP_TLS_LOGI_FUNC("Open socket %d for %s:%d", fd, remote_ip_str, port);

    // Set timeout options, keep-alive options and bind device options if configured
    ret = esp_tls_set_socket_options(fd, cfg);
    if (ret != ESP_OK) {
        goto err_close_socket;
    }

    // Set to non block before connecting to better control connection timeout
    ret = esp_tls_set_socket_non_blocking(fd, true);
    if (ret != ESP_OK) {
        goto err_close_socket;
    }

    ret = connect(fd, (struct sockaddr*)&sa4, sa4.sin_len);
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
            ESP_TLS_LOGE_FUNC("[%s:%d] Failed to connect to host, err %d (%s)", remote_ip_str, port, errno, (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
            ESP_INT_EVENT_TRACKER_CAPTURE(error_handle, ESP_TLS_ERR_TYPE_SYSTEM, errno);
            ret = ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST;
            goto err_close_socket;
        }
        if (!(cfg && cfg->non_block)) {
            struct timeval tv = { .tv_usec = 0, .tv_sec = ESP_TLS_DEFAULT_CONN_TIMEOUT }; // Default connection timeout is 10 s
            if ( cfg && cfg->timeout_ms > 0 ) {
                ms_to_timeval(cfg->timeout_ms, &tv);
            }
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(fd, &fdset);

            int res = select(fd+1, NULL, &fdset, NULL, &tv);
            if (res < 0) {
                str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
                ESP_TLS_LOGE_FUNC("[%s:%d] select() error: %d (%s)", remote_ip_str, port, errno, (NULL != err_desc.buf) ? err_desc.buf : "");
                str_buf_free_buf(&err_desc);
                ESP_INT_EVENT_TRACKER_CAPTURE(error_handle, ESP_TLS_ERR_TYPE_SYSTEM, errno);
                goto err_close_socket;
            } else if (res == 0) {
                ESP_TLS_LOGE_FUNC("[%s:%d] select() timeout", remote_ip_str, port);
                ret = ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT;
                goto err_close_socket;
            } else {
                int sockerr = 0;
                socklen_t len = (socklen_t)sizeof(int);

                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len) < 0) {
                    str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(errno);
                    ESP_TLS_LOGE_FUNC("[%s:%d] getsockopt() error: %d (%s)", remote_ip_str, port, errno,
                             (NULL != err_desc.buf) ? err_desc.buf : "");
                    str_buf_free_buf(&err_desc);
                    ret = ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED;
                    goto err_close_socket;
                } else if (sockerr) {
                    ESP_INT_EVENT_TRACKER_CAPTURE(error_handle, ESP_TLS_ERR_TYPE_SYSTEM, sockerr);
                    str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(sockerr);
                    ESP_TLS_LOGE_FUNC("[%s:%d] delayed connect error: %d (%s)", remote_ip_str, port, sockerr,
                             (NULL != err_desc.buf) ? err_desc.buf : "");
                    str_buf_free_buf(&err_desc);
                    goto err_close_socket;
                }
            }
        }
    }

    if (cfg && !cfg->non_block) {
        // reset back to blocking mode (unless non_block configured)
        ret = esp_tls_set_socket_non_blocking(fd, false);
        if (ret != ESP_OK) {
            goto err_close_socket;
        }
    }

    *sockfd = fd;
    return ESP_OK;

err_close_socket:
    close(fd);
err_exit:
    return ret;
}

static void
esp_tls_low_level_conn_callback_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    esp_tls_async_dns_req_info_t * const p_req_info = arg;
    LWIP_UNUSED_ARG(hostname);

    if (NULL == p_req_info) {
        ESP_TLS_LOGE_FUNC("arg is NULL");
        return;
    }

    ESP_LOGD(TAG, "callback_dns_found (tls=0x%p): hostname=%p, ipaddr=%p", p_req_info->tls, hostname, ipaddr);
    ESP_LOGD(TAG, "tls->dns_mutex=%p", p_req_info->tls->dns_mutex);
    ESP_LOGD(
        TAG,
        "callback_dns_found (tls=0x%p): hostname=%s, ipaddr=0x%08x",
        p_req_info->tls,
        hostname ? hostname : "<NULL>",
        ipaddr ? ipaddr->u_addr.ip4.addr : 0);

    xSemaphoreTake(p_req_info->p_dns_mutex, portMAX_DELAY);
    p_req_info->flag_busy = false;
    if (NULL == p_req_info->tls)
    {
        // tls object was already destroyed by esp_tls_conn_destroy
        ESP_LOGD(
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
        ESP_LOGD(
            TAG,
            "[%s] %s: resolved successfully",
            pcTaskGetTaskName(NULL) ? pcTaskGetTaskName(NULL) : "???",
            __func__);
        p_req_info->tls->dns_cb_remote_ip = *ipaddr;
        p_req_info->tls->dns_cb_status = true;
    } else {
        ESP_LOGD(
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
        ESP_TLS_LOGE_FUNC("empty esp_tls parameter");
        return -1;
    }
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_TLS_LOGE_FUNC("[%.*s] psa_crypto_init failed, ret=0x%x", hostlen, hostname, (unsigned int)status);
        return -1;
    }

    esp_err_t esp_ret;
    /* These states are used to keep a tab on connection progress in case of non-blocking connect,
    and in case of blocking connect these cases will get executed one after the other */
    switch (tls->conn_state) {
    case ESP_TLS_INIT:
    {
        ESP_TLS_LOGD_FUNC("ESP_TLS_INIT: %.*s, is_plain_tcp=%d", hostlen, hostname, (int)(!(cfg && !cfg->is_plain_tcp)));
        xSemaphoreTake(tls->dns_mutex, portMAX_DELAY);
        esp_tls_async_dns_req_info_t* const p_dns_req_info = esp_tls_find_free_dns_req_info();
        if (NULL == p_dns_req_info)
        {
            xSemaphoreGive(tls->dns_mutex);
            ESP_TLS_LOGE_FUNC("Failed to resolve hostname '%.*s', there is no free slot for DNS query", hostlen, hostname);
            return -1;
        }
        p_dns_req_info->tls = tls;
        p_dns_req_info->p_dns_mutex = tls->dns_mutex;
        p_dns_req_info->flag_busy = true;
        tls->p_async_dns_req_info = p_dns_req_info;
        xSemaphoreGive(tls->dns_mutex);

        tls->sockfd = -1;
        tls->hostname = strndup(hostname, hostlen);
        if (NULL == tls->hostname) {
            ESP_TLS_LOGE_FUNC("Can't allocate memory for hostname: %.*s", hostlen, hostname);
            return ESP_ERR_NO_MEM;
        }
        if (cfg != NULL && cfg->is_plain_tcp == false) {
            _esp_tls_net_init(tls);
            tls->is_tls = true;
        }
        err_t err = dns_gethostbyname(
            tls->hostname,
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
            ESP_TLS_LOGE_FUNC("Failed to resolve hostname '%s', error %d", tls->hostname, err);
            ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME);
            free(tls->hostname);
            tls->hostname = NULL;
            return -1;
        }
        char remote_ip4_str[IP4ADDR_STRLEN_MAX];
        ip4addr_ntoa_r(&tls->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
        ESP_TLS_LOGI("Hostname '%s' resolved to %s", tls->hostname, remote_ip4_str);

        tls->conn_state = ESP_TLS_CONNECT;
#if defined(__GNUC__) && (__GNUC__ >= 7)
        __attribute__((fallthrough));
#endif
    }
    /* falls through */
    case ESP_TLS_CONNECT:
    {
        ESP_TLS_LOGD_FUNC("[%s] ESP_TLS_CONNECT", esp_tls_get_hostname(tls));
        char remote_ip4_str[IP4ADDR_STRLEN_MAX];
        ip4addr_ntoa_r(&tls->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
        ESP_TLS_LOGD_FUNC("[%s] Connect to IP: %s", esp_tls_get_hostname(tls), remote_ip4_str);
        if ((esp_ret = esp_tls_tcp_connect(&tls->remote_ip, port, cfg, tls->error_handle, &tls->sockfd)) != ESP_OK) {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(esp_ret);
            ESP_TLS_LOGE_FUNC("[%s] esp_tcp_connect failed, err=%d (%s)",
                    esp_tls_get_hostname(tls), esp_ret, (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
            ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, esp_ret);
            return -1;
        }
        if (cfg && cfg->non_block) {
            tls->timer_start = xTaskGetTickCount();
            ESP_TLS_LOGD_FUNC("[sock=%d][%s] ESP_TLS_INIT: start timer: %u", tls->sockfd, esp_tls_get_hostname(tls), tls->timer_start);
        }
        tls->conn_state = ESP_TLS_CONNECTING;
#if defined(__GNUC__) && (__GNUC__ >= 7)
        __attribute__((fallthrough));
#endif
    }
    /* falls through */
    case ESP_TLS_CONNECTING:
        ESP_TLS_LOGD_FUNC("[sock=%d][%s] ESP_TLS_CONNECTING: non_block=%d", tls->sockfd, esp_tls_get_hostname(tls), cfg->non_block);
        if (cfg && cfg->non_block)
        {
            ESP_TLS_LOGD_FUNC("[sock=%d][%s] connecting...", tls->sockfd, esp_tls_get_hostname(tls));
            struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };

            FD_ZERO(&tls->rset);
            FD_SET(tls->sockfd, &tls->rset);
            tls->wset = tls->rset;

            /* In case of non-blocking I/O, we use the select() API to check whether
               connection has been established or not*/
            if (select(tls->sockfd + 1, &tls->rset, &tls->wset, NULL, &tv) < 0) {
                ESP_TLS_LOGE_FUNC("[sock=%d][%s] Non blocking connecting failed", tls->sockfd, esp_tls_get_hostname(tls));
                tls->conn_state = ESP_TLS_FAIL;
                close(tls->sockfd);
                tls->sockfd = -1;
                return -1;
            }
            ESP_TLS_LOGD_FUNC("[sock=%d][%s] select: rset=%d, wset=%d",
                              tls->sockfd, esp_tls_get_hostname(tls), (int)(FD_ISSET(tls->sockfd, &tls->rset)), (int)(FD_ISSET(tls->sockfd, &tls->wset)));
            if (FD_ISSET(tls->sockfd, &tls->rset) || FD_ISSET(tls->sockfd, &tls->wset)) {
                int error = 0;
                socklen_t len = sizeof(error);
                /* pending error check */
                if (getsockopt(tls->sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                    ESP_TLS_LOGE_FUNC("[sock=%d][%s] Non blocking connect failed", tls->sockfd, esp_tls_get_hostname(tls));
                    ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_SYSTEM, errno);
                    ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED);
                    tls->conn_state = ESP_TLS_FAIL;
                    close(tls->sockfd);
                    tls->sockfd = -1;
                    return -1;
                }
                if (0 != error) {
                    str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(error);
                    ESP_TLS_LOGE_FUNC("[sock=%d][%s] Non blocking connect failed: error=%d (%s)",
                                      tls->sockfd, esp_tls_get_hostname(tls), error, (NULL != err_desc.buf) ? err_desc.buf : "");
                    str_buf_free_buf(&err_desc);
                    ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_SYSTEM, error);
                    ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, error);
                    tls->conn_state = ESP_TLS_FAIL;
                    close(tls->sockfd);
                    tls->sockfd = -1;
                    return -1;
                }
            } else {
                const TickType_t now = xTaskGetTickCount();
                const uint32_t delta_ticks = now - tls->timer_start;
                ESP_TLS_LOGD_FUNC("[sock=%d][%s] ESP_TLS_CONNECTING: timer delta_ticks: %u", tls->sockfd, esp_tls_get_hostname(tls), delta_ticks);
                if (delta_ticks > pdMS_TO_TICKS(cfg->timeout_ms))
                {
                    ESP_TLS_LOGE_FUNC("[sock=%d][%s] connection timeout", tls->sockfd, esp_tls_get_hostname(tls));
                    ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_SYSTEM, ESP_ERR_TIMEOUT);
                    ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, ESP_ERR_TIMEOUT);
                    tls->conn_state = ESP_TLS_FAIL;
                    close(tls->sockfd);
                    tls->sockfd = -1;
                    return -1;
                }
                return 0; // Connection has not yet established
            }
        }
        /* By now, the connection has been established */
        if (tls->is_tls == false) {
            tls->conn_state = ESP_TLS_DONE;
            tls->read = tcp_read;
            tls->write = tcp_write;
            ESP_TLS_LOGD_FUNC("[sock=%d][%s] non-tls connection established", tls->sockfd, esp_tls_get_hostname(tls));
            return 1;
        }
        esp_ret = create_ssl_handle(hostname, hostlen, cfg, tls);
        if (esp_ret != ESP_OK) {
            str_buf_t err_desc = esp_err_to_name_with_alloc_str_buf(tls->error_handle->esp_tls_error_code);
            ESP_TLS_LOGE_FUNC("[sock=%d][%s] create_ssl_handle failed, error code: -0x%04X(%d) (%s)",
                tls->sockfd,
                esp_tls_get_hostname(tls),
                -tls->error_handle->esp_tls_error_code,
                tls->error_handle->esp_tls_error_code,
                (NULL != err_desc.buf) ? err_desc.buf : "");
            str_buf_free_buf(&err_desc);
            ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP, esp_ret);
            tls->conn_state = ESP_TLS_FAIL;
            close(tls->sockfd);
            tls->sockfd = -1;
            return -1;
        }
        tls->read = _esp_tls_read;
        tls->write = _esp_tls_write;
        tls->conn_state = ESP_TLS_HANDSHAKE;
        if (cfg && cfg->non_block) {
            tls->timer_start = xTaskGetTickCount();
            ESP_TLS_LOGD_FUNC("[sock=%d][%s] ESP_TLS_CONNECTING: start timer: %u", tls->sockfd, esp_tls_get_hostname(tls), tls->timer_start);
        }
#if defined(__GNUC__) && (__GNUC__ >= 7)
        __attribute__((fallthrough));
#endif
    /* falls through */
    case ESP_TLS_HANDSHAKE:
    {
        ESP_TLS_LOGD_FUNC("[sock=%d][%s] handshake in progress...", tls->sockfd, esp_tls_get_hostname(tls));
        const int res = esp_tls_handshake(tls, cfg);
        if (res == 0) {
            const TickType_t now = xTaskGetTickCount();
            const uint32_t delta_ticks = now - tls->timer_start;
            ESP_TLS_LOGD_FUNC("[sock=%d][%s] ESP_TLS_HANDSHAKE: timer delta_ticks: %u", tls->sockfd, esp_tls_get_hostname(tls), delta_ticks);
            if (delta_ticks > pdMS_TO_TICKS(cfg->timeout_ms))
            {
                ESP_TLS_LOGE_FUNC("[sock=%d][%s] connection timeout", tls->sockfd, esp_tls_get_hostname(tls));
                tls->conn_state = ESP_TLS_FAIL;
                // after create_ssl_handle we don't need to close the socket manually
                return -1;
            }
            return 0; // Connection has not yet established
        }
        return res;
    }
    case ESP_TLS_FAIL:
        ESP_TLS_LOGE_FUNC("[sock=%d][%s] failed to open a new connection", tls->sockfd, esp_tls_get_hostname(tls));
        break;
    case ESP_TLS_HOSTNAME_RESOLVING:
    {
        ESP_TLS_LOGD_FUNC("[%s] ESP_TLS_HOSTNAME_RESOLVING", esp_tls_get_hostname(tls));
        xSemaphoreTake(tls->dns_mutex, portMAX_DELAY);
        if (tls->dns_cb_ready) {
            tls->dns_cb_ready = false;
            tls->remote_ip = tls->dns_cb_remote_ip;
            if (tls->dns_cb_status) {
                char remote_ip4_str[IP4ADDR_STRLEN_MAX];
                ip4addr_ntoa_r(&tls->remote_ip.u_addr.ip4, remote_ip4_str, sizeof(remote_ip4_str));
                ESP_TLS_LOGI("Hostname '%s' resolved to %s", tls->hostname, remote_ip4_str);
                tls->conn_state = ESP_TLS_CONNECT;
            } else {
                ESP_TLS_LOGE_FUNC("[%s] hostname resolving failed", esp_tls_get_hostname(tls));
                ESP_TLS_LOGD_FUNC("[%s] tls->error_handle=%p", esp_tls_get_hostname(tls), tls->error_handle);
                ESP_INT_EVENT_TRACKER_CAPTURE(tls->error_handle, ESP_TLS_ERR_TYPE_ESP,
                                              ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME);
                tls->conn_state = ESP_TLS_FAIL;
            }
        }
        xSemaphoreGive(tls->dns_mutex);
        if (ESP_TLS_FAIL == tls->conn_state) {
            ESP_TLS_LOGE_FUNC("[%s] failed to open a new connection", esp_tls_get_hostname(tls));
            break;
        }
        return 0;
    }
    default:
        ESP_TLS_LOGE_FUNC("[%s] invalid esp-tls state", esp_tls_get_hostname(tls));
        break;
    }
    return -1;
}

static esp_err_t resolve_dns(const char *host, struct sockaddr_in *ip)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    str_buf_t err_desc = str_buf_init_null();

    int err = getaddrinfo(host, NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        const char* p_err_desc = "Unknown";
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
                err_desc = esp_err_to_name_with_alloc_str_buf(err);
                p_err_desc = err_desc.buf;
                break;
        }
        ESP_TLS_LOGE_FUNC("[%s] DNS lookup failed err=%d (%s)", host, err, p_err_desc ? p_err_desc : "");
        str_buf_free_buf(&err_desc);
        return (err != 0) ? err : ESP_FAIL;
    }
    ip->sin_family = AF_INET;
    memcpy(&ip->sin_addr, &((struct sockaddr_in *)(res->ai_addr))->sin_addr, sizeof(ip->sin_addr));
    freeaddrinfo(res);
    return ESP_OK;
}

/**
 * @brief Create a new plain TCP connection
 */
esp_err_t esp_tls_plain_tcp_connect(const char *host, int hostlen, int port, const esp_tls_cfg_t *cfg, esp_tls_error_handle_t error_handle, int *sockfd)
{
    if (sockfd == NULL || error_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "esp_tls_plain_tcp_connect: %.*s:%d, timeout=%d ms, non_block=%d", hostlen, host, port, cfg ? cfg->timeout_ms : 0, cfg ? cfg->non_block : 0);
    char* hostname = strndup(host, hostlen);
    if (NULL == hostname) {
        ESP_TLS_LOGE_FUNC("Can't allocate memory for hostname: %.*s", hostlen, host);
        return ESP_ERR_NO_MEM;
    }
    struct sockaddr_in remote_ip;
    bzero(&remote_ip, sizeof(struct sockaddr_in));
    if (inet_pton(AF_INET, host, &remote_ip.sin_addr) != 1) {
        esp_err_t res = resolve_dns(host, &remote_ip);
        if (ESP_OK != res) {
            free(hostname);
            hostname = NULL;
            return res;
        }
    }
    char remote_ip4_str[IP4ADDR_STRLEN_MAX];
    ip4addr_ntoa_r((const ip4_addr_t*)&remote_ip.sin_addr.s_addr, remote_ip4_str, sizeof(remote_ip4_str));
    ESP_LOGD(TAG, "Connecting to server %s. IP: %s, Port: %d", hostname, remote_ip4_str, port);
    free(hostname);
    hostname = NULL;
    ip_addr_t remote_ip_addr = { 0 };
    ip4addr_aton(remote_ip4_str, &remote_ip_addr.u_addr.ip4);
    return esp_tls_tcp_connect(&remote_ip_addr, port, cfg, error_handle, sockfd);
}

int esp_tls_conn_new_sync(const char *hostname, int hostlen, int port, const esp_tls_cfg_t *cfg, esp_tls_t *tls)
{
    const TickType_t start_time_tick = xTaskGetTickCount();
    while (1) {
        int ret = esp_tls_low_level_conn(hostname, hostlen, port, cfg, tls);
        if (ret == 1) {
            return ret;
        } else if (ret == -1) {
            ESP_TLS_LOGE_FUNC("[%s] Failed to open new connection", esp_tls_get_hostname(tls));
            return -1;
        } else if (ret == 0 && cfg->timeout_ms >= 0) {
            const TickType_t current_time_tick = xTaskGetTickCount();
            const uint32_t elapsed_time_ticks = current_time_tick - start_time_tick;
            if (elapsed_time_ticks >= pdMS_TO_TICKS(cfg->timeout_ms)) {
                ESP_TLS_LOGW_FUNC(
                    "[%s] Failed to open new connection in specified timeout (%u ms)",
                    esp_tls_get_hostname(tls),
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
    ESP_TLS_LOGD_FUNC("[sock=%d][%s] esp_tls_low_level_conn", tls->sockfd, esp_tls_get_hostname(tls));
    const int ret = esp_tls_low_level_conn(hostname, hostlen, port, cfg, tls);
    ESP_TLS_LOGD_FUNC("[sock=%d][%s] esp_tls_low_level_conn, ret=%d", tls->sockfd, esp_tls_get_hostname(tls), ret);
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
    esp_tls_conn_destroy(tls);
    return NULL;
}

/**
 * @brief      Create a new TLS/SSL connection with a given "HTTP" url
 */
int esp_tls_conn_http_new_sync(const char *url, const esp_tls_cfg_t *cfg, esp_tls_t *tls)
{
    /* Parse URI */
    struct http_parser_url u;
    http_parser_url_init(&u);
    http_parser_parse_url(url, strlen(url), 0, &u);

    /* Connect to host */
    return esp_tls_conn_new_sync(&url[u.field_data[UF_HOST].off], u.field_data[UF_HOST].len,
                                  get_port(url, &u), cfg, tls);
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

#ifdef CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS
bool esp_tls_copy_client_session(const esp_tls_t * const tls, esp_tls_client_session_t * const p_client_session)
{
    return _esp_tls_copy_client_session(tls, p_client_session);
}

esp_tls_client_session_t *esp_tls_get_client_session(esp_tls_t *tls)
{
    return _esp_tls_get_client_session(tls);
}

void esp_tls_free_client_session(esp_tls_client_session_t *client_session)
{
    _esp_tls_free_client_session(client_session);
}
#endif /* CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS */


#ifdef CONFIG_ESP_TLS_SERVER
esp_err_t esp_tls_cfg_server_session_tickets_init(esp_tls_cfg_server_t *cfg)
{
#if defined(CONFIG_ESP_TLS_SERVER_SESSION_TICKETS)
    if (!cfg || cfg->ticket_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    cfg->ticket_ctx = calloc(1, sizeof(esp_tls_server_session_ticket_ctx_t));
    if (!cfg->ticket_ctx) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret =  _esp_tls_server_session_ticket_ctx_init(cfg->ticket_ctx);
    if (ret != ESP_OK) {
        free(cfg->ticket_ctx);
    }
    return ret;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

void esp_tls_cfg_server_session_tickets_free(esp_tls_cfg_server_t *cfg)
{
#if defined(CONFIG_ESP_TLS_SERVER_SESSION_TICKETS)
    if (cfg && cfg->ticket_ctx) {
        _esp_tls_server_session_ticket_ctx_free(cfg->ticket_ctx);
    }
#endif
}

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

void *esp_tls_get_ssl_context(esp_tls_t *tls)
{
    return _esp_tls_get_ssl_context(tls);
}

esp_err_t esp_tls_get_conn_sockfd(esp_tls_t *tls, int *sockfd)
{
    if (!tls || !sockfd) {
        ESP_TLS_LOGE_FUNC("Invalid arguments passed");
        return ESP_ERR_INVALID_ARG;
    }
    *sockfd = tls->sockfd;
    return ESP_OK;
}

esp_err_t esp_tls_set_conn_sockfd(esp_tls_t *tls, int sockfd)
{
    if (!tls || sockfd < 0) {
        ESP_TLS_LOGE_FUNC("Invalid arguments passed");
        return ESP_ERR_INVALID_ARG;
    }
    tls->sockfd = sockfd;
    return ESP_OK;
}

esp_err_t esp_tls_get_conn_state(esp_tls_t *tls, esp_tls_conn_state_t *conn_state)
{
    if (!tls || !conn_state) {
        ESP_TLS_LOGE_FUNC("Invalid arguments passed");
        return ESP_ERR_INVALID_ARG;
    }
    *conn_state = tls->conn_state;
    return ESP_OK;
}

esp_err_t esp_tls_set_conn_state(esp_tls_t *tls, esp_tls_conn_state_t conn_state)
{
    if (!tls || conn_state < ESP_TLS_INIT || conn_state > ESP_TLS_DONE) {
        ESP_TLS_LOGE_FUNC("Invalid arguments passed");
        return ESP_ERR_INVALID_ARG;
    }
    tls->conn_state = conn_state;
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

esp_err_t esp_tls_get_error_handle(esp_tls_t *tls, esp_tls_error_handle_t *error_handle)
{
    if (!tls || !error_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    *error_handle = tls->error_handle;
    return ESP_OK;
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

const char* esp_tls_get_hostname(const esp_tls_t * const tls)
{
    if (NULL == tls) {
        return "<NULL>";
    }
    return (NULL != tls->hostname) ? tls->hostname : "";
}
