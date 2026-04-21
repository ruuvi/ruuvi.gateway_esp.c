/**
 * @file tcp_transport_stub.c
 * @author TheSomeMan
 * @date 2026-04-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "tcp_transport_stub.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <esp_attr.h>
#include "esp_tls.h"
// #include "mbedtls/ssl_misc.h"
#define LOG_LOCAL_LEVEL 3
#include "esp_log.h"

// #include "esp_heap_caps.h"
#include "esp_transport.h"
#include "esp_transport_ssl.h"
#include "esp_transport_internal.h"
#include "snprintf_with_esp_err_desc.h"

static const char* TAG = "transport_base";

#define INVALID_SOCKET (-1)

#define GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t) \
    transport_esp_tls_t* ssl = ssl_get_context_data(t); \
    if (!ssl) \
    { \
        return; \
    }

typedef enum
{
    TRANS_SSL_INIT = 0,
    TRANS_SSL_CONNECTING,
} transport_ssl_conn_state_t;

/**
 *  mbedtls specific transport data
 */
typedef struct transport_esp_tls
{
    esp_tls_t*                 tls;
    esp_tls_cfg_t              cfg;
    bool                       ssl_initialized;
    transport_ssl_conn_state_t conn_state;
    int                        sockfd;
    bool                       flag_timer_started;
    TickType_t                 timer_start;
} transport_esp_tls_t;

static inline transport_esp_tls_t*
ssl_get_context_data(esp_transport_handle_t t)
{
    if (!t)
    {
        return NULL;
    }
    return (transport_esp_tls_t*)t->data;
}

static int
base_get_socket(esp_transport_handle_t t)
{
    transport_esp_tls_t* ctx = ssl_get_context_data(t);
    if (ctx)
    {
        return ctx->sockfd;
    }
    return INVALID_SOCKET;
}

static transport_esp_tls_t*
esp_transport_esp_tls_create(void)
{
    transport_esp_tls_t* transport_esp_tls = calloc(1, sizeof(transport_esp_tls_t));
    if (transport_esp_tls == NULL)
    {
        return NULL;
    }
    transport_esp_tls->sockfd = INVALID_SOCKET;
    return transport_esp_tls;
}

esp_foundation_transport_t*
esp_transport_init_foundation_transport(void)
{
    esp_foundation_transport_t* foundation = calloc(1, sizeof(esp_foundation_transport_t));
    ESP_TRANSPORT_MEM_CHECK(TAG, foundation, return NULL);
    foundation->error_handle = calloc(1, sizeof(struct esp_transport_error_storage));
    ESP_TRANSPORT_MEM_CHECK(TAG, foundation->error_handle, free(foundation); return NULL);
    return foundation;
}

static esp_transport_handle_t
esp_transport_base_init(void)
{
    esp_transport_handle_t transport = esp_transport_init();
    if (transport == NULL)
    {
        return NULL;
    }
    transport->foundation = esp_transport_init_foundation_transport();
    ESP_TRANSPORT_MEM_CHECK(TAG, transport->foundation, free(transport); return NULL);

    transport->data = esp_transport_esp_tls_create();
    ESP_TRANSPORT_MEM_CHECK(TAG, transport->data, free(transport->foundation); free(transport); return NULL)
    return transport;
}

static int
base_close(esp_transport_handle_t t)
{
    return -1;
}

static int
base_poll_read(esp_transport_handle_t t, int timeout_ms)
{
    return -1;
}

static int
base_poll_write(esp_transport_handle_t t, int timeout_ms)
{
    return -1;
}

static int
base_destroy(esp_transport_handle_t transport)
{
    return 0;
}

esp_transport_handle_t
esp_transport_tcp_init(void)
{
    esp_transport_handle_t tcp_transport = esp_transport_base_init();
    if (tcp_transport == NULL)
    {
        return NULL;
    }
    ((transport_esp_tls_t*)tcp_transport->data)->cfg.is_plain_tcp = true;
    esp_transport_set_func(
        tcp_transport,
        tcp_connect,
        tcp_read,
        tcp_write,
        base_close,
        base_poll_read,
        base_poll_write,
        base_destroy);
    esp_transport_set_async_connect_func(tcp_transport, tcp_connect_async);
    tcp_transport->_get_socket = base_get_socket;
    return tcp_transport;
}

esp_transport_handle_t
esp_transport_ssl_init(void)
{
    esp_transport_handle_t ssl_transport = esp_transport_base_init();
    if (ssl_transport == NULL)
    {
        return NULL;
    }
    ((transport_esp_tls_t*)ssl_transport->data)->cfg.is_plain_tcp = false;
    esp_transport_set_func(
        ssl_transport,
        tcp_connect,
        tcp_read,
        tcp_write,
        base_close,
        base_poll_read,
        base_poll_write,
        base_destroy);
    esp_transport_set_async_connect_func(ssl_transport, tcp_connect_async);
    ssl_transport->_get_socket = base_get_socket;
    return ssl_transport;
}

void
esp_transport_tcp_set_keep_alive(esp_transport_handle_t t, esp_transport_keep_alive_t* keep_alive_cfg)
{
}

void
esp_transport_ssl_enable_global_ca_store(esp_transport_handle_t t)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.use_global_ca_store = true;
}

void
esp_transport_ssl_set_cert_data(esp_transport_handle_t t, const char* data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.cacert_pem_buf   = (void*)data;
    ssl->cfg.cacert_pem_bytes = len + 1;
}

void
esp_transport_ssl_set_client_cert_data(esp_transport_handle_t t, const char* data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientcert_pem_buf   = (void*)data;
    ssl->cfg.clientcert_pem_bytes = len + 1;
}

void
esp_transport_ssl_set_client_key_data(esp_transport_handle_t t, const char* data, int len)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.clientkey_pem_buf   = (void*)data;
    ssl->cfg.clientkey_pem_bytes = len + 1;
}

void
esp_transport_ssl_skip_common_name_check(esp_transport_handle_t t)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.skip_common_name = true;
}

void
esp_transport_ssl_set_keep_alive(esp_transport_handle_t t, esp_transport_keep_alive_t* keep_alive_cfg)
{
    GET_SSL_FROM_TRANSPORT_OR_RETURN(ssl, t);
    ssl->cfg.keep_alive_cfg = (tls_keep_alive_cfg_t*)keep_alive_cfg;
}

void
esp_transport_ssl_crt_bundle_attach(esp_transport_handle_t t, esp_err_t((*crt_bundle_attach)(void* conf)))
{
}

esp_err_t
esp_crt_bundle_attach(void* conf)
{
    return ESP_OK;
}

bool
esp_transport_ssl_set_buffer(esp_transport_handle_t t, const esp_transport_ssl_buf_cfg_t* const p_buf_cfg)
{
    return true;
}

esp_err_t
esp_tls_get_and_clear_last_error(esp_tls_error_handle_t h, int* esp_tls_code, int* esp_tls_flags)
{
    *esp_tls_code  = 0;
    *esp_tls_flags = 0;
    return ESP_OK;
}

esp_err_t
esp_tls_get_and_clear_error_type(esp_tls_error_handle_t h, esp_tls_error_type_t type, int* code)
{
    *code = 0;
    return ESP_OK;
}
