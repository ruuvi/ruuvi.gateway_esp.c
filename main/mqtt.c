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

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define TOPIC_LEN 512

typedef int mqtt_message_id_t;

typedef struct mqtt_topic_buf_t
{
    char buf[TOPIC_LEN];
} mqtt_topic_buf_t;

static esp_mqtt_client_handle_t g_p_mqtt_client          = NULL;
static bool                     g_mqtt_mutex_initialized = false;
static os_mutex_t               g_mqtt_mutex;
static os_mutex_static_t        g_mqtt_mutex_mem;

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t
mqtt_mutex_lock(void)
{
    if (!g_mqtt_mutex_initialized)
    {
        g_mqtt_mutex             = os_mutex_create_static(&g_mqtt_mutex_mem);
        g_mqtt_mutex_initialized = true;
    }
    os_mutex_lock(g_mqtt_mutex);
    return g_p_mqtt_client;
}

static void
mqtt_mutex_unlock(esp_mqtt_client_handle_t *const p_client)
{
    *p_client = NULL;
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
        snprintf(p_full_topic->buf, sizeof(p_full_topic->buf), "%s/%s", p_prefix_str, p_topic_str);
    }
    else
    {
        snprintf(p_full_topic->buf, sizeof(p_full_topic->buf), "%s", p_topic_str);
    }
}

bool
mqtt_publish_adv(const adv_report_t *const p_adv)
{
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    cjson_wrap_str_t              json_str = cjson_wrap_str_null();
    if (!mqtt_create_json_str(p_adv, time(NULL), &g_gw_mac_sta_str, p_gw_cfg->coordinates.buf, &json_str))
    {
        LOG_ERR("%s failed", "mqtt_create_json_str");
        gw_cfg_unlock_ro(&p_gw_cfg);
        return false;
    }
    const mac_address_str_t tag_mac_str = mac_address_to_str(&p_adv->tag_mac);
    mqtt_topic_buf_t        topic;
    mqtt_create_full_topic(&topic, p_gw_cfg->mqtt.mqtt_prefix, tag_mac_str.str_buf);
    gw_cfg_unlock_ro(&p_gw_cfg);

    LOG_DBG("publish: topic: %s, data: %s", topic.buf, json_str.p_str);
    const int32_t mqtt_len              = 0;
    const int32_t mqtt_qos              = 1;
    const int32_t mqtt_flag_retain      = 0;
    bool          is_publish_successful = false;

    esp_mqtt_client_handle_t p_mqtt_client = mqtt_mutex_lock();
    if (esp_mqtt_client_publish(p_mqtt_client, topic.buf, json_str.p_str, mqtt_len, mqtt_qos, mqtt_flag_retain) >= 0)
    {
        is_publish_successful = true;
    }
    mqtt_mutex_unlock(&p_mqtt_client);

    cjson_wrap_free_json_str(&json_str);
    return is_publish_successful;
}

