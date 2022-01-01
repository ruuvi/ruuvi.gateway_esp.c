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

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp_transport_utils.h"
#include "esp_transport.h"
#include "esp_transport_internal.h"

static const char *TAG = "TRANS_TCP";

typedef enum {
    TRANS_TCP_INIT = 0,
    TRANS_TCP_CONNECTING,
    TRANS_TCP_CONNECTED,
    TRANS_TCP_FAIL,
} transport_tcp_conn_state_t;

typedef struct {
    int sock;
    transport_tcp_conn_state_t conn_state;
    bool non_block; /*!< Configure non-blocking mode. If set to true the
                         underneath socket will be configured in non
                         blocking mode after tls session is established */
    fd_set rset;    /*!< read file descriptors */
    fd_set wset;    /*!< write file descriptors */
    struct timeval timer_start;
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
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
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

static int tcp_connect(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    struct sockaddr_in remote_ip;
    struct timeval tv = { 0 };
    transport_tcp_t *tcp = esp_transport_get_context_data(t);

    ESP_LOGD(TAG, "tcp_connect: %s:%d, timeout=%d ms, non_block=%d", host, port, timeout_ms, tcp->non_block);

    bzero(&remote_ip, sizeof(struct sockaddr_in));

    //if stream_host is not ip address, resolve it AF_INET,servername,&serveraddr.sin_addr
    if (inet_pton(AF_INET, host, &remote_ip.sin_addr) != 1) {
        if (resolve_dns(host, &remote_ip) < 0) {
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
        ESP_LOGE(TAG, "[sock=%d] get file flags error: %s", tcp->sock, strerror(errno));
        goto error;
    }
    if (fcntl(tcp->sock, F_SETFL, flags |= O_NONBLOCK) < 0) {
        ESP_LOGE(TAG, "[sock=%d] set nonblocking error: %s", tcp->sock, strerror(errno));
        goto error;
    }

    ESP_LOGD(TAG, "[sock=%d] Connecting to server. IP: %s, Port: %d",
            tcp->sock, ipaddr_ntoa((const ip_addr_t*)&remote_ip.sin_addr.s_addr), port);

    if (connect(tcp->sock, (struct sockaddr *)(&remote_ip), sizeof(struct sockaddr)) < 0) {
        if (!tcp->non_block && (errno == EINPROGRESS)) {
            fd_set fdset;

            esp_transport_utils_ms_to_timeval(timeout_ms, &tv);
            FD_ZERO(&fdset);
            FD_SET(tcp->sock, &fdset);

            int res = select(tcp->sock+1, NULL, &fdset, NULL, &tv);
            if (res < 0) {
                ESP_LOGE(TAG, "[sock=%d] select() error: %s", tcp->sock, strerror(errno));
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
                    ESP_LOGE(TAG, "[sock=%d] getsockopt() error: %s", tcp->sock, strerror(errno));
                    goto error;
                }
                else if (sockerr) {
                    esp_transport_capture_errno(t, sockerr);
                    ESP_LOGE(TAG, "[sock=%d] delayed connect error: %s", tcp->sock, strerror(sockerr));
                    goto error;
                }
            }
        } else if (!tcp->non_block || (errno != EINPROGRESS)) {
            ESP_LOGE(TAG, "[sock=%d] connect() error: %s", tcp->sock, strerror(errno));
            goto error;
        }
    }
    if (!tcp->non_block) {
        // Reset socket to blocking
        if ((flags = fcntl(tcp->sock, F_GETFL, NULL)) < 0) {
            ESP_LOGE(TAG, "[sock=%d] get file flags error: %s", tcp->sock, strerror(errno));
            goto error;
        }
        if (fcntl(tcp->sock, F_SETFL, flags & ~O_NONBLOCK) < 0) {
            ESP_LOGE(TAG, "[sock=%d] reset blocking error: %s", tcp->sock, strerror(errno));
            goto error;
        }
    }
    return tcp->sock;
error:
    close(tcp->sock);
    tcp->sock = -1;
    return -1;
}

static int esp_transport_tcp_connect_async(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    /* These states are used to keep a tab on connection progress in case of non-blocking connect,
    and in case of blocking connect these cases will get executed one after the other */
    switch (tcp->conn_state) {
        case TRANS_TCP_INIT:
            tcp->non_block = true;
            tcp->sock = tcp_connect(t, host, port, timeout_ms);
            if (tcp->sock < 0) {
                return -1;
            }
            FD_ZERO(&tcp->rset);
            FD_SET(tcp->sock, &tcp->rset);
            tcp->wset = tcp->rset;
            gettimeofday(&tcp->timer_start, NULL);
            tcp->conn_state = TRANS_TCP_CONNECTING;
            return 0; // Connection has not yet established

        case TRANS_TCP_CONNECTING:
        {
            struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
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
                struct timeval now = { .tv_sec = 0, .tv_usec = 0 };
                gettimeofday(&now, NULL);
                unsigned long delta_ms = (now.tv_sec - tcp->timer_start.tv_sec) * 1000ul
                                         + (now.tv_usec - tcp->timer_start.tv_usec) / 1000;
                if (delta_ms > timeout_ms)
                {
                    ESP_LOGD(TAG, "select() timed out");
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
        default:
            ESP_LOGE(TAG, "%s: invalid TCP conn-state", __func__);
            break;
    }
    return -1;
}

static int tcp_write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll;
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    if ((poll = esp_transport_poll_write(t, timeout_ms)) <= 0) {
        return poll;
    }
    return write(tcp->sock, buffer, len);
}

static int tcp_read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    transport_tcp_t *tcp = esp_transport_get_context_data(t);
    int poll = -1;
    if ((poll = esp_transport_poll_read(t, timeout_ms)) <= 0) {
        return poll;
    }
    int read_len = read(tcp->sock, buffer, len);
    if (read_len == 0) {
        return -1;
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
        ESP_LOGE(TAG, "tcp_poll_read select error %d, errno = %s, fd = %d", sock_errno, strerror(sock_errno), tcp->sock);
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
        ESP_LOGE(TAG, "tcp_poll_write select error %d, errno = %s, fd = %d", sock_errno, strerror(sock_errno), tcp->sock);
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
    esp_transport_handle_t t = esp_transport_init();
    transport_tcp_t *tcp = calloc(1, sizeof(transport_tcp_t));
    ESP_TRANSPORT_MEM_CHECK(TAG, tcp, {
        esp_transport_destroy(t);
        return NULL;
    });

    tcp->sock = -1;
    tcp->non_block = false;
    esp_transport_set_func(t, tcp_connect, tcp_read, tcp_write, tcp_close, tcp_poll_read, tcp_poll_write, tcp_destroy);
    esp_transport_set_context_data(t, tcp);
    esp_transport_set_async_connect_func(t, &esp_transport_tcp_connect_async);
    t->_get_socket = tcp_get_socket;

    return t;
}
