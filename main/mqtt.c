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
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "ruuvi_gateway.h"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static esp_mqtt_client_handle_t mqtt_client = NULL;

#define TOPIC_LEN 512

static const char *TAG = "MQTT";

static void
create_full_topic(char *full_topic, const char *prefix, const char *topic)
{
    if ((NULL == full_topic) || (NULL == topic))
    {
        ESP_LOGE(TAG, "%s: null arguments", __func__);
        return;
    }

    if (NULL != prefix)
    {
        snprintf(full_topic, TOPIC_LEN, "%s/%s", prefix, topic);
    }
    else
    {
        snprintf(full_topic, TOPIC_LEN, "%s", topic);
    }
}

static char *
mqtt_create_json(adv_report_t *adv)
{
    time_t now = 0;
    time(&now);
    cJSON *root = cJSON_CreateObject();

    if (root)
    {
        cJSON_AddStringToObject(root, "gw_mac", gw_mac_sta.str_buf);
        cJSON_AddNumberToObject(root, "rssi", adv->rssi);
        cJSON_AddArrayToObject(root, "aoa");
        cjson_wrap_add_timestamp(root, "gwts", now);
        cjson_wrap_add_timestamp(root, "ts", adv->timestamp);
        cJSON_AddStringToObject(root, "data", adv->data);
        cJSON_AddStringToObject(root, "coords", g_gateway_config.coordinates);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

static void
mqtt_publish_adv(adv_report_t *adv)
{
    char                    topic[TOPIC_LEN];
    char *                  json        = mqtt_create_json(adv);
    const mac_address_str_t tag_mac_str = mac_address_to_str(&adv->tag_mac);
    create_full_topic(topic, g_gateway_config.mqtt_prefix, tag_mac_str.str_buf);
    ESP_LOGD(TAG, "publish: topic: %s, data: %s", topic, json);
    esp_mqtt_client_publish(mqtt_client, topic, json, 0, 1, 0);
    free(json);
}

void
mqtt_publish_table(const adv_report_table_t *table)
{
    ESP_LOGI(TAG, "sending advertisement table (%d items) to MQTT", table->num_of_advs);

    for (num_of_advs_t i = 0; i < table->num_of_advs; ++i)
    {
        adv_report_t adv = table->table[i];
        mqtt_publish_adv(&adv);
    }
}

static void
mqtt_publish_connect(void)
{
    char *message = "{\"state\": \"online\"}";
    char  topic[TOPIC_LEN];
    create_full_topic(topic, g_gateway_config.mqtt_prefix, "gw_status");
    ESP_LOGI(TAG, "esp_mqtt_client_publish: topic:'%s', message:'%s'", topic, message);
    const int message_id = esp_mqtt_client_publish(mqtt_client, topic, message, strlen(message), 1, 1);
    if (-1 == message_id)
    {
        ESP_LOGE(TAG, "esp_mqtt_client_publish failed");
    }
    else
    {
        ESP_LOGI(TAG, "esp_mqtt_client_publish: message_id=%d", message_id);
    }
}

static esp_err_t
mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(status_bits, MQTT_CONNECTED_BIT);
            mqtt_publish_connect();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(status_bits, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;

        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;

        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }

    return ESP_OK;
}

void
mqtt_app_start(void)
{
    ESP_LOGI(TAG, "%s", __func__);

    if (mqtt_client != NULL)
    {
        ESP_LOGI(TAG, "MQTT destroy");
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        xEventGroupClearBits(status_bits, MQTT_CONNECTED_BIT);
    }

    char lwt_topic[TOPIC_LEN];
    create_full_topic(lwt_topic, g_gateway_config.mqtt_prefix, "gw_status");
    char *    lwt_message = "{\"state\": \"offline\"}";
    esp_err_t err         = 0;

    if (0 == g_gateway_config.mqtt_server[0])
    {
        err = ESP_ERR_INVALID_ARG;
    }

    if (err)
    {
        ESP_LOGE(
            TAG,
            "Invalid MQTT parameters: server: %s, topic prefix: '%s', "
            "port: %u, user: '%s', password: '%s'",
            g_gateway_config.mqtt_server,
            g_gateway_config.mqtt_prefix,
            g_gateway_config.mqtt_port,
            g_gateway_config.mqtt_user,
            "******");
        return;
    }
    else
    {
        ESP_LOGI(
            TAG,
            "Using server: %s, topic prefix: '%s', port: %u, user: '%s', "
            "password: '%s'",
            g_gateway_config.mqtt_server,
            g_gateway_config.mqtt_prefix,
            g_gateway_config.mqtt_port,
            g_gateway_config.mqtt_user,
            "******");
    }

    const esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle                = &mqtt_event_handler,
        .event_loop_handle           = NULL,
        .host                        = g_gateway_config.mqtt_server,
        .uri                         = NULL,
        .port                        = g_gateway_config.mqtt_port,
        .client_id                   = NULL,
        .username                    = g_gateway_config.mqtt_user,
        .password                    = g_gateway_config.mqtt_pass,
        .lwt_topic                   = lwt_topic,
        .lwt_msg                     = lwt_message,
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
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);
    // TODO handle connection fails, wrong server, user, pass etc
}
