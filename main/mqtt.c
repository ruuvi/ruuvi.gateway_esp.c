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

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define TOPIC_LEN 512

typedef int mqtt_message_id_t;

typedef struct mqtt_topic_buf_t
{
    char buf[TOPIC_LEN];
} mqtt_topic_buf_t;

static esp_mqtt_client_handle_t gp_mqtt_client = NULL;

static const char *TAG = "MQTT";

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

    if (NULL != p_prefix_str)
    {
        snprintf(p_full_topic->buf, sizeof(p_full_topic->buf), "%s/%s", p_prefix_str, p_topic_str);
    }
    else
    {
        snprintf(p_full_topic->buf, sizeof(p_full_topic->buf), "%s", p_topic_str);
    }
}

static void
mqtt_publish_adv(const adv_report_t *p_adv)
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!mqtt_create_json_str(p_adv, time(NULL), &g_gw_mac_sta_str, g_gateway_config.coordinates, &json_str))
    {
        LOG_ERR("%s failed", "mqtt_create_json_str");
        return;
    }
    const mac_address_str_t tag_mac_str = mac_address_to_str(&p_adv->tag_mac);
    mqtt_topic_buf_t        topic;
    mqtt_create_full_topic(&topic, g_gateway_config.mqtt.mqtt_prefix, tag_mac_str.str_buf);
    LOG_DBG("publish: topic: %s, data: %s", topic.buf, json_str.p_str);
    const int32_t mqtt_len         = 0;
    const int32_t mqtt_qos         = 1;
    const int32_t mqtt_flag_retain = 0;
    esp_mqtt_client_publish(gp_mqtt_client, topic.buf, json_str.p_str, mqtt_len, mqtt_qos, mqtt_flag_retain);
    cjson_wrap_free_json_str(&json_str);
}

void
mqtt_publish_table(const adv_report_table_t *p_table)
{
    LOG_INFO("sending advertisement table (%d items) to MQTT", p_table->num_of_advs);

    for (num_of_advs_t i = 0; i < p_table->num_of_advs; ++i)
    {
        const adv_report_t adv = p_table->table[i];
        mqtt_publish_adv(&adv);
    }
}

static void
mqtt_publish_connect(void)
{
    char *p_message = "{\"state\": \"online\"}";

    mqtt_topic_buf_t topic;
    mqtt_create_full_topic(&topic, g_gateway_config.mqtt.mqtt_prefix, "gw_status");
    LOG_INFO("esp_mqtt_client_publish: topic:'%s', message:'%s'", topic.buf, p_message);
    const int32_t mqtt_qos         = 1;
    const int32_t mqtt_flag_retain = 1;

    const mqtt_message_id_t message_id
        = esp_mqtt_client_publish(gp_mqtt_client, topic.buf, p_message, strlen(p_message), mqtt_qos, mqtt_flag_retain);
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
            break;

        case MQTT_EVENT_DISCONNECTED:
            LOG_INFO("MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(status_bits, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            LOG_INFO("MQTT_EVENT_SUBSCRIBED, msg_id=%d", h_event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            LOG_INFO("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", h_event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            LOG_INFO("MQTT_EVENT_PUBLISHED, msg_id=%d", h_event->msg_id);
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

void
mqtt_app_start(void)
{
    LOG_INFO("%s", __func__);

    if (NULL != gp_mqtt_client)
    {
        LOG_INFO("MQTT destroy");
        esp_mqtt_client_destroy(gp_mqtt_client);
        gp_mqtt_client = NULL;
        xEventGroupClearBits(status_bits, MQTT_CONNECTED_BIT);
    }

    mqtt_topic_buf_t lwt_topic_buf;
    mqtt_create_full_topic(&lwt_topic_buf, g_gateway_config.mqtt.mqtt_prefix, "gw_status");
    const char *p_lwt_message = "{\"state\": \"offline\"}";

    if ('\0' == g_gateway_config.mqtt.mqtt_server[0])
    {
        LOG_ERR(
            "Invalid MQTT parameters: server: %s, topic prefix: '%s', port: %u, user: '%s', password: '%s'",
            g_gateway_config.mqtt.mqtt_server,
            g_gateway_config.mqtt.mqtt_prefix,
            g_gateway_config.mqtt.mqtt_port,
            g_gateway_config.mqtt.mqtt_user,
            "******");
        return;
    }

    LOG_INFO(
        "Using server: %s, topic prefix: '%s', port: %u, user: '%s', password: '%s'",
        g_gateway_config.mqtt.mqtt_server,
        g_gateway_config.mqtt.mqtt_prefix,
        g_gateway_config.mqtt.mqtt_port,
        g_gateway_config.mqtt.mqtt_user,
        "******");

    const esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle                = &mqtt_event_handler,
        .event_loop_handle           = NULL,
        .host                        = g_gateway_config.mqtt.mqtt_server,
        .uri                         = NULL,
        .port                        = g_gateway_config.mqtt.mqtt_port,
        .client_id                   = g_gw_mac_sta_str.str_buf,
        .username                    = g_gateway_config.mqtt.mqtt_user,
        .password                    = g_gateway_config.mqtt.mqtt_pass,
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
    gp_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (NULL == gp_mqtt_client)
    {
        LOG_ERR("%s failed", "esp_mqtt_client_init");
        return;
    }
    esp_mqtt_client_start(gp_mqtt_client);
    // TODO handle connection fails, wrong server, user, pass etc
}
