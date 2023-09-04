/**
 * @file gw_cfg_json_parse_mqtt.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_mqtt.h"
#include <string.h>
#include "gw_cfg_json_parse_internal.h"
#include "gw_cfg_default.h"

#if !defined(RUUVI_TESTS_HTTP_SERVER_CB)
#define RUUVI_TESTS_HTTP_SERVER_CB 0
#endif

#if !defined(RUUVI_TESTS_JSON_RUUVI)
#define RUUVI_TESTS_JSON_RUUVI 0
#endif

#if RUUVI_TESTS_HTTP_SERVER_CB || RUUVI_TESTS_JSON_RUUVI
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "gw_cfg";

static gw_cfg_mqtt_data_format_e
gw_cfg_json_parse_mqtt_data_format(const cJSON* const p_json_root)
{
    char data_format_str[GW_CFG_MQTT_DATA_FORMAT_STR_SIZE];
    if (!gw_cfg_json_copy_string_val(p_json_root, "mqtt_data_format", &data_format_str[0], sizeof(data_format_str)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_data_format");
        return GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW;
    }
    if (0 == strcmp(GW_CFG_MQTT_DATA_FORMAT_STR_RUUVI_RAW, data_format_str))
    {
        return GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW;
    }
    if (0 == strcmp(GW_CFG_MQTT_DATA_FORMAT_STR_RAW_AND_DECODED, data_format_str))
    {
        return GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED;
    }
    if (0 == strcmp(GW_CFG_MQTT_DATA_FORMAT_STR_DECODED, data_format_str))
    {
        return GW_CFG_MQTT_DATA_FORMAT_RUUVI_DECODED;
    }
    LOG_WARN("Unknown mqtt_data_format='%s', use 'ruuvi'", data_format_str);
    return GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW;
}

void
gw_cfg_json_parse_mqtt(const cJSON* const p_cjson, ruuvi_gw_cfg_mqtt_t* const p_mqtt)
{
    if (!gw_cfg_json_get_bool_val(p_cjson, "use_mqtt", &p_mqtt->use_mqtt))
    {
        LOG_WARN("Can't find key '%s' in config-json", "use_mqtt");
    }
    if (!gw_cfg_json_get_bool_val(p_cjson, "mqtt_disable_retained_messages", &p_mqtt->mqtt_disable_retained_messages))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_disable_retained_messages");
    }
    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "mqtt_transport",
            &p_mqtt->mqtt_transport.buf[0],
            sizeof(p_mqtt->mqtt_transport.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_transport");
    }
    p_mqtt->mqtt_data_format = gw_cfg_json_parse_mqtt_data_format(p_cjson);
    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "mqtt_server",
            &p_mqtt->mqtt_server.buf[0],
            sizeof(p_mqtt->mqtt_server.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_server");
    }
    if (!gw_cfg_json_get_uint16_val(p_cjson, "mqtt_port", &p_mqtt->mqtt_port))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_port");
    }
    if (!gw_cfg_json_get_uint32_val(p_cjson, "mqtt_sending_interval", &p_mqtt->mqtt_sending_interval))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_sending_interval");
    }

    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "mqtt_prefix",
            &p_mqtt->mqtt_prefix.buf[0],
            sizeof(p_mqtt->mqtt_prefix.buf)))
    {
        const ruuvi_gw_cfg_mqtt_t* const p_default_mqtt = gw_cfg_default_get_mqtt();
        p_mqtt->mqtt_prefix                             = p_default_mqtt->mqtt_prefix;
        LOG_WARN("Can't find key '%s' in config-json, use default value: %s", "mqtt_prefix", p_mqtt->mqtt_prefix.buf);
    }
    if ('\0' == p_mqtt->mqtt_prefix.buf[0])
    {
        const ruuvi_gw_cfg_mqtt_t* const p_default_mqtt = gw_cfg_default_get_mqtt();
        p_mqtt->mqtt_prefix                             = p_default_mqtt->mqtt_prefix;
        LOG_WARN("Key '%s' is empty in config-json, use default value: %s", "mqtt_prefix", p_mqtt->mqtt_prefix.buf);
    }
    if (!gw_cfg_json_copy_string_val(
            p_cjson,
            "mqtt_client_id",
            &p_mqtt->mqtt_client_id.buf[0],
            sizeof(p_mqtt->mqtt_client_id.buf)))
    {
        const ruuvi_gw_cfg_mqtt_t* const p_default_mqtt = gw_cfg_default_get_mqtt();
        p_mqtt->mqtt_client_id                          = p_default_mqtt->mqtt_client_id;
        LOG_WARN(
            "Can't find key '%s' in config-json, use default value: %s",
            "mqtt_client_id",
            p_mqtt->mqtt_client_id.buf);
    }
    if ('\0' == p_mqtt->mqtt_client_id.buf[0])
    {
        const ruuvi_gw_cfg_mqtt_t* const p_default_mqtt = gw_cfg_default_get_mqtt();
        p_mqtt->mqtt_client_id                          = p_default_mqtt->mqtt_client_id;
        LOG_WARN(
            "Key '%s' is empty in config-json, use default value: %s",
            "mqtt_client_id",
            p_mqtt->mqtt_client_id.buf);
    }
    if (!gw_cfg_json_copy_string_val(p_cjson, "mqtt_user", &p_mqtt->mqtt_user.buf[0], sizeof(p_mqtt->mqtt_user.buf)))
    {
        LOG_WARN("Can't find key '%s' in config-json", "mqtt_user");
    }
    if (!gw_cfg_json_copy_string_val(p_cjson, "mqtt_pass", &p_mqtt->mqtt_pass.buf[0], sizeof(p_mqtt->mqtt_pass.buf)))
    {
        LOG_INFO("Can't find key '%s' in config-json, leave the previous value unchanged", "mqtt_pass");
    }
    if (p_mqtt->use_mqtt)
    {
        if (!gw_cfg_json_get_bool_val(p_cjson, "mqtt_use_ssl_client_cert", &p_mqtt->use_ssl_client_cert))
        {
            LOG_WARN("Can't find key '%s' in config-json", "mqtt_use_ssl_client_cert");
        }
        if (!gw_cfg_json_get_bool_val(p_cjson, "mqtt_use_ssl_server_cert", &p_mqtt->use_ssl_server_cert))
        {
            LOG_WARN("Can't find key '%s' in config-json", "mqtt_use_ssl_server_cert");
        }
    }
}
