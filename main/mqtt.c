/**
 * @file mqtt.c
 * @author Jukka Saari
 * @date 2019-11-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "mqtt.h"
#include "cJSON.h"
#include "cjson_wrap.h"
#include "esp_err.h"
#include "mqtt_client.h"
#include "ruuvi_gateway.h"
#include "mqtt_json.h"
#include "leds.h"
#include "fw_update.h"
#include "os_mutex.h"
#include "gw_mac.h"
#include "esp_crt_bundle.h"
#include "gw_status.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define TOPIC_LEN 512

#define MQTT_NETWORK_TIMEOUT_MS (10U * 1000U)

typedef int mqtt_message_id_t;

typedef int esp_mqtt_client_data_len_t;

typedef struct mqtt_topic_buf_t
{
    char buf[TOPIC_LEN];
} mqtt_topic_buf_t;

typedef struct mqtt_protected_data_t
{
    esp_mqtt_client_handle_t p_mqtt_client;
    mqtt_topic_buf_t         mqtt_topic;
} mqtt_protected_data_t;

static bool                  g_mqtt_mutex_initialized = false;
static os_mutex_t            g_mqtt_mutex;
static os_mutex_static_t     g_mqtt_mutex_mem;
static mqtt_protected_data_t g_mqtt_data;

static const char *TAG = "MQTT";

static mqtt_protected_data_t *
mqtt_mutex_lock(void)
{
    if (!g_mqtt_mutex_initialized)
    {
        g_mqtt_mutex             = os_mutex_create_static(&g_mqtt_mutex_mem);
        g_mqtt_mutex_initialized = true;
    }
    os_mutex_lock(g_mqtt_mutex);
    return &g_mqtt_data;
}

static void
mqtt_mutex_unlock(mqtt_protected_data_t **const p_p_data)
{
    *p_p_data = NULL;
    os_mutex_unlock(g_mqtt_mutex);
}

static void
mqtt_create_full_topic(
    mqtt_topic_buf_t *const p_full_topic,
    const char *const       p_prefix_str,
    const char *const       p_topic_str)
{
    if ((NULL == p_full_topic) || (NULL == p_topic_str))
    {
        LOG_ERR("null arguments");
        return;
    }

    if ((NULL != p_prefix_str) && ('\0' != p_prefix_str[0]))
    {
        snprintf(p_full_topic->buf, sizeof(p_full_topic->buf), "%s%s", p_prefix_str, p_topic_str);
    }
    else
    {
        snprintf(p_full_topic->buf, sizeof(p_full_topic->buf), "%s", p_topic_str);
    }
}

bool
mqtt_publish_adv(const adv_report_t *const p_adv, const bool flag_use_timestamps, const time_t timestamp)
{
    cjson_wrap_str_t                 json_str    = cjson_wrap_str_null();
    const ruuvi_gw_cfg_coordinates_t coordinates = gw_cfg_get_coordinates();
    if (!mqtt_create_json_str(
            p_adv,
            flag_use_timestamps,
            timestamp,
            gw_cfg_get_nrf52_mac_addr(),
            coordinates.buf,
            &json_str))
    {
        LOG_ERR("%s failed", "mqtt_create_json_str");
        return false;
    }

    const mac_address_str_t          tag_mac_str = mac_address_to_str(&p_adv->tag_mac);
    const ruuvi_gw_cfg_mqtt_prefix_t mqtt_prefix = gw_cfg_get_mqtt_prefix();

    mqtt_protected_data_t *p_mqtt_data = mqtt_mutex_lock();
    if (NULL == p_mqtt_data->p_mqtt_client)
    {
        LOG_ERR("Can't send advs - MQTT was stopped");
        mqtt_mutex_unlock(&p_mqtt_data);
        cjson_wrap_free_json_str(&json_str);
        return false;
    }
    mqtt_create_full_topic(&p_mqtt_data->mqtt_topic, mqtt_prefix.buf, tag_mac_str.str_buf);

    LOG_DBG("publish: topic: %s, data: %s", p_mqtt_data->mqtt_topic.buf, json_str.p_str);
    const int32_t mqtt_len              = 0;
    const int32_t mqtt_qos              = 1;
    const int32_t mqtt_flag_retain      = 0;
    bool          is_publish_successful = false;

    if (esp_mqtt_client_publish(
            p_mqtt_data->p_mqtt_client,
            p_mqtt_data->mqtt_topic.buf,
            json_str.p_str,
            mqtt_len,
            mqtt_qos,
            mqtt_flag_retain)
        >= 0)
    {
        is_publish_successful = true;
    }
    mqtt_mutex_unlock(&p_mqtt_data);

    cjson_wrap_free_json_str(&json_str);
    return is_publish_successful;
}

void
mqtt_publish_connect(void)
{
    char *p_message = "{\"state\": \"online\"}";

    const ruuvi_gw_cfg_mqtt_prefix_t mqtt_prefix = gw_cfg_get_mqtt_prefix();

    mqtt_protected_data_t *p_mqtt_data = mqtt_mutex_lock();
    mqtt_create_full_topic(&p_mqtt_data->mqtt_topic, mqtt_prefix.buf, "gw_status");
    LOG_INFO("esp_mqtt_client_publish: topic:'%s', message:'%s'", p_mqtt_data->mqtt_topic.buf, p_message);
    const int32_t mqtt_qos         = 1;
    const int32_t mqtt_flag_retain = 1;

    const mqtt_message_id_t message_id = esp_mqtt_client_publish(
        p_mqtt_data->p_mqtt_client,
        p_mqtt_data->mqtt_topic.buf,
        p_message,
        (esp_mqtt_client_data_len_t)strlen(p_message),
        mqtt_qos,
        mqtt_flag_retain);

    mqtt_mutex_unlock(&p_mqtt_data);

    if (-1 == message_id)
    {
        LOG_ERR("esp_mqtt_client_publish failed");
    }
    else
    {
        LOG_INFO("esp_mqtt_client_publish: message_id=%d", message_id);
    }
}

static void
mqtt_publish_state_offline(mqtt_protected_data_t *const p_mqtt_data)
{
    char *p_message = "{\"state\": \"offline\"}";

    const ruuvi_gw_cfg_mqtt_prefix_t mqtt_prefix = gw_cfg_get_mqtt_prefix();

    mqtt_create_full_topic(&p_mqtt_data->mqtt_topic, mqtt_prefix.buf, "gw_status");
    LOG_INFO("esp_mqtt_client_publish: topic:'%s', message:'%s'", p_mqtt_data->mqtt_topic.buf, p_message);
    const int32_t mqtt_qos         = 1;
    const int32_t mqtt_flag_retain = 1;

    const mqtt_message_id_t message_id = esp_mqtt_client_publish(
        p_mqtt_data->p_mqtt_client,
        p_mqtt_data->mqtt_topic.buf,
        p_message,
        (esp_mqtt_client_data_len_t)strlen(p_message),
        mqtt_qos,
        mqtt_flag_retain);

    if (-1 == message_id)
    {
        LOG_ERR("esp_mqtt_client_publish failed");
    }
    else
    {
        LOG_INFO("esp_mqtt_client_publish: message_id=%d", message_id);
    }
}

static esp_err_t
mqtt_event_handler(esp_mqtt_event_handle_t h_event)
{
    switch (h_event->event_id)
    {
        case MQTT_EVENT_CONNECTED:
            LOG_INFO("MQTT_EVENT_CONNECTED");
            gw_status_set_mqtt_connected();
            main_task_send_sig_mqtt_publish_connect();
            leds_indication_on_network_ok();
            if (!fw_update_mark_app_valid_cancel_rollback())
            {
                LOG_ERR("%s failed", "fw_update_mark_app_valid_cancel_rollback");
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            LOG_INFO("MQTT_EVENT_DISCONNECTED");
            gw_status_clear_mqtt_connected();
            leds_indication_network_no_connection();
            break;

        case MQTT_EVENT_SUBSCRIBED:
            LOG_INFO("MQTT_EVENT_SUBSCRIBED, msg_id=%d", h_event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            LOG_INFO("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", h_event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            LOG_DBG("MQTT_EVENT_PUBLISHED, msg_id=%d", h_event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            LOG_INFO("MQTT_EVENT_DATA");
            break;

        case MQTT_EVENT_ERROR:
            LOG_INFO("MQTT_EVENT_ERROR");
            break;

        case MQTT_EVENT_BEFORE_CONNECT:
            LOG_INFO("MQTT_EVENT_BEFORE_CONNECT");
            break;

        default:
            LOG_INFO("Other event id:%d", h_event->event_id);
            break;
    }
    return ESP_OK;
}

static esp_mqtt_client_config_t
mqtt_generate_client_config(
    const ruuvi_gw_cfg_mqtt_t *const p_cfg_mqtt,
    const mqtt_topic_buf_t *const    p_mqtt_topic,
    const char *const                p_lwt_message,
    const esp_mqtt_transport_t       mqtt_transport)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle                = &mqtt_event_handler,
        .event_loop_handle           = NULL,
        .host                        = p_cfg_mqtt->mqtt_server.buf,
        .uri                         = NULL,
        .port                        = p_cfg_mqtt->mqtt_port,
        .client_id                   = p_cfg_mqtt->mqtt_client_id.buf,
        .username                    = p_cfg_mqtt->mqtt_user.buf,
        .password                    = p_cfg_mqtt->mqtt_pass.buf,
        .lwt_topic                   = p_mqtt_topic->buf,
        .lwt_msg                     = p_lwt_message,
        .lwt_qos                     = 1,
        .lwt_retain                  = true,
        .lwt_msg_len                 = 0,
        .disable_clean_session       = 0,
        .keepalive                   = 0,
        .disable_auto_reconnect      = false,
        .user_context                = NULL,
        .task_prio                   = 0,
        .task_stack                  = 0,
        .buffer_size                 = 0,
        .cert_pem                    = NULL,
        .cert_len                    = 0,
        .client_cert_pem             = NULL,
        .client_cert_len             = 0,
        .client_key_pem              = NULL,
        .client_key_len              = 0,
        .transport                   = mqtt_transport,
        .refresh_connection_after_ms = 0,
        .psk_hint_key                = NULL,
        .use_global_ca_store         = false,
        .crt_bundle_attach           = &esp_crt_bundle_attach,
        .reconnect_timeout_ms        = 0,
        .alpn_protos                 = NULL,
        .clientkey_password          = NULL,
        .clientkey_password_len      = 0,
        .protocol_ver                = MQTT_PROTOCOL_UNDEFINED,
        .out_buffer_size             = 0,
        .skip_cert_common_name_check = false,
        .use_secure_element          = false,
        .ds_data                     = NULL,
        .network_timeout_ms          = MQTT_NETWORK_TIMEOUT_MS,
        .disable_keepalive = false,
        .path              = NULL,
    };
    return mqtt_cfg;
}

static esp_mqtt_client_config_t
mqtt_prep_client_config(const ruuvi_gw_cfg_mqtt_t *const p_cfg_mqtt, mqtt_protected_data_t *const p_mqtt_data)
{
    mqtt_create_full_topic(&p_mqtt_data->mqtt_topic, p_cfg_mqtt->mqtt_prefix.buf, "gw_status");
    const char *p_lwt_message = "{\"state\": \"offline\"}";

    LOG_INFO(
        "Using server: %s, client id: '%s', topic prefix: '%s', port: %u, user: '%s', password: '%s'",
        p_cfg_mqtt->mqtt_server.buf,
        p_cfg_mqtt->mqtt_client_id.buf,
        p_cfg_mqtt->mqtt_prefix.buf,
        p_cfg_mqtt->mqtt_port,
        p_cfg_mqtt->mqtt_user.buf,
        "******");

    esp_mqtt_transport_t mqtt_transport = MQTT_TRANSPORT_OVER_TCP;
    if (0 == strcmp(p_cfg_mqtt->mqtt_transport.buf, MQTT_TRANSPORT_TCP))
    {
        mqtt_transport = MQTT_TRANSPORT_OVER_TCP;
    }
    else if (0 == strcmp(p_cfg_mqtt->mqtt_transport.buf, MQTT_TRANSPORT_SSL))
    {
        mqtt_transport = MQTT_TRANSPORT_OVER_SSL;
    }
    else if (0 == strcmp(p_cfg_mqtt->mqtt_transport.buf, MQTT_TRANSPORT_WS))
    {
        mqtt_transport = MQTT_TRANSPORT_OVER_WS;
    }
    else if (0 == strcmp(p_cfg_mqtt->mqtt_transport.buf, MQTT_TRANSPORT_WSS))
    {
        mqtt_transport = MQTT_TRANSPORT_OVER_WSS;
    }
    else
    {
        LOG_WARN("Unknown MQTT transport='%s', use TCP", p_cfg_mqtt->mqtt_transport.buf);
    }

    return mqtt_generate_client_config(p_cfg_mqtt, &p_mqtt_data->mqtt_topic, p_lwt_message, mqtt_transport);
}

static void
mqtt_app_start_internal(const esp_mqtt_client_config_t *const p_mqtt_cfg, mqtt_protected_data_t *const p_mqtt_data)
{
    p_mqtt_data->p_mqtt_client = esp_mqtt_client_init(p_mqtt_cfg);
    if (NULL == p_mqtt_data->p_mqtt_client)
    {
        LOG_ERR("%s failed", "esp_mqtt_client_init");
        return;
    }
    const esp_err_t err = esp_mqtt_client_start(p_mqtt_data->p_mqtt_client);
    if (ESP_OK != err)
    {
        esp_mqtt_client_destroy(p_mqtt_data->p_mqtt_client);
        p_mqtt_data->p_mqtt_client = NULL;
    }
    // TODO handle connection fails, wrong server, user, pass etc
}

void
mqtt_app_start(void)
{
    LOG_INFO("%s", __func__);

    mqtt_protected_data_t *p_mqtt_data = mqtt_mutex_lock();
    if (NULL != p_mqtt_data->p_mqtt_client)
    {
        LOG_INFO("MQTT client is already running");
    }
    else
    {
        const gw_cfg_t *p_gw_cfg = gw_cfg_lock_ro();

        if (('\0' == p_gw_cfg->ruuvi_cfg.mqtt.mqtt_server.buf[0]) || (0 == p_gw_cfg->ruuvi_cfg.mqtt.mqtt_port))
        {
            LOG_ERR(
                "Invalid MQTT parameters: server: %s, topic prefix: '%s', port: %u, user: '%s', password: '%s'",
                p_gw_cfg->ruuvi_cfg.mqtt.mqtt_server.buf,
                p_gw_cfg->ruuvi_cfg.mqtt.mqtt_prefix.buf,
                p_gw_cfg->ruuvi_cfg.mqtt.mqtt_port,
                p_gw_cfg->ruuvi_cfg.mqtt.mqtt_user.buf,
                "******");
            gw_cfg_unlock_ro(&p_gw_cfg);
        }
        else
        {
            const esp_mqtt_client_config_t mqtt_cli_cfg = mqtt_prep_client_config(
                &p_gw_cfg->ruuvi_cfg.mqtt,
                p_mqtt_data);
            gw_cfg_unlock_ro(&p_gw_cfg);

            mqtt_app_start_internal(&mqtt_cli_cfg, p_mqtt_data);
        }
    }

    mqtt_mutex_unlock(&p_mqtt_data);
}

void
mqtt_app_stop(void)
{
    LOG_INFO("%s", __func__);
    mqtt_protected_data_t *p_mqtt_data = mqtt_mutex_lock();
    if (NULL != p_mqtt_data->p_mqtt_client)
    {
        if (gw_status_is_mqtt_connected())
        {
            mqtt_publish_state_offline(p_mqtt_data);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        gw_status_clear_mqtt_connected();
        LOG_INFO("MQTT destroy");
        esp_mqtt_client_destroy(p_mqtt_data->p_mqtt_client);
        LOG_INFO("MQTT destroyed");
        p_mqtt_data->p_mqtt_client = NULL;
    }
    mqtt_mutex_unlock(&p_mqtt_data);
}

bool
mqtt_app_is_working(void)
{
    mqtt_protected_data_t *p_mqtt_data = mqtt_mutex_lock();
    const bool             is_working  = (NULL != p_mqtt_data->p_mqtt_client) ? true : false;
    mqtt_mutex_unlock(&p_mqtt_data);
    return is_working;
}
