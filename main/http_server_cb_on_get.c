/**
 * @file http_server_cb_on_get.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_cb.h"
#include <string.h>
#include <stdlib.h>
#include "os_malloc.h"
#include "gw_cfg_ruuvi_json.h"
#include "http_server_resp.h"
#include "reset_task.h"
#include "metrics.h"
#include "time_task.h"
#include "adv_post.h"
#include "flashfatfs.h"
#include "ruuvi_gateway.h"
#include "str_buf.h"
#include "gw_status.h"
#include "os_str.h"

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

#define HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS (60U)

#define MQTT_URL_PREFIX_LEN (20)

#define BASE_10 (10)

typedef double cjson_double_t;

typedef enum http_validate_type_e
{
    HTTP_VALIDATE_TYPE_INVALID,
    HTTP_VALIDATE_TYPE_POST_ADVS,
    HTTP_VALIDATE_TYPE_POST_STAT,
    HTTP_VALIDATE_TYPE_CHECK_MQTT,
    HTTP_VALIDATE_TYPE_CHECK_REMOTE_CFG,
    HTTP_VALIDATE_TYPE_CHECK_FILE,
} http_validate_type_e;

extern const flash_fat_fs_t* gp_ffs_gwui;

static const char TAG[] = "http_server";

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json_ruuvi(void)
{
    const gw_cfg_t*  p_gw_cfg = gw_cfg_lock_ro();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!gw_cfg_ruuvi_json_generate(p_gw_cfg, &json_str))
    {
        gw_cfg_unlock_ro(&p_gw_cfg);
        return http_server_resp_503();
    }
    gw_cfg_unlock_ro(&p_gw_cfg);

    LOG_INFO("ruuvi.json: %s", json_str.p_str);

    main_task_send_sig_activate_cfg_mode();

    return http_server_resp_200_json_in_heap(json_str.p_str);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json_github_latest_release(void)
{
    const http_server_download_info_t info = http_download_latest_release_info(true);
    if (info.is_error)
    {
        return http_server_resp_504();
    }

    LOG_DBG("github_latest_release.json: %s", info.p_json_buf);
    return http_server_resp_200_json_in_heap(info.p_json_buf);
}

static bool
json_info_add_string(cJSON* p_json_root, const char* p_item_name, const char* p_val)
{
    if (NULL == cJSON_AddStringToObject(p_json_root, p_item_name, p_val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
json_info_add_uint32(cJSON* p_json_root, const char* p_item_name, const uint32_t val)
{
    if (NULL == cJSON_AddNumberToObject(p_json_root, p_item_name, (cjson_double_t)val))
    {
        LOG_ERR("Can't add json item: %s", p_item_name);
        return false;
    }
    return true;
}

static bool
json_info_add_items(cJSON* p_json_root, const bool flag_use_timestamps)
{
    if (!json_info_add_string(p_json_root, "ESP_FW", gw_cfg_get_esp32_fw_ver()->buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "NRF_FW", gw_cfg_get_nrf52_fw_ver()->buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "DEVICE_ADDR", gw_cfg_get_nrf52_mac_addr()->str_buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "DEVICE_ID", gw_cfg_get_nrf52_device_id()->str_buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "ETHERNET_MAC", gw_cfg_get_esp32_mac_addr_eth()->str_buf))
    {
        return false;
    }
    if (!json_info_add_string(p_json_root, "WIFI_MAC", gw_cfg_get_esp32_mac_addr_wifi()->str_buf))
    {
        return false;
    }
    const time_t        cur_time  = http_server_get_cur_time();
    adv_report_table_t* p_reports = os_malloc(sizeof(*p_reports));
    if (NULL == p_reports)
    {
        return false;
    }
    const bool flag_use_filter = flag_use_timestamps;
    adv_table_history_read(
        p_reports,
        cur_time,
        flag_use_timestamps,
        flag_use_timestamps ? HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS : 0,
        flag_use_filter);
    const num_of_advs_t num_of_advs = p_reports->num_of_advs;
    os_free(p_reports);

    if (!json_info_add_uint32(p_json_root, "TAGS_SEEN", num_of_advs))
    {
        return false;
    }
    if (!json_info_add_uint32(p_json_root, "BUTTON_PRESSES", g_cnt_cfg_button_pressed))
    {
        return false;
    }
    return true;
}

static bool
generate_json_info_str(cjson_wrap_str_t* p_json_str, const bool flag_use_timestamps)
{
    p_json_str->p_str = NULL;

    cJSON* p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        LOG_ERR("Can't create json object");
        return false;
    }
    if (!json_info_add_items(p_json_root, flag_use_timestamps))
    {
        cjson_wrap_delete(&p_json_root);
        return false;
    }

    *p_json_str = cjson_wrap_print_and_delete(&p_json_root);
    if (NULL == p_json_str->p_str)
    {
        LOG_ERR("Can't create json string");
        return false;
    }
    return true;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json_info(void)
{
    const gw_cfg_t*  p_gw_cfg = gw_cfg_lock_ro();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!generate_json_info_str(&json_str, gw_cfg_get_ntp_use()))
    {
        gw_cfg_unlock_ro(&p_gw_cfg);
        return http_server_resp_503();
    }
    gw_cfg_unlock_ro(&p_gw_cfg);
    LOG_INFO("info.json: %s", json_str.p_str);
    return http_server_resp_200_json_in_heap(json_str.p_str);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json(const char* p_file_name, const bool flag_access_from_lan)
{
    if (0 == strcmp(p_file_name, "ruuvi.json"))
    {
        return http_server_resp_json_ruuvi();
    }
    if (0 == strcmp(p_file_name, "github_latest_release.json"))
    {
        return http_server_resp_json_github_latest_release();
    }
    if ((0 == strcmp(p_file_name, "info.json")) && (!flag_access_from_lan))
    {
        return http_server_resp_json_info();
    }
    LOG_WARN("Request to unknown json: %s", p_file_name);
    return http_server_resp_404();
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_metrics(void)
{
    const char* p_metrics = metrics_generate();
    if (NULL == p_metrics)
    {
        LOG_ERR("Not enough memory");
        return http_server_resp_503();
    }
    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    LOG_INFO("metrics: %s", p_metrics);
    return http_server_resp_200_data_in_heap(
        HTTP_CONENT_TYPE_TEXT_PLAIN,
        "version=0.0.4",
        strlen(p_metrics),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t*)p_metrics,
        flag_no_cache,
        flag_add_header_date);
}

HTTP_SERVER_CB_STATIC
void
http_server_get_filter_from_params(
    const char* const p_params,
    const bool        flag_use_timestamps,
    const bool        flag_time_is_synchronized,
    bool*             p_flag_use_filter,
    uint32_t*         p_filter)
{
    if (flag_use_timestamps)
    {
        str_buf_t str_buf = http_server_get_from_params(p_params, "time=");
        if (NULL != str_buf.buf)
        {
            *p_filter = (uint32_t)strtoul(str_buf.buf, NULL, 0);
            if (flag_time_is_synchronized)
            {
                *p_flag_use_filter = true;
            }
            str_buf_free_buf(&str_buf);
        }
    }
    else
    {
        str_buf_t str_buf = http_server_get_from_params(p_params, "counter=");
        if (NULL != str_buf.buf)
        {
            *p_filter          = (uint32_t)strtoul(str_buf.buf, NULL, 0);
            *p_flag_use_filter = true;
            str_buf_free_buf(&str_buf);
        }
    }
}

HTTP_SERVER_CB_STATIC
bool
http_server_get_decode_from_params(const char* const p_params)
{
    str_buf_t str_buf = http_server_get_from_params(p_params, "decode=");
    if (NULL == str_buf.buf)
    {
        return true;
    }
    bool flag_decode = true;
    if (0 == strcmp("false", str_buf.buf))
    {
        flag_decode = false;
    }
    str_buf_free_buf(&str_buf);
    return flag_decode;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_history(const char* const p_params)
{
    const bool flag_use_timestamps       = gw_cfg_get_ntp_use();
    const bool flag_time_is_synchronized = time_is_synchronized();
    uint32_t   filter                    = flag_use_timestamps ? HTTP_SERVER_DEFAULT_HISTORY_INTERVAL_SECONDS : 0;
    bool       flag_use_filter           = (flag_use_timestamps && flag_time_is_synchronized) ? true : false;

    bool flag_decode = true;
    if (NULL != p_params)
    {
        http_server_get_filter_from_params(
            p_params,
            flag_use_timestamps,
            flag_time_is_synchronized,
            &flag_use_filter,
            &filter);
        flag_decode = http_server_get_decode_from_params(p_params);
    }

    const time_t        cur_time  = http_server_get_cur_time();
    adv_report_table_t* p_reports = os_calloc(1, sizeof(*p_reports));
    if (NULL == p_reports)
    {
        return http_server_resp_503();
    }

    adv_table_history_read(p_reports, cur_time, flag_use_timestamps, filter, flag_use_filter);

    const bool      flag_use_nonce = false;
    const uint32_t  nonce          = 0;
    const gw_cfg_t* p_gw_cfg       = gw_cfg_lock_ro();

    const http_json_create_stream_gen_advs_params_t params = {
        .flag_decode         = flag_decode,
        .flag_use_timestamps = flag_use_timestamps,
        .cur_time            = http_server_get_cur_time(),
        .flag_use_nonce      = flag_use_nonce,
        .nonce               = nonce,
        .p_mac_addr          = gw_cfg_get_nrf52_mac_addr(),
        .p_coordinates       = &p_gw_cfg->ruuvi_cfg.coordinates,
    };
    json_stream_gen_t* p_gen = http_json_create_stream_gen_advs(p_reports, &params);
    gw_cfg_unlock_ro(&p_gw_cfg);
    os_free(p_reports);
    if (NULL == p_gen)
    {
        return http_server_resp_503();
    }

    adv_post_last_successful_network_comm_timestamp_update();
    main_task_on_get_history();

    if (flag_use_filter)
    {
        if (flag_use_timestamps)
        {
            LOG_INFO("Requested /history on %u seconds interval", (unsigned)filter);
        }
        else
        {
            LOG_INFO("Requested /history starting from counter %u", (unsigned)filter);
        }
    }
    else
    {
        LOG_INFO("Requested /history (without filtering)");
    }

    return http_server_resp_200_json_generator(p_gen);
}

HTTP_SERVER_CB_STATIC
bool
http_server_get_bool_from_params(const char* const p_params, const char* const p_key)
{
    str_buf_t str_val = http_server_get_from_params_with_decoding(p_params, p_key);
    if (NULL == str_val.buf)
    {
        LOG_ERR("Can't find key: %s", p_key);
        return false;
    }
    bool res = false;
    if (0 == strcmp(str_val.buf, "true"))
    {
        res = true;
    }
    else if (0 == strcmp(str_val.buf, "false"))
    {
        res = false;
    }
    else
    {
        LOG_ERR("Incorrect bool value for key '%s': %s", p_key, str_val.buf);
    }
    str_buf_free_buf(&str_val);
    return res;
}

HTTP_SERVER_CB_STATIC
str_buf_t
http_server_get_password_from_params(const char* const p_params)
{
    str_buf_t encrypted_password = http_server_get_from_params_with_decoding(p_params, "encrypted_password=");
    if (NULL == encrypted_password.buf)
    {
        return str_buf_init_null();
    }
    str_buf_t encrypted_password_iv = http_server_get_from_params_with_decoding(p_params, "encrypted_password_iv=");
    if (NULL == encrypted_password_iv.buf)
    {
        str_buf_free_buf(&encrypted_password);
        return str_buf_init_null();
    }
    str_buf_t encrypted_password_hash = http_server_get_from_params_with_decoding(p_params, "encrypted_password_hash=");
    if (NULL == encrypted_password_hash.buf)
    {
        str_buf_free_buf(&encrypted_password);
        str_buf_free_buf(&encrypted_password_iv);
        return str_buf_init_null();
    }

    str_buf_t decrypted_password = str_buf_init_null();
    if (!http_server_decrypt_by_params(
            encrypted_password.buf,
            encrypted_password_iv.buf,
            encrypted_password_hash.buf,
            &decrypted_password))
    {
        LOG_WARN("Failed to decrypt password: %s", encrypted_password.buf);
    }
    else
    {
        LOG_DBG("HTTP params: Decrypted password: %s", decrypted_password.buf);
    }
    str_buf_free_buf(&encrypted_password);
    str_buf_free_buf(&encrypted_password_iv);
    str_buf_free_buf(&encrypted_password_hash);
    return decrypted_password;
}

HTTP_SERVER_CB_STATIC
http_validate_type_e
http_server_get_validate_type_from_params(const char* const p_params)
{
    const char* const p_prefix   = "validate_type=";
    const size_t      prefix_len = strlen(p_prefix);
    const char* const p_param    = strstr(p_params, p_prefix);
    if (NULL == p_param)
    {
        LOG_ERR("Can't find prefix '%s'", p_prefix);
        return HTTP_VALIDATE_TYPE_INVALID;
    }
    const char* const p_value = &p_param[prefix_len];
    const char* const p_end   = strchr(p_value, '&');
    const size_t      val_len = (NULL == p_end) ? strlen(p_value) : (size_t)(p_end - p_value);
    LOG_INFO("Found validate_type: %.*s", val_len, p_value);
    if (0 == strncmp(p_value, "check_post_advs", val_len))
    {
        return HTTP_VALIDATE_TYPE_POST_ADVS;
    }
    if (0 == strncmp(p_value, "check_post_stat", val_len))
    {
        return HTTP_VALIDATE_TYPE_POST_STAT;
    }
    if (0 == strncmp(p_value, "check_mqtt", val_len))
    {
        return HTTP_VALIDATE_TYPE_CHECK_MQTT;
    }
    if (0 == strncmp(p_value, "check_remote_cfg", val_len))
    {
        return HTTP_VALIDATE_TYPE_CHECK_REMOTE_CFG;
    }
    if (0 == strncmp(p_value, "check_file", val_len))
    {
        return HTTP_VALIDATE_TYPE_CHECK_FILE;
    }
    LOG_ERR("Unknown validate_type: %.*s", val_len, p_value);
    return HTTP_VALIDATE_TYPE_INVALID;
}

HTTP_SERVER_CB_STATIC
bool
http_server_get_use_ssl_client_cert(const char* const p_params)
{
    const char* const p_prefix   = "use_ssl_client_cert=";
    const size_t      prefix_len = strlen(p_prefix);
    const char* const p_param    = strstr(p_params, p_prefix);
    if (NULL == p_param)
    {
        LOG_ERR("Can't find prefix '%s'", p_prefix);
        return false;
    }
    const char* const p_value = &p_param[prefix_len];
    const char* const p_end   = strchr(p_value, '&');
    const size_t      val_len = (NULL == p_end) ? strlen(p_value) : (size_t)(p_end - p_value);
    LOG_INFO("Found use_ssl_client_cert: %.*s", val_len, p_value);
    if (0 == strncmp(p_value, "true", val_len))
    {
        return true;
    }
    return false;
}

HTTP_SERVER_CB_STATIC
bool
http_server_get_use_ssl_server_cert(const char* const p_params)
{
    const char* const p_prefix   = "use_ssl_server_cert=";
    const size_t      prefix_len = strlen(p_prefix);
    const char* const p_param    = strstr(p_params, p_prefix);
    if (NULL == p_param)
    {
        LOG_ERR("Can't find prefix '%s'", p_prefix);
        return false;
    }
    const char* const p_value = &p_param[prefix_len];
    const char* const p_end   = strchr(p_value, '&');
    const size_t      val_len = (NULL == p_end) ? strlen(p_value) : (size_t)(p_end - p_value);
    LOG_INFO("Found use_ssl_server_cert: %.*s", val_len, p_value);
    if (0 == strncmp(p_value, "true", val_len))
    {
        return true;
    }
    return false;
}

HTTP_SERVER_CB_STATIC
bool
http_server_parse_mqtt_url(const char* const p_url, ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg)
{
    LOG_DBG("Parse MQTT URL: %s", p_url);
    const char* const p_prefix_separator_str = "://";
    const size_t      prefix_separator_len   = strlen(p_prefix_separator_str);
    const char* const p_prefix               = p_url;
    const char* const p_prefix_separator     = strstr(p_prefix, p_prefix_separator_str);
    if (NULL == p_prefix_separator)
    {
        LOG_ERR("Can't find MQTT protocol prefix separator in URL: %s", p_url);
        return false;
    }
    char               prefix_buf[MQTT_URL_PREFIX_LEN];
    str_buf_t          prefix       = STR_BUF_INIT(prefix_buf, sizeof(prefix_buf));
    const printf_int_t prefix_width = (p_prefix_separator - p_prefix) + prefix_separator_len;
    str_buf_printf(&prefix, "%.*s", prefix_width, p_url);
    const char* p_mqtt_transport = NULL;
    if (0 == strcmp("mqtt://", prefix_buf))
    {
        p_mqtt_transport = MQTT_TRANSPORT_TCP;
    }
    else if (0 == strcmp("mqtts://", prefix_buf))
    {
        p_mqtt_transport = MQTT_TRANSPORT_SSL;
    }
    else if (0 == strcmp("mqttws://", prefix_buf))
    {
        p_mqtt_transport = MQTT_TRANSPORT_WS;
    }
    else if (0 == strcmp("mqttwss://", prefix_buf))
    {
        p_mqtt_transport = MQTT_TRANSPORT_WSS;
    }
    else
    {
        LOG_ERR("Unknown MQTT protocol in URL: %s", p_url);
        return false;
    }
    (void)snprintf(p_mqtt_cfg->mqtt_transport.buf, sizeof(p_mqtt_cfg->mqtt_transport.buf), "%s", p_mqtt_transport);
    LOG_DBG("MQTT transport: %s", p_mqtt_cfg->mqtt_transport.buf);

    const char* const p_server = p_prefix_separator + prefix_separator_len;

    const char* const p_port_separator = strchr(p_server, ':');
    if (NULL == p_port_separator)
    {
        LOG_ERR("Can't find MQTT port separator in URL: %s", p_url);
        return false;
    }
    const size_t server_len = (size_t)(p_port_separator - p_server);
    if (server_len >= sizeof(p_mqtt_cfg->mqtt_server.buf))
    {
        LOG_ERR("Server name in too long in MQTT URL: %s", p_url);
        return false;
    }
    (void)snprintf(
        p_mqtt_cfg->mqtt_server.buf,
        sizeof(p_mqtt_cfg->mqtt_server.buf),
        "%.*s",
        (printf_int_t)server_len,
        p_server);
    LOG_DBG("MQTT server: %s", p_mqtt_cfg->mqtt_server.buf);

    const char* const p_port = p_port_separator + 1;

    const char* p_end     = NULL;
    uint32_t    mqtt_port = os_str_to_uint32_cptr(p_port, &p_end, BASE_10);
    if ('\0' != *p_end)
    {
        LOG_ERR("Incorrect MQTT port in URL: %s", p_url);
        return false;
    }
    if ((0 == mqtt_port) || (mqtt_port > UINT16_MAX))
    {
        LOG_ERR("MQTT port is out of range in URL: %s", p_url);
        return false;
    }

    p_mqtt_cfg->mqtt_port = (uint16_t)mqtt_port;
    LOG_DBG("MQTT port: %u", (printf_uint_t)p_mqtt_cfg->mqtt_port);

    return true;
}

HTTP_SERVER_CB_STATIC
bool
http_server_get_mqtt_disable_retained_messages(const char* const p_params)
{
    const char* const p_prefix   = "mqtt_disable_retained_messages=";
    const size_t      prefix_len = strlen(p_prefix);
    const char* const p_param    = strstr(p_params, p_prefix);
    if (NULL == p_param)
    {
        LOG_ERR("Can't find prefix '%s'", p_prefix);
        return false;
    }
    const char* const p_value = &p_param[prefix_len];
    const char* const p_end   = strchr(p_value, '&');
    const size_t      val_len = (NULL == p_end) ? strlen(p_value) : (size_t)(p_end - p_value);
    LOG_INFO("Found mqtt_disable_retained_messages: %.*s", val_len, p_value);
    if (0 == strncmp(p_value, "true", val_len))
    {
        return true;
    }
    return false;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_on_get_check_mqtt(
    const http_check_params_t* const p_params,
    const char* const                p_url_params,
    const TimeUnitsSeconds_t         timeout_seconds)
{
    str_buf_t topic_prefix = http_server_get_from_params_with_decoding(p_url_params, "mqtt_topic_prefix=");
    if (NULL == topic_prefix.buf)
    {
        return http_server_resp_400();
    }
    str_buf_t client_id = http_server_get_from_params_with_decoding(p_url_params, "mqtt_client_id=");
    if (NULL == client_id.buf)
    {
        str_buf_free_buf(&topic_prefix);
        return http_server_resp_400();
    }

    ruuvi_gw_cfg_mqtt_t* p_mqtt_cfg = os_calloc(1, sizeof(*p_mqtt_cfg));
    if (NULL == p_mqtt_cfg)
    {
        str_buf_free_buf(&client_id);
        str_buf_free_buf(&topic_prefix);
        LOG_ERR("Can't allocate memory for mqtt_cfg");
        return http_server_resp_500();
    }

    if ((strlen(topic_prefix.buf) >= sizeof(p_mqtt_cfg->mqtt_prefix.buf))
        || (strlen(client_id.buf) >= sizeof(p_mqtt_cfg->mqtt_client_id.buf))
        || (strlen(p_params->p_user) >= sizeof(p_mqtt_cfg->mqtt_user.buf))
        || (strlen(p_params->p_pass) >= sizeof(p_mqtt_cfg->mqtt_pass.buf)))
    {
        str_buf_free_buf(&client_id);
        str_buf_free_buf(&topic_prefix);
        os_free(p_mqtt_cfg);
        return http_server_resp_400();
    }

    if (!http_server_parse_mqtt_url(p_params->p_url, p_mqtt_cfg))
    {
        str_buf_free_buf(&client_id);
        str_buf_free_buf(&topic_prefix);
        os_free(p_mqtt_cfg);
        return http_server_resp_400();
    }
    const bool flag_disable_retained_messages = http_server_get_mqtt_disable_retained_messages(p_url_params);

    p_mqtt_cfg->use_mqtt                       = true;
    p_mqtt_cfg->mqtt_disable_retained_messages = flag_disable_retained_messages;
    p_mqtt_cfg->use_ssl_client_cert            = p_params->use_ssl_client_cert;
    p_mqtt_cfg->use_ssl_server_cert            = p_params->use_ssl_server_cert;

    (void)snprintf(p_mqtt_cfg->mqtt_prefix.buf, sizeof(p_mqtt_cfg->mqtt_prefix.buf), "%s", topic_prefix.buf);
    (void)snprintf(p_mqtt_cfg->mqtt_client_id.buf, sizeof(p_mqtt_cfg->mqtt_client_id.buf), "%s", client_id.buf);
    (void)snprintf(
        p_mqtt_cfg->mqtt_user.buf,
        sizeof(p_mqtt_cfg->mqtt_user.buf),
        "%s",
        (NULL != p_params->p_user) ? p_params->p_user : "");
    (void)snprintf(
        p_mqtt_cfg->mqtt_pass.buf,
        sizeof(p_mqtt_cfg->mqtt_pass.buf),
        "%s",
        (NULL != p_params->p_pass) ? p_params->p_pass : "");

    const http_server_resp_t http_resp = http_check_mqtt(p_mqtt_cfg, timeout_seconds);

    os_free(p_mqtt_cfg);
    str_buf_free_buf(&client_id);
    str_buf_free_buf(&topic_prefix);

    return http_resp;
}

HTTP_SERVER_CB_STATIC
bool
gw_cfg_remote_set_auth_basic(
    ruuvi_gw_cfg_remote_t* const p_remote_cfg,
    const char* const            p_user,
    const char* const            p_pass)
{
    if (strlen(p_user) >= sizeof(p_remote_cfg->auth.auth_basic.user.buf))
    {
        LOG_ERR("remote_cfg username is too long: %s", p_user);
        return false;
    }
    if (strlen(p_pass) >= sizeof(p_remote_cfg->auth.auth_basic.password.buf))
    {
        LOG_ERR("remote_cfg password is too long: %s", p_pass);
        return false;
    }
    (void)
        snprintf(p_remote_cfg->auth.auth_basic.user.buf, sizeof(p_remote_cfg->auth.auth_basic.user.buf), "%s", p_user);
    (void)snprintf(
        p_remote_cfg->auth.auth_basic.password.buf,
        sizeof(p_remote_cfg->auth.auth_basic.password.buf),
        "%s",
        p_pass);
    return true;
}

HTTP_SERVER_CB_STATIC
bool
gw_cfg_remote_set_auth_bearer(ruuvi_gw_cfg_remote_t* const p_remote_cfg, const char* const p_token)
{
    if (strlen(p_token) >= sizeof(p_remote_cfg->auth.auth_bearer.token.buf))
    {
        LOG_ERR("remote_cfg token is too long: %s", p_token);
        return false;
    }
    (void)snprintf(
        p_remote_cfg->auth.auth_bearer.token.buf,
        sizeof(p_remote_cfg->auth.auth_bearer.token.buf),
        "%s",
        p_token);
    return true;
}

HTTP_SERVER_CB_STATIC
bool
gw_cfg_remote_set_auth_token(ruuvi_gw_cfg_remote_t* const p_remote_cfg, const char* const p_token)
{
    if (strlen(p_token) >= sizeof(p_remote_cfg->auth.auth_token.token.buf))
    {
        LOG_ERR("remote_cfg token is too long: %s", p_token);
        return false;
    }
    (void)snprintf(
        p_remote_cfg->auth.auth_token.token.buf,
        sizeof(p_remote_cfg->auth.auth_token.token.buf),
        "%s",
        p_token);
    return true;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_on_get_check_remote_cfg(const http_check_params_t* const p_params)
{
    ruuvi_gw_cfg_remote_t* p_remote_cfg = os_calloc(1, sizeof(*p_remote_cfg));
    if (NULL == p_remote_cfg)
    {
        LOG_ERR("Can't allocate memory for remote_cfg");
        return http_server_resp_500();
    }
    p_remote_cfg->use_remote_cfg           = true;
    p_remote_cfg->use_ssl_client_cert      = p_params->use_ssl_client_cert;
    p_remote_cfg->use_ssl_server_cert      = p_params->use_ssl_server_cert;
    p_remote_cfg->refresh_interval_minutes = 0;
    (void)snprintf(p_remote_cfg->url.buf, sizeof(p_remote_cfg->url.buf), "%s", p_params->p_url);
    p_remote_cfg->auth_type = p_params->auth_type;
    bool res                = false;
    switch (p_params->auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            res = true;
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            res = gw_cfg_remote_set_auth_basic(p_remote_cfg, p_params->p_user, p_params->p_pass);
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            res = gw_cfg_remote_set_auth_bearer(p_remote_cfg, p_params->p_pass);
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            res = gw_cfg_remote_set_auth_token(p_remote_cfg, p_params->p_pass);
            break;
    }
    if (!res)
    {
        os_free(p_remote_cfg);
        return http_server_resp_400();
    }

    const bool             flag_free_memory = true;
    gw_cfg_t*              p_gw_cfg_tmp     = NULL;
    str_buf_t              err_msg          = str_buf_init_null();
    const http_resp_code_e http_resp_code   = http_server_gw_cfg_download_and_parse(
        p_remote_cfg,
        flag_free_memory,
        &p_gw_cfg_tmp,
        &err_msg);
    os_free(p_remote_cfg);
    if (NULL != p_gw_cfg_tmp)
    {
        os_free(p_gw_cfg_tmp);
    }
    const http_server_resp_t resp = http_server_cb_gen_resp(
        http_resp_code,
        "%s",
        (NULL != err_msg.buf) ? err_msg.buf : "");
    if (NULL != err_msg.buf)
    {
        str_buf_free_buf(&err_msg);
    }
    return resp;
}

static bool
http_server_check_url(const str_buf_t* const p_url)
{
    struct http_parser_url* p_parser_url = os_calloc(1, sizeof(*p_parser_url));
    if (NULL == p_parser_url)
    {
        LOG_ERR("Can't allocate memory");
        return false;
    }
    http_parser_url_init(p_parser_url);
    const printf_int_t parser_status = http_parser_parse_url(p_url->buf, strlen(p_url->buf), 0, p_parser_url);
    os_free(p_parser_url);
    if (0 != parser_status)
    {
        LOG_ERR("Incorrect URL: %s", p_url->buf);
        return false;
    }
    return true;
}

typedef struct http_server_resp_validate_params_t
{
    const str_buf_t* const        p_url;
    const gw_cfg_http_auth_type_e auth_type;
    const str_buf_t* const        p_user;
    const str_buf_t* const        p_password;
    const bool                    flag_use_saved_password;
    const bool                    use_ssl_client_cert;
    const bool                    use_ssl_server_cert;
} http_server_resp_validate_params_t;

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_validate_post_advs(const http_server_resp_validate_params_t* const p_params)
{
    if (!http_server_check_url(p_params->p_url))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Incorrect URL: '%s'", p_params->p_url->buf);
    }

    str_buf_t saved_password = gw_cfg_get_http_password_copy();
    if (NULL == saved_password.buf)
    {
        LOG_ERR("Can't allocate memory for saved http_password");
        return http_server_resp_500();
    }
    const http_check_params_t params = {
        .p_url               = p_params->p_url->buf,
        .auth_type           = p_params->auth_type,
        .p_user              = p_params->p_user->buf,
        .p_pass              = p_params->flag_use_saved_password ? saved_password.buf : p_params->p_password->buf,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };
    const http_server_resp_t http_resp = http_check_post_advs(&params, HTTP_DOWNLOAD_TIMEOUT_SECONDS);
    str_buf_free_buf(&saved_password);
    return http_resp;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_validate_post_stat(const http_server_resp_validate_params_t* const p_params)
{
    if (!http_server_check_url(p_params->p_url))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Incorrect URL: '%s'", p_params->p_url->buf);
    }

    str_buf_t saved_password = gw_cfg_get_http_stat_password_copy();
    if (NULL == saved_password.buf)
    {
        LOG_ERR("Can't allocate memory for saved http_stat_password");
        return http_server_resp_500();
    }
    const char* const p_pass = p_params->flag_use_saved_password ? saved_password.buf : p_params->p_password->buf;

    const http_check_params_t params = {
        .p_url               = p_params->p_url->buf,
        .auth_type           = p_params->auth_type,
        .p_user              = (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) ? "" : p_params->p_user->buf,
        .p_pass              = (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) ? "" : p_pass,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };

    const http_server_resp_t http_resp = http_check_post_stat(&params, HTTP_DOWNLOAD_TIMEOUT_SECONDS);
    str_buf_free_buf(&saved_password);
    return http_resp;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_validate_check_mqtt(
    const http_server_resp_validate_params_t* const p_params,
    const char* const                               p_url_params)
{
    str_buf_t saved_password = gw_cfg_get_mqtt_password_copy();
    if (NULL == saved_password.buf)
    {
        LOG_ERR("Can't allocate memory for saved mqtt_password");
        return http_server_resp_500();
    }

    const char* const p_pass = p_params->flag_use_saved_password ? saved_password.buf : p_params->p_password->buf;
    const http_check_params_t params = {
        .p_url               = p_params->p_url->buf,
        .auth_type           = p_params->auth_type,
        .p_user              = (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) ? "" : p_params->p_user->buf,
        .p_pass              = (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) ? "" : p_pass,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };
    const http_server_resp_t http_resp = http_server_on_get_check_mqtt(
        &params,
        p_url_params,
        HTTP_DOWNLOAD_CHECK_MQTT_TIMEOUT_SECONDS);
    str_buf_free_buf(&saved_password);
    return http_resp;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_validate_check_remote_cfg(const http_server_resp_validate_params_t* const p_params)
{
    if (!http_server_check_url(p_params->p_url))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Incorrect URL: '%s'", p_params->p_url->buf);
    }

    const ruuvi_gw_cfg_remote_t* p_saved_remote_cfg = gw_cfg_get_remote_cfg_copy();
    if (NULL == p_saved_remote_cfg)
    {
        LOG_ERR("Can't allocate memory for copy of remote_cfg");
        return http_server_resp_500();
    }
    const char* p_saved_password = NULL;
    switch (p_params->auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            p_saved_password = p_saved_remote_cfg->auth.auth_basic.password.buf;
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            p_saved_password = p_saved_remote_cfg->auth.auth_bearer.token.buf;
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            p_saved_password = p_saved_remote_cfg->auth.auth_token.token.buf;
            break;
    }
    os_free(p_saved_remote_cfg);
    const http_check_params_t params = {
        .p_url               = p_params->p_url->buf,
        .auth_type           = p_params->auth_type,
        .p_user              = p_params->p_user->buf,
        .p_pass              = p_params->flag_use_saved_password ? p_saved_password : p_params->p_password->buf,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };
    const http_server_resp_t http_resp = http_server_on_get_check_remote_cfg(&params);
    return http_resp;
}

static bool
http_server_resp_validate_check_file_prep_auth_basic(
    const http_server_resp_validate_params_t* const p_params,
    ruuvi_gw_cfg_http_auth_t* const                 p_http_auth)
{
    if (('\0' == p_params->p_user->buf[0]) || ('\0' == p_params->p_password->buf[0]))
    {
        LOG_ERR("Username or password is empty");
        return false;
    }
    ruuvi_gw_cfg_http_auth_basic_t* const p_auth = &p_http_auth->auth_basic;
    (void)snprintf(p_auth->user.buf, sizeof(p_auth->user.buf), "%s", p_params->p_user->buf);
    (void)snprintf(p_auth->password.buf, sizeof(p_auth->password.buf), "%s", p_params->p_password->buf);
    return true;
}

static bool
http_server_resp_validate_check_file_prep_auth_bearer(
    const http_server_resp_validate_params_t* const p_params,
    ruuvi_gw_cfg_http_auth_t* const                 p_http_auth)
{
    if ('\0' == p_params->p_password->buf[0])
    {
        LOG_ERR("Token is empty");
        return false;
    }
    ruuvi_gw_cfg_http_bearer_token_t* const p_bearer = &p_http_auth->auth_bearer.token;
    (void)snprintf(p_bearer->buf, sizeof(p_bearer->buf), "%s", p_params->p_password->buf);
    return true;
}

static bool
http_server_resp_validate_check_file_prep_auth_token(
    const http_server_resp_validate_params_t* const p_params,
    ruuvi_gw_cfg_http_auth_t* const                 p_http_auth)
{
    if ('\0' == p_params->p_password->buf[0])
    {
        LOG_ERR("Token is empty");
        return false;
    }
    ruuvi_gw_cfg_http_token_t* const p_token = &p_http_auth->auth_token.token;
    (void)snprintf(p_token->buf, sizeof(p_token->buf), "%s", p_params->p_password->buf);
    return true;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_validate_check_file(const http_server_resp_validate_params_t* const p_params)
{
    ruuvi_gw_cfg_http_auth_t* p_http_auth = NULL;
    if (GW_CFG_HTTP_AUTH_TYPE_NONE != p_params->auth_type)
    {
        p_http_auth = os_calloc(1, sizeof(*p_http_auth));
        if (NULL == p_http_auth)
        {
            LOG_ERR("Can't allocate memory for http_auth");
            return http_server_resp_500();
        }
        switch (p_params->auth_type)
        {
            case GW_CFG_HTTP_AUTH_TYPE_NONE:
                break;
            case GW_CFG_HTTP_AUTH_TYPE_BASIC:
                if (!http_server_resp_validate_check_file_prep_auth_basic(p_params, p_http_auth))
                {
                    os_free(p_http_auth);
                    return http_server_resp_400();
                }
                break;
            case GW_CFG_HTTP_AUTH_TYPE_BEARER:
                if (!http_server_resp_validate_check_file_prep_auth_bearer(p_params, p_http_auth))
                {
                    os_free(p_http_auth);
                    return http_server_resp_400();
                }
                break;
            case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
                if (!http_server_resp_validate_check_file_prep_auth_token(p_params, p_http_auth))
                {
                    os_free(p_http_auth);
                    return http_server_resp_400();
                }
                break;
        }
    }
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = p_params->p_url->buf,
            .timeout_seconds         = HTTP_DOWNLOAD_TIMEOUT_SECONDS,
            .flag_feed_task_watchdog = true,
            .flag_free_memory        = true,
            .p_server_cert           = NULL,
            .p_client_cert           = NULL,
            .p_client_key            = NULL,
        },
        .auth_type = p_params->auth_type,
        .p_http_auth = p_http_auth,
        .p_extra_header_item = NULL,
    };
    const http_resp_code_e http_resp_code = http_check(&params);

    if (NULL != p_http_auth)
    {
        os_free(p_http_auth);
    }

    return http_server_cb_gen_resp(http_resp_code, "%s", "");
}

static bool
http_server_resp_validate_url_get_auth_type(const char* const p_params, gw_cfg_http_auth_type_e* const p_auth_type)
{
    str_buf_t auth_type_str_buf = http_server_get_from_params_with_decoding(p_params, "auth_type=");
    if (NULL == auth_type_str_buf.buf)
    {
        LOG_ERR("HTTP validate: can't find 'auth_type' in params: %s", p_params);
        return false;
    }
    gw_cfg_http_auth_type_e auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
    if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_NONE, auth_type_str_buf.buf))
    {
        auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
    }
    else if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_BASIC, auth_type_str_buf.buf))
    {
        auth_type = GW_CFG_HTTP_AUTH_TYPE_BASIC;
    }
    else if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_BEARER, auth_type_str_buf.buf))
    {
        auth_type = GW_CFG_HTTP_AUTH_TYPE_BEARER;
    }
    else if (0 == strcmp(GW_CFG_HTTP_AUTH_TYPE_STR_TOKEN, auth_type_str_buf.buf))
    {
        auth_type = GW_CFG_HTTP_AUTH_TYPE_TOKEN;
    }
    else
    {
        LOG_ERR("HTTP validate: unknown 'auth_type' in params: %s", p_params);
        str_buf_free_buf(&auth_type_str_buf);
        return false;
    }
    *p_auth_type = auth_type;
    str_buf_free_buf(&auth_type_str_buf);
    return true;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_validate_url(const char* const p_url_params)
{
    if (NULL == p_url_params)
    {
        LOG_ERR("Expected params for HTTP validate");
        return http_server_resp_400();
    }

    gw_cfg_http_auth_type_e auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
    if (!http_server_resp_validate_url_get_auth_type(p_url_params, &auth_type))
    {
        return http_server_resp_400();
    }

    str_buf_t url = http_server_get_from_params_with_decoding(p_url_params, "url=");
    if (NULL == url.buf)
    {
        LOG_ERR("HTTP validate: can't find 'url' in params: %s", p_url_params);
        return http_server_resp_400();
    }
    LOG_DBG("Got URL from params: %s", url.buf);

    str_buf_t user     = http_server_get_from_params_with_decoding(p_url_params, "user=");
    str_buf_t password = http_server_get_password_from_params(p_url_params);

    const bool flag_wait_until_relaying_stopped = true;
    gw_status_suspend_relaying(flag_wait_until_relaying_stopped);

    const http_validate_type_e validate_type = http_server_get_validate_type_from_params(p_url_params);

    const http_server_resp_validate_params_t params = {
        .p_url                   = &url,
        .auth_type               = auth_type,
        .p_user                  = &user,
        .p_password              = &password,
        .flag_use_saved_password = http_server_get_bool_from_params(p_url_params, "use_saved_password="),
        .use_ssl_client_cert     = http_server_get_use_ssl_client_cert(p_url_params),
        .use_ssl_server_cert     = http_server_get_use_ssl_server_cert(p_url_params),
    };
    http_server_resp_t http_resp = http_server_resp_err(HTTP_RESP_CODE_400);
    switch (validate_type)
    {
        case HTTP_VALIDATE_TYPE_INVALID:
            LOG_ERR("HTTP validate: invalid 'validate_type' param: %s", p_url_params);
            http_resp = http_server_resp_500();
            break;
        case HTTP_VALIDATE_TYPE_POST_ADVS:
            http_resp = http_server_resp_validate_post_advs(&params);
            break;
        case HTTP_VALIDATE_TYPE_POST_STAT:
            http_resp = http_server_resp_validate_post_stat(&params);
            break;
        case HTTP_VALIDATE_TYPE_CHECK_MQTT:
            http_resp = http_server_resp_validate_check_mqtt(&params, p_url_params);
            break;
        case HTTP_VALIDATE_TYPE_CHECK_REMOTE_CFG:
            http_resp = http_server_resp_validate_check_remote_cfg(&params);
            break;
        case HTTP_VALIDATE_TYPE_CHECK_FILE:
            http_resp = http_server_resp_validate_check_file(&params);
            break;
    }

    str_buf_free_buf(&url);
    str_buf_free_buf(&user);
    str_buf_free_buf(&password);

    const bool flag_wait_until_relaying_resumed = true;
    gw_status_resume_relaying(flag_wait_until_relaying_resumed);

    return http_resp;
}

HTTP_SERVER_CB_STATIC
http_content_type_e
http_get_content_type_by_ext(const char* p_file_ext)
{
    if (0 == strcmp(p_file_ext, ".html"))
    {
        return HTTP_CONENT_TYPE_TEXT_HTML;
    }
    if ((0 == strcmp(p_file_ext, ".css")) || (0 == strcmp(p_file_ext, ".scss")))
    {
        return HTTP_CONENT_TYPE_TEXT_CSS;
    }
    if (0 == strcmp(p_file_ext, ".js"))
    {
        return HTTP_CONENT_TYPE_TEXT_JAVASCRIPT;
    }
    if (0 == strcmp(p_file_ext, ".png"))
    {
        return HTTP_CONENT_TYPE_IMAGE_PNG;
    }
    if (0 == strcmp(p_file_ext, ".svg"))
    {
        return HTTP_CONENT_TYPE_IMAGE_SVG_XML;
    }
    if (0 == strcmp(p_file_ext, ".ttf"))
    {
        return HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM;
    }
    return HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM;
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_file(const char* file_path, const http_resp_code_e http_resp_code)
{
    LOG_DBG("Try to find file: %s", file_path);
    if (NULL == gp_ffs_gwui)
    {
        LOG_ERR("GWUI partition is not ready");
        return http_server_resp_503();
    }
    const flash_fat_fs_t* p_ffs = gp_ffs_gwui;

    const char* p_file_ext = strrchr(file_path, '.');
    if (NULL == p_file_ext)
    {
        p_file_ext = "";
    }

    size_t       file_size         = 0;
    bool         is_gzipped        = false;
    char         tmp_file_path[64] = { '\0' };
    const char*  suffix_gz         = ".gz";
    const size_t suffix_gz_len     = strlen(suffix_gz);
    if ((strlen(file_path) + suffix_gz_len) >= sizeof(tmp_file_path))
    {
        LOG_ERR("Temporary buffer is not enough for the file path '%s'", file_path);
        return http_server_resp_503();
    }
    if ((0 == strcmp(".js", p_file_ext)) || (0 == strcmp(".html", p_file_ext)) || (0 == strcmp(".css", p_file_ext)))
    {
        snprintf(tmp_file_path, sizeof(tmp_file_path), "%s%s", file_path, suffix_gz);
        if (flashfatfs_get_file_size(p_ffs, tmp_file_path, &file_size))
        {
            is_gzipped = true;
        }
    }
    if (!is_gzipped)
    {
        snprintf(tmp_file_path, sizeof(tmp_file_path), "%s", file_path);
        if (!flashfatfs_get_file_size(p_ffs, tmp_file_path, &file_size))
        {
            LOG_ERR("Can't find file: %s", tmp_file_path);
            return http_server_resp_404();
        }
    }
    const http_content_type_e     content_type     = http_get_content_type_by_ext(p_file_ext);
    const http_content_encoding_e content_encoding = is_gzipped ? HTTP_CONENT_ENCODING_GZIP : HTTP_CONENT_ENCODING_NONE;

    const file_descriptor_t fd = flashfatfs_open(p_ffs, tmp_file_path);
    if (fd < 0)
    {
        LOG_ERR("Can't open file: %s", tmp_file_path);
        return http_server_resp_503();
    }
    LOG_DBG("File %s was opened successfully, fd=%d", tmp_file_path, fd);
    const bool flag_no_cache = true;
    return http_server_resp_data_from_file(
        http_resp_code,
        content_type,
        NULL,
        file_size,
        content_encoding,
        fd,
        flag_no_cache);
}

http_server_resp_t
http_server_cb_on_get(
    const char* const               p_path,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth)
{
    const char* p_file_ext = strrchr(p_path, '.');
    LOG_DBG(
        "http_server_cb_on_get /%s%s%s",
        p_path,
        (NULL != p_uri_params) ? "?" : "",
        (NULL != p_uri_params) ? p_uri_params : "");

    if ((NULL != p_file_ext) && (0 == strcmp(p_file_ext, ".json")))
    {
        return http_server_resp_json(p_path, flag_access_from_lan);
    }
    if (0 == strcmp(p_path, "metrics"))
    {
        return http_server_resp_metrics();
    }
    if (0 == strcmp(p_path, "history"))
    {
        return http_server_resp_history(p_uri_params);
    }
    if (0 == strcmp(p_path, "validate_url"))
    {
        return http_server_resp_validate_url(p_uri_params);
    }
    const char* p_file_path = ('\0' == p_path[0]) ? "ruuvi.html" : p_path;
    return http_server_resp_file(p_file_path, (NULL != p_resp_auth) ? p_resp_auth->http_resp_code : HTTP_RESP_CODE_200);
}