static void
mqtt_publish_connect(void)
{
    char *p_message = "{\"state\": \"online\"}";

    mqtt_topic_buf_t              topic    = { 0 };
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    mqtt_create_full_topic(&topic, p_gw_cfg->mqtt.mqtt_prefix, "gw_status");
    gw_cfg_unlock_ro(&p_gw_cfg);
    LOG_INFO("esp_mqtt_client_publish: topic:'%s', message:'%s'", topic.buf, p_message);
    const int32_t mqtt_qos         = 1;
    const int32_t mqtt_flag_retain = 1;

    esp_mqtt_client_handle_t p_mqtt_client = mqtt_mutex_lock();
    const mqtt_message_id_t  message_id    = esp_mqtt_client_publish(
        p_mqtt_client,
        topic.buf,
        p_message,
        (int)strlen(p_message),
        mqtt_qos,
        mqtt_flag_retain);
    mqtt_mutex_unlock(&p_mqtt_client);
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
mqtt_publish_state_offline(esp_mqtt_client_handle_t p_mqtt_client)
{
    char *p_message = "{\"state\": \"offline\"}";

    mqtt_topic_buf_t              topic;
    const ruuvi_gateway_config_t *p_gw_cfg = gw_cfg_lock_ro();
    mqtt_create_full_topic(&topic, p_gw_cfg->mqtt.mqtt_prefix, "gw_status");
    gw_cfg_unlock_ro(&p_gw_cfg);
    LOG_INFO("esp_mqtt_client_publish: topic:'%s', message:'%s'", topic.buf, p_message);
    const int32_t mqtt_qos         = 1;
    const int32_t mqtt_flag_retain = 1;

    const mqtt_message_id_t message_id = esp_mqtt_client_publish(
        p_mqtt_client,
        topic.buf,
        p_message,
        (int)strlen(p_message),
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
            xEventGroupSetBits(status_bits, MQTT_CONNECTED_BIT);
            mqtt_publish_connect();
            leds_indication_on_network_ok();
            if (!fw_update_mark_app_valid_cancel_rollback())
            {
                LOG_ERR("%s failed", "fw_update_mark_app_valid_cancel_rollback");
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            LOG_INFO("MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(status_bits, MQTT_CONNECTED_BIT);
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

static void
mqtt_app_start_internal(void)
{
    mqtt_topic_buf_t              lwt_topic_buf = { 0 };
    const ruuvi_gateway_config_t *p_gw_cfg      = gw_cfg_lock_ro();
    mqtt_create_full_topic(&lwt_topic_buf, p_gw_cfg->mqtt.mqtt_prefix, "gw_status");
    const char *p_lwt_message = "{\"state\": \"offline\"}";

    if ('\0' == p_gw_cfg->mqtt.mqtt_server[0])
    {
        LOG_ERR(
            "Invalid MQTT parameters: server: %s, topic prefix: '%s', port: %u, user: '%s', password: '%s'",
            p_gw_cfg->mqtt.mqtt_server,
            p_gw_cfg->mqtt.mqtt_prefix,
            p_gw_cfg->mqtt.mqtt_port,
            p_gw_cfg->mqtt.mqtt_user,
            "******");
        gw_cfg_unlock_ro(&p_gw_cfg);
        return;
    }

    LOG_INFO(
        "Using server: %s, client id: '%s', topic prefix: '%s', port: %u, user: '%s', password: '%s'",
        p_gw_cfg->mqtt.mqtt_server,
        p_gw_cfg->mqtt.mqtt_client_id,
        p_gw_cfg->mqtt.mqtt_prefix,
        p_gw_cfg->mqtt.mqtt_port,
        p_gw_cfg->mqtt.mqtt_user,
        "******");

    const esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle                = &mqtt_event_handler,
        .event_loop_handle           = NULL,
        .host                        = p_gw_cfg->mqtt.mqtt_server,
        .uri                         = NULL,
        .port                        = p_gw_cfg->mqtt.mqtt_port,
        .client_id                   = p_gw_cfg->mqtt.mqtt_client_id,
        .username                    = p_gw_cfg->mqtt.mqtt_user,
        .password                    = p_gw_cfg->mqtt.mqtt_pass,
        .lwt_topic                   = lwt_topic_buf.buf,
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
        .transport                   = MQTT_TRANSPORT_OVER_TCP,
        .refresh_connection_after_ms = 0,
        .psk_hint_key                = NULL,
        .use_global_ca_store         = false,
        .reconnect_timeout_ms        = 0,
        .alpn_protos                 = NULL,
        .clientkey_password          = NULL,
        .clientkey_password_len      = 0,
    };
    gw_cfg_unlock_rw(&p_gw_cfg);

    g_p_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (NULL == g_p_mqtt_client)
    {
        LOG_ERR("%s failed", "esp_mqtt_client_init");
        return;
    }
    esp_mqtt_client_start(g_p_mqtt_client);
    // TODO handle connection fails, wrong server, user, pass etc
}

void
mqtt_app_start(void)
{
    LOG_INFO("%s", __func__);

    esp_mqtt_client_handle_t p_mqtt_client = mqtt_mutex_lock();
    if (NULL != p_mqtt_client)
    {
        LOG_INFO("MQTT destroy");
        esp_mqtt_client_destroy(p_mqtt_client);
        g_p_mqtt_client = NULL;
        xEventGroupClearBits(status_bits, MQTT_CONNECTED_BIT);
    }

    mqtt_app_start_internal();

    mqtt_mutex_unlock(&p_mqtt_client);
}

void
mqtt_app_stop(void)
{
    LOG_INFO("%s", __func__);
    esp_mqtt_client_handle_t p_mqtt_client = mqtt_mutex_lock();
    if (NULL != p_mqtt_client)
    {
        mqtt_publish_state_offline(p_mqtt_client);
        LOG_INFO("MQTT destroy");
        esp_mqtt_client_destroy(p_mqtt_client);
        g_p_mqtt_client = NULL;
        xEventGroupClearBits(status_bits, MQTT_CONNECTED_BIT);
    }
    mqtt_mutex_unlock(&p_mqtt_client);
}
