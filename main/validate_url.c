/**
 * @file validate_url.c
 * @author TheSomeMan
 * @date 2023-07-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "validate_url.h"
#include "gw_cfg.h"
#include "os_malloc.h"
#include "os_str.h"
#include "http_server_cb.h"
#include "gw_status.h"

#if RUUVI_TESTS_HTTP_SERVER_CB
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char TAG[] = "http_server";

#define MQTT_URL_PREFIX_LEN (20)

#define BASE_10 (10)

typedef enum validate_url_type_e
{
    HTTP_VALIDATE_TYPE_INVALID,
    HTTP_VALIDATE_TYPE_POST_ADVS,
    HTTP_VALIDATE_TYPE_POST_STAT,
    HTTP_VALIDATE_TYPE_CHECK_MQTT,
    HTTP_VALIDATE_TYPE_CHECK_REMOTE_CFG,
    HTTP_VALIDATE_TYPE_CHECK_FW_UPDATE_URL,
    HTTP_VALIDATE_TYPE_CHECK_FILE,
} validate_url_type_e;

typedef struct validate_url_params_t
{
    str_buf_t                     url;
    str_buf_t                     user;
    str_buf_t                     password;
    const gw_cfg_http_auth_type_e auth_type;
    const bool                    flag_use_saved_password;
    const bool                    use_ssl_client_cert;
    const bool                    use_ssl_server_cert;
} validate_url_params_t;

static bool
validate_url_get_bool_from_params(const char* const p_params, const char* const p_key)
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

static str_buf_t
validate_url_get_password_from_params(const char* const p_params)
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

static validate_url_type_e
validate_url_get_validate_type_from_params(const char* const p_params)
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
    if (0 == strncmp(p_value, "check_fw_update_url", val_len))
    {
        return HTTP_VALIDATE_TYPE_CHECK_FW_UPDATE_URL;
    }
    if (0 == strncmp(p_value, "check_file", val_len))
    {
        return HTTP_VALIDATE_TYPE_CHECK_FILE;
    }
    LOG_ERR("Unknown validate_type: %.*s", val_len, p_value);
    return HTTP_VALIDATE_TYPE_INVALID;
}

static bool
validate_url_get_use_ssl_client_cert(const char* const p_params)
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

static bool
validate_url_get_use_ssl_server_cert(const char* const p_params)
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

static bool
validate_url_check_url(const str_buf_t* const p_url)
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

static const char*
validate_url_auth_type_to_str(const gw_cfg_http_auth_type_e auth_type)
{
    switch (auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            return GW_CFG_HTTP_AUTH_TYPE_STR_NONE;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            return GW_CFG_HTTP_AUTH_TYPE_STR_BASIC;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            return GW_CFG_HTTP_AUTH_TYPE_STR_BEARER;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            return GW_CFG_HTTP_AUTH_TYPE_STR_TOKEN;
    }
    return "UNKNOWN";
}

static http_server_resp_t
validate_url_post_advs(const validate_url_params_t* const p_params)
{
    if (!validate_url_check_url(&p_params->url))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Incorrect URL: '%s'", p_params->url.buf);
    }

    str_buf_t saved_password = gw_cfg_get_http_password_copy();
    if (NULL == saved_password.buf)
    {
        LOG_ERR("Can't allocate memory for saved http_password");
        return http_server_resp_500();
    }
    const http_check_params_t params = {
        .p_url               = p_params->url.buf,
        .auth_type           = p_params->auth_type,
        .p_user              = p_params->user.buf,
        .p_pass              = p_params->flag_use_saved_password ? saved_password.buf : p_params->password.buf,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };
    LOG_INFO("Validate URL (POST advs): %s", params.p_url);
    LOG_INFO("Validate URL (POST advs): auth_type=%s", validate_url_auth_type_to_str(params.auth_type));
    LOG_INFO("Validate URL (POST advs): user=%s", (NULL != params.p_user) ? params.p_user : "NULL");
    LOG_INFO("Validate URL (POST advs): use_saved_password=%d", p_params->flag_use_saved_password);
    LOG_DBG("Validate URL (POST advs): saved_password=%s", saved_password.buf);
    LOG_DBG("Validate URL (POST advs): password=%s", (NULL != params.p_pass) ? params.p_pass : "NULL");
    LOG_INFO("Validate URL (POST advs): use_ssl_client_cert=%d", p_params->use_ssl_client_cert);
    LOG_INFO("Validate URL (POST advs): use_ssl_server_cert=%d", p_params->use_ssl_server_cert);
    const http_server_resp_t http_resp = http_check_post_advs(&params, HTTP_DOWNLOAD_TIMEOUT_SECONDS);
    str_buf_free_buf(&saved_password);
    return http_resp;
}

static http_server_resp_t
validate_url_post_stat(const validate_url_params_t* const p_params)
{
    if (!validate_url_check_url(&p_params->url))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Incorrect URL: '%s'", p_params->url.buf);
    }

    str_buf_t saved_password = gw_cfg_get_http_stat_password_copy();
    if (NULL == saved_password.buf)
    {
        LOG_ERR("Can't allocate memory for saved http_stat_password");
        return http_server_resp_500();
    }
    const char* const p_pass = p_params->flag_use_saved_password ? saved_password.buf : p_params->password.buf;

    const http_check_params_t params = {
        .p_url               = p_params->url.buf,
        .auth_type           = p_params->auth_type,
        .p_user              = (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) ? "" : p_params->user.buf,
        .p_pass              = (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) ? "" : p_pass,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };
    LOG_INFO("Validate URL (POST stat): %s", params.p_url);
    LOG_INFO("Validate URL (POST stat): auth_type=%s", validate_url_auth_type_to_str(params.auth_type));
    LOG_INFO("Validate URL (POST stat): user=%s", (NULL != params.p_user) ? params.p_user : "NULL");
    LOG_INFO("Validate URL (POST stat): use_saved_password=%d", p_params->flag_use_saved_password);
    LOG_DBG("Validate URL (POST stat): saved_password=%s", saved_password.buf);
    LOG_DBG("Validate URL (POST stat): password=%s", (NULL != params.p_pass) ? params.p_pass : "NULL");
    LOG_INFO("Validate URL (POST stat): use_ssl_client_cert=%d", p_params->use_ssl_client_cert);
    LOG_INFO("Validate URL (POST stat): use_ssl_server_cert=%d", p_params->use_ssl_server_cert);

    const http_server_resp_t http_resp = http_check_post_stat(&params, HTTP_DOWNLOAD_TIMEOUT_SECONDS);
    str_buf_free_buf(&saved_password);
    return http_resp;
}

static bool
validate_url_parse_mqtt_url(const char* const p_url, ruuvi_gw_cfg_mqtt_t* const p_mqtt_cfg)
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

static bool
validate_url_get_mqtt_disable_retained_messages(const char* const p_params)
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

static http_server_resp_t
validate_url_on_get_check_mqtt(
    const http_check_params_t* const p_params,
    const char* const                p_url_params,
    const TimeUnitsSeconds_t         timeout_seconds)
{
    if ((GW_CFG_HTTP_AUTH_TYPE_NONE != p_params->auth_type)
        && ((NULL == p_params->p_user) || (NULL == p_params->p_pass)))
    {
        return http_server_resp_400();
    }

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

    if (!validate_url_parse_mqtt_url(p_params->p_url, p_mqtt_cfg))
    {
        str_buf_free_buf(&client_id);
        str_buf_free_buf(&topic_prefix);
        os_free(p_mqtt_cfg);
        return http_server_resp_400();
    }
    const bool flag_disable_retained_messages = validate_url_get_mqtt_disable_retained_messages(p_url_params);

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

static http_server_resp_t
validate_url_check_mqtt(const validate_url_params_t* const p_params, const char* const p_url_params)
{
    str_buf_t saved_password = gw_cfg_get_mqtt_password_copy();
    if (NULL == saved_password.buf)
    {
        LOG_ERR("Can't allocate memory for saved mqtt_password");
        return http_server_resp_500();
    }

    const char* const         p_pass = p_params->flag_use_saved_password ? saved_password.buf : p_params->password.buf;
    const http_check_params_t params = {
        .p_url               = p_params->url.buf,
        .auth_type           = p_params->auth_type,
        .p_user              = (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) ? "" : p_params->user.buf,
        .p_pass              = (GW_CFG_HTTP_AUTH_TYPE_NONE == p_params->auth_type) ? "" : p_pass,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };
    LOG_INFO("Validate URL (MQTT): %s", params.p_url);
    LOG_INFO("Validate URL (MQTT): auth_type=%s", validate_url_auth_type_to_str(params.auth_type));
    LOG_INFO("Validate URL (MQTT): user=%s", (NULL != params.p_user) ? params.p_user : "NULL");
    LOG_INFO("Validate URL (MQTT): use_saved_password=%d", p_params->flag_use_saved_password);
    LOG_DBG("Validate URL (MQTT): saved_password=%s", saved_password.buf);
    LOG_DBG("Validate URL (MQTT): password=%s", (NULL != params.p_pass) ? params.p_pass : "NULL");
    LOG_INFO("Validate URL (MQTT): use_ssl_client_cert=%d", p_params->use_ssl_client_cert);
    LOG_INFO("Validate URL (MQTT): use_ssl_server_cert=%d", p_params->use_ssl_server_cert);
    const http_server_resp_t http_resp = validate_url_on_get_check_mqtt(
        &params,
        p_url_params,
        HTTP_DOWNLOAD_CHECK_MQTT_TIMEOUT_SECONDS);
    str_buf_free_buf(&saved_password);
    return http_resp;
}

static bool
gw_cfg_remote_set_auth_basic(
    ruuvi_gw_cfg_remote_t* const p_remote_cfg,
    const char* const            p_user,
    const char* const            p_pass)
{
    if (NULL == p_user)
    {
        LOG_ERR("remote_cfg username is NULL");
        return false;
    }
    if (NULL == p_pass)
    {
        LOG_ERR("remote_cfg password is NULL");
        return false;
    }
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

static bool
gw_cfg_remote_set_auth_bearer(ruuvi_gw_cfg_remote_t* const p_remote_cfg, const char* const p_token)
{
    if (NULL == p_token)
    {
        LOG_ERR("remote_cfg token is NULL");
        return false;
    }
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

static bool
gw_cfg_remote_set_auth_token(ruuvi_gw_cfg_remote_t* const p_remote_cfg, const char* const p_token)
{
    if (NULL == p_token)
    {
        LOG_ERR("remote_cfg token is NULL");
        return false;
    }
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

static http_server_resp_t
validate_url_on_get_check_remote_cfg(const http_check_params_t* const p_params)
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

static http_server_resp_t
validate_url_check_remote_cfg(const validate_url_params_t* const p_params)
{
    if (!validate_url_check_url(&p_params->url))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Incorrect URL: '%s'", p_params->url.buf);
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
        .p_url               = p_params->url.buf,
        .auth_type           = p_params->auth_type,
        .p_user              = p_params->user.buf,
        .p_pass              = p_params->flag_use_saved_password ? p_saved_password : p_params->password.buf,
        .use_ssl_client_cert = p_params->use_ssl_client_cert,
        .use_ssl_server_cert = p_params->use_ssl_server_cert,
    };
    LOG_INFO("Validate URL (GET remote_cfg): %s", params.p_url);
    LOG_INFO("Validate URL (GET remote_cfg): auth_type=%s", validate_url_auth_type_to_str(params.auth_type));
    LOG_INFO("Validate URL (GET remote_cfg): user=%s", (NULL != params.p_user) ? params.p_user : "NULL");
    LOG_INFO("Validate URL (GET remote_cfg): use_saved_password=%d", p_params->flag_use_saved_password);
    LOG_DBG("Validate URL (GET remote_cfg): saved_password=%s", p_saved_password);
    LOG_DBG("Validate URL (GET remote_cfg): password=%s", (NULL != params.p_pass) ? params.p_pass : "NULL");
    LOG_INFO("Validate URL (GET remote_cfg): use_ssl_client_cert=%d", p_params->use_ssl_client_cert);
    LOG_INFO("Validate URL (GET remote_cfg): use_ssl_server_cert=%d", p_params->use_ssl_server_cert);
    const http_server_resp_t http_resp = validate_url_on_get_check_remote_cfg(&params);
    return http_resp;
}

static http_server_resp_t
validate_url_check_fw_update_url_step3(const cJSON* const p_json_root, const char* const p_json_buf)
{
    bool flag_use_beta_version = false;

    const char* const p_binaries_url = parse_fw_update_info_json(p_json_root, &flag_use_beta_version);
    if (NULL == p_binaries_url)
    {
        LOG_ERR("Failed to parse fw_update_info: %s", p_json_buf);
        return http_server_cb_gen_resp(HTTP_RESP_CODE_502, "Server returned incorrect json: could not get latest/url");
    }

    const char*            p_err_file_name = NULL;
    const http_resp_code_e http_resp_code  = http_server_check_fw_update_binary_files(p_binaries_url, &p_err_file_name);
    if (HTTP_RESP_CODE_200 != http_resp_code)
    {
        LOG_ERR(
            "Failed to download firmware update: http_resp_code=%u, failed to download file: %s",
            http_resp_code,
            p_err_file_name);
        return http_server_cb_gen_resp(http_resp_code, "Failed to download %s", p_err_file_name);
    }
    return http_server_cb_gen_resp_json(HTTP_RESP_CODE_200, p_json_buf);
}

static http_server_resp_t
validate_url_check_fw_update_url_step2(const http_server_download_info_t* const p_info)
{
    LOG_INFO("Firmware update info (json): %s", p_info->p_json_buf);
    if (NULL == p_info->p_json_buf)
    {
        LOG_ERR("Failed to download firmware update info: Can't allocate memory for json");
        return http_server_cb_gen_resp(HTTP_RESP_CODE_500, "Can't allocate memory for json");
    }

    cJSON* p_json_root = cJSON_Parse(p_info->p_json_buf);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse fw_update_info: %s", p_info->p_json_buf);
        return http_server_cb_gen_resp(HTTP_RESP_CODE_502, "Server returned incorrect json: could not parse json");
    }

    const http_server_resp_t http_resp = validate_url_check_fw_update_url_step3(p_json_root, p_info->p_json_buf);

    cJSON_free(p_json_root);

    return http_resp;
}

static http_server_resp_t
validate_url_check_fw_update_url(const validate_url_params_t* const p_params)
{
    if (!validate_url_check_url(&p_params->url))
    {
        return http_server_cb_gen_resp(HTTP_RESP_CODE_400, "Incorrect URL: '%s'", p_params->url.buf);
    }
    http_server_download_info_t info = http_download_firmware_update_info(p_params->url.buf, true);
    if (info.is_error)
    {
        LOG_ERR(
            "Failed to download firmware update info: http_resp_code=%u, content: %s",
            info.http_resp_code,
            NULL != info.p_json_buf ? info.p_json_buf : "<NULL>");
        if (NULL != info.p_json_buf)
        {
            const http_server_resp_t resp = http_server_cb_gen_resp(info.http_resp_code, "%s", info.p_json_buf);
            os_free(info.p_json_buf);
            return resp;
        }
        return http_server_cb_gen_resp(info.http_resp_code, "Server returned error");
    }

    const http_server_resp_t http_resp = validate_url_check_fw_update_url_step2(&info);

    os_free(info.p_json_buf);

    return http_resp;
}

static bool
validate_url_check_file_prep_auth_basic(
    const validate_url_params_t* const p_params,
    ruuvi_gw_cfg_http_auth_t* const    p_http_auth)
{
    if (NULL == p_params->user.buf)
    {
        LOG_ERR("Username is NULL");
        return false;
    }
    if (NULL == p_params->password.buf)
    {
        LOG_ERR("Password is NULL");
        return false;
    }
    if ('\0' == p_params->user.buf[0])
    {
        LOG_ERR("Username is empty");
        return false;
    }
    if ('\0' == p_params->password.buf[0])
    {
        LOG_ERR("Password is empty");
        return false;
    }
    ruuvi_gw_cfg_http_auth_basic_t* const p_auth = &p_http_auth->auth_basic;
    (void)snprintf(p_auth->user.buf, sizeof(p_auth->user.buf), "%s", p_params->user.buf);
    (void)snprintf(p_auth->password.buf, sizeof(p_auth->password.buf), "%s", p_params->password.buf);
    return true;
}

static bool
validate_url_check_file_prep_auth_bearer(
    const validate_url_params_t* const p_params,
    ruuvi_gw_cfg_http_auth_t* const    p_http_auth)
{
    if (NULL == p_params->password.buf)
    {
        LOG_ERR("Token is NULL");
        return false;
    }
    if ('\0' == p_params->password.buf[0])
    {
        LOG_ERR("Token is empty");
        return false;
    }
    ruuvi_gw_cfg_http_bearer_token_t* const p_bearer = &p_http_auth->auth_bearer.token;
    (void)snprintf(p_bearer->buf, sizeof(p_bearer->buf), "%s", p_params->password.buf);
    return true;
}

static bool
validate_url_check_file_prep_auth_token(
    const validate_url_params_t* const p_params,
    ruuvi_gw_cfg_http_auth_t* const    p_http_auth)
{
    if (NULL == p_params->password.buf)
    {
        LOG_ERR("Token is NULL");
        return false;
    }
    if ('\0' == p_params->password.buf[0])
    {
        LOG_ERR("Token is empty");
        return false;
    }
    ruuvi_gw_cfg_http_token_t* const p_token = &p_http_auth->auth_token.token;
    (void)snprintf(p_token->buf, sizeof(p_token->buf), "%s", p_params->password.buf);
    return true;
}

static http_server_resp_t
validate_url_check_file(const validate_url_params_t* const p_params)
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
                if (!validate_url_check_file_prep_auth_basic(p_params, p_http_auth))
                {
                    os_free(p_http_auth);
                    return http_server_resp_400();
                }
                break;
            case GW_CFG_HTTP_AUTH_TYPE_BEARER:
                if (!validate_url_check_file_prep_auth_bearer(p_params, p_http_auth))
                {
                    os_free(p_http_auth);
                    return http_server_resp_400();
                }
                break;
            case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
                if (!validate_url_check_file_prep_auth_token(p_params, p_http_auth))
                {
                    os_free(p_http_auth);
                    return http_server_resp_400();
                }
                break;
        }
    }
    const http_download_param_with_auth_t params = {
        .base = {
            .p_url                   = p_params->url.buf,
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
    LOG_INFO("Validate URL (GET file): %s", params.base.p_url);
    LOG_INFO("Validate URL (GET file): auth_type=%s", validate_url_auth_type_to_str(params.auth_type));
    switch (params.auth_type)
    {
        case GW_CFG_HTTP_AUTH_TYPE_NONE:
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BASIC:
            LOG_INFO("Validate URL (GET file): user=%s", params.p_http_auth->auth_basic.user.buf);
            LOG_DBG("Validate URL (GET file): password=%s", params.p_http_auth->auth_basic.password.buf);
            break;
        case GW_CFG_HTTP_AUTH_TYPE_BEARER:
            LOG_DBG("Validate URL (GET file): bearer token=%s", params.p_http_auth->auth_bearer.token.buf);
            break;
        case GW_CFG_HTTP_AUTH_TYPE_TOKEN:
            LOG_DBG("Validate URL (GET file): token=%s", params.p_http_auth->auth_token.token.buf);
            break;
    }
    const http_resp_code_e http_resp_code = http_check(&params);

    if (NULL != p_http_auth)
    {
        os_free(p_http_auth);
    }

    return http_server_cb_gen_resp(http_resp_code, "%s", "");
}

static bool
validate_url_get_auth_type(const char* const p_params, gw_cfg_http_auth_type_e* const p_auth_type)
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

static http_server_resp_t
validate_url_internal(
    const validate_url_type_e          validate_type,
    const validate_url_params_t* const p_params,
    const char* const                  p_url_params)
{
    switch (validate_type)
    {
        case HTTP_VALIDATE_TYPE_INVALID:
            LOG_ERR("HTTP validate: invalid 'validate_type' param: %s", p_url_params);
            return http_server_resp_500();
        case HTTP_VALIDATE_TYPE_POST_ADVS:
            return validate_url_post_advs(p_params);
        case HTTP_VALIDATE_TYPE_POST_STAT:
            return validate_url_post_stat(p_params);
        case HTTP_VALIDATE_TYPE_CHECK_MQTT:
            return validate_url_check_mqtt(p_params, p_url_params);
        case HTTP_VALIDATE_TYPE_CHECK_REMOTE_CFG:
            return validate_url_check_remote_cfg(p_params);
        case HTTP_VALIDATE_TYPE_CHECK_FW_UPDATE_URL:
            return validate_url_check_fw_update_url(p_params);
        case HTTP_VALIDATE_TYPE_CHECK_FILE:
            return validate_url_check_file(p_params);
    }
    LOG_ERR("HTTP validate: unknown 'validate_type': %d", validate_type);
    assert(0);
    return http_server_resp_500();
}

http_server_resp_t
validate_url(const char* const p_url_params)
{
    if (NULL == p_url_params)
    {
        LOG_ERR("Expected params for HTTP validate");
        return http_server_resp_400();
    }

    gw_cfg_http_auth_type_e auth_type = GW_CFG_HTTP_AUTH_TYPE_NONE;
    if (!validate_url_get_auth_type(p_url_params, &auth_type))
    {
        return http_server_resp_400();
    }

    const bool flag_wait_until_relaying_stopped = true;
    gw_status_suspend_relaying(flag_wait_until_relaying_stopped);

    validate_url_params_t params = {
        .url                     = http_server_get_from_params_with_decoding(p_url_params, "url="),
        .user                    = http_server_get_from_params_with_decoding(p_url_params, "user="),
        .password                = validate_url_get_password_from_params(p_url_params),
        .auth_type               = auth_type,
        .flag_use_saved_password = validate_url_get_bool_from_params(p_url_params, "use_saved_password="),
        .use_ssl_client_cert     = validate_url_get_use_ssl_client_cert(p_url_params),
        .use_ssl_server_cert     = validate_url_get_use_ssl_server_cert(p_url_params),
    };
    if (NULL == params.url.buf)
    {
        LOG_ERR("HTTP validate: can't find 'url' in params: %s", p_url_params);
        str_buf_free_buf(&params.user);
        str_buf_free_buf(&params.password);
        return http_server_resp_400();
    }
    LOG_DBG("Got URL from params: %s", params.url.buf);

    const validate_url_type_e validate_type = validate_url_get_validate_type_from_params(p_url_params);

    const http_server_resp_t http_resp = validate_url_internal(validate_type, &params, p_url_params);

    str_buf_free_buf(&params.url);
    str_buf_free_buf(&params.user);
    str_buf_free_buf(&params.password);

    const bool flag_wait_until_relaying_resumed = true;
    gw_status_resume_relaying(flag_wait_until_relaying_resumed);

    return http_resp;
}
