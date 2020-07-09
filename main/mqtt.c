#include "cJSON.h"
#include "esp_err.h"
#include "mqtt_client.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "ruuvidongle.h"
#include "mqtt.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static esp_mqtt_client_handle_t mqtt_client = NULL;

#define TOPIC_LEN 512

extern char gw_mac[13];

static const char * TAG = "MQTT";

static void create_full_topic (char * full_topic, char * prefix, char * topic)
{
    if (full_topic == NULL || topic == NULL || prefix == NULL)
    {
        ESP_LOGE (TAG, "%s: null arguments", __func__);
        return;
    }

    if (strlen (prefix) > 0)
    {
        snprintf (full_topic, TOPIC_LEN - 1, "%s%s", prefix, topic);
    }
    else
    {
        snprintf (full_topic, TOPIC_LEN - 1, "%s", topic);
    }
}

static char * mqtt_create_json (adv_report_t * adv)
{
    time_t now = 0;
    time (&now);
    cJSON * root = cJSON_CreateObject();

    if (root)
    {
        cJSON_AddStringToObject (root, "gwmac", gw_mac);
        cJSON_AddNumberToObject (root, "rssi", adv->rssi);
        cJSON_AddArrayToObject (root, "aoa");
        cJSON_AddNumberToObject (root, "gwts", now);
        cJSON_AddNumberToObject (root, "ts", adv->timestamp);
        cJSON_AddStringToObject (root, "data", adv->data);
        cJSON_AddStringToObject (root, "coords", m_dongle_config.coordinates);
    }

    char * json_str = cJSON_PrintUnformatted (root);
    cJSON_Delete (root);
    return json_str;
}

static void mqtt_publish_adv (adv_report_t * adv)
{
    char topic[TOPIC_LEN];
    char * json = mqtt_create_json (adv);
    create_full_topic (topic, m_dongle_config.mqtt_prefix, adv->tag_mac);
    ESP_LOGD (TAG, "publish: topic: %s, data: %s", topic, json);
    esp_mqtt_client_publish (mqtt_client, topic, json, 0, 1, 0);
    free (json);
}

void mqtt_publish_table (struct adv_report_table * table)
{
    ESP_LOGI (TAG, "sending advertisement table (%d items) to MQTT", table->num_of_advs);

    for (int i = 0; i < table->num_of_advs; i++)
    {
        adv_report_t adv = table->table[i];
        mqtt_publish_adv (&adv);
    }
}

static void mqtt_publish_connect()
{
    char * message = "{\"state\": \"online\"}";
    char topic[TOPIC_LEN];
    create_full_topic (topic, m_dongle_config.mqtt_prefix, gw_mac);
    ESP_LOGI(TAG, "esp_mqtt_client_publish: topic:'%s', message:'%s'", topic, message);
    const int message_id = esp_mqtt_client_publish (
            mqtt_client, topic, message, strlen (message), 1, 1);
    if (-1 == message_id)
    {
        ESP_LOGE(TAG, "esp_mqtt_client_publish failed");
    }
    else
    {
        ESP_LOGI(TAG, "esp_mqtt_client_publish: message_id=%d", message_id);
    }
}

static esp_err_t mqtt_event_handler (esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI (TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits (status_bits, MQTT_CONNECTED_BIT);
            mqtt_publish_connect();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI (TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits (status_bits, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI (TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI (TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI (TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI (TAG, "MQTT_EVENT_DATA");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI (TAG, "MQTT_EVENT_ERROR");
            break;

        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI (TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;

        default:
            ESP_LOGI (TAG, "Other event id:%d", event->event_id);
            break;
    }

    return ESP_OK;
}

void mqtt_app_start (void)
{
    ESP_LOGI (TAG, "%s", __func__);

    if (mqtt_client != NULL)
    {
        ESP_LOGI (TAG, "MQTT destroy");
        esp_mqtt_client_destroy (mqtt_client);
        mqtt_client = NULL;
        xEventGroupClearBits (status_bits, MQTT_CONNECTED_BIT);
    }

    char lwt_topic[TOPIC_LEN];
    create_full_topic (lwt_topic, m_dongle_config.mqtt_prefix, gw_mac);
    char * lwt_message = "{\"state\": \"offline\"}";
    esp_err_t err = 0;

    if (m_dongle_config.mqtt_server[0] == 0)
    {
        err = ESP_ERR_INVALID_ARG;
    }

    if (err)
    {
        ESP_LOGE (TAG,
                  "Invalid MQTT parameters: server: %s, topic prefix: '%s', port: %d, user: '%s', password: '%s'",
                  m_dongle_config.mqtt_server, m_dongle_config.mqtt_prefix, m_dongle_config.mqtt_port,
                  m_dongle_config.mqtt_user, m_dongle_config.mqtt_pass);
        return;
    }
    else
    {
        ESP_LOGI (TAG, "Using server: %s, topic prefix: '%s', port: %d, user: '%s', password: '%s'",
                  m_dongle_config.mqtt_server, m_dongle_config.mqtt_prefix, m_dongle_config.mqtt_port,
                  m_dongle_config.mqtt_user, m_dongle_config.mqtt_pass);
    }

    const esp_mqtt_client_config_t mqtt_cfg =
    {
        .event_handle = mqtt_event_handler,
        .host = m_dongle_config.mqtt_server,
        .port = m_dongle_config.mqtt_port,
        .username = m_dongle_config.mqtt_user,
        .password = m_dongle_config.mqtt_pass,
        .lwt_retain = true,
        .lwt_topic = lwt_topic,
        .lwt_msg = lwt_message,
        .lwt_qos = 1
    };
    mqtt_client = esp_mqtt_client_init (&mqtt_cfg);
    esp_mqtt_client_start (mqtt_client);
    //TODO handle connection fails, wrong server, user, pass etc
}
