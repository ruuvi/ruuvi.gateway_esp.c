/**
 * @file http.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http.h"
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "cjson_wrap.h"
#include "esp_http_client.h"
#include "ruuvi_gateway.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "log.h"

static const char TAG[] = "http";

static esp_err_t
http_event_handler(esp_http_client_event_t *p_evt)
{
    switch (p_evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            LOG_ERR("HTTP_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            LOG_DBG("HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_HEADER_SENT:
            LOG_DBG("HTTP_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            LOG_DBG("HTTP_EVENT_ON_HEADER: %.*s", p_evt->data_len, (char *)p_evt->data);
            break;

        case HTTP_EVENT_ON_DATA:
            LOG_DBG("HTTP_EVENT_ON_DATA, len=%d: %.*s", p_evt->data_len, p_evt->data_len, (char *)p_evt->data);
            break;

        case HTTP_EVENT_ON_FINISH:
            LOG_DBG("HTTP_EVENT_ON_FINISH");
            break;

        case HTTP_EVENT_DISCONNECTED:
            LOG_DBG("HTTP_EVENT_DISCONNECTED");
            break;

        default:
            break;
    }
    return ESP_OK;
}

void
http_send(const char *p_msg)
{
    esp_http_client_config_t http_config = {
        .url                   = g_gateway_config.http_url,
        .host                  = NULL,
        .port                  = 0,
        .username              = g_gateway_config.http_user,
        .password              = g_gateway_config.http_pass,
        .auth_type             = ('\0' != g_gateway_config.http_user[0]) ? HTTP_AUTH_TYPE_BASIC : HTTP_AUTH_TYPE_NONE,
        .path                  = NULL,
        .query                 = NULL,
        .cert_pem              = NULL,
        .client_cert_pem       = NULL,
        .client_key_pem        = NULL,
        .method                = HTTP_METHOD_POST,
        .timeout_ms            = 0,
        .disable_auto_redirect = false,
        .max_redirection_count = 0,
        .event_handler         = &http_event_handler,
        .transport_type        = HTTP_TRANSPORT_UNKNOWN,
        .buffer_size           = 0,
        .buffer_size_tx        = 0,
        .user_data             = NULL,
        .is_async              = false,
        .use_global_ca_store   = false,
        .skip_cert_common_name_check = false,
    };
    esp_http_client_handle_t http_handle = esp_http_client_init(&http_config);
    if (NULL == http_handle)
    {
        LOG_ERR("Can't init http client");
        return;
    }

    esp_http_client_set_post_field(http_handle, p_msg, strlen(p_msg));
    esp_http_client_set_header(http_handle, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_perform(http_handle);
    if (ESP_OK == err)
    {
        LOG_DBG(
            "HTTP POST Status = %d, content_length = %d",
            esp_http_client_get_status_code(http_handle),
            esp_http_client_get_content_length(http_handle));
    }
    else
    {
        LOG_ERR_ESP(err, "HTTP POST request failed");
    }

    err = esp_http_client_cleanup(http_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_http_client_cleanup failed");
    }
}

void
http_send_advs(const struct adv_report_table *reports)
{
    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json");
        return;
    }

    cJSON *p_json_data = cJSON_AddObjectToObject(p_json_root, "data");
    if (NULL == p_json_data)
    {
        LOG_ERR("Can't create json");
        return;
    }

    const time_t now = time(NULL);
    cJSON_AddStringToObject(p_json_data, "coordinates", g_gateway_config.coordinates);
    cjson_wrap_add_timestamp(p_json_data, "timestamp", now);
    cJSON_AddStringToObject(p_json_data, "gw_mac", gw_mac_sta.str_buf);

    cJSON *p_json_tags = cJSON_AddObjectToObject(p_json_data, "tags");
    if (NULL != p_json_tags)
    {
        LOG_ERR("can't create json");
        return;
    }
    for (num_of_advs_t i = 0; i < reports->num_of_advs; ++i)
    {
        const adv_report_t *p_adv      = &reports->table[i];
        cJSON *             p_json_tag = cJSON_CreateObject();
        cJSON_AddNumberToObject(p_json_tag, "rssi", p_adv->rssi);
        cjson_wrap_add_timestamp(p_json_tag, "timestamp", p_adv->timestamp);
        cJSON_AddStringToObject(p_json_tag, "data", p_adv->data);
        const mac_address_str_t mac_str = mac_address_to_str(&p_adv->tag_mac);
        cJSON_AddItemToObject(p_json_tags, mac_str.str_buf, p_json_tag);
    }

    cjson_wrap_str_t json_str = cjson_wrap_print_and_delete(&p_json_root);
    LOG_INFO("HTTP POST: %s", json_str.p_str);
    http_send(json_str.p_str);
    cjson_wrap_free_json_str(&json_str);
}
