/**
 * @file http_server_cb.c
 * @author TheSomeMan
 * @date 2020-10-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_cb.h"
#include <string.h>
#include "settings.h"
#include "ruuvi_gateway.h"
#include "cjson_wrap.h"
#include "ruuvi_gwui_html.h"
#include "wifi_manager.h"
#include "ethernet.h"
#include "json_ruuvi.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "log.h"

static const char TAG[] = "http_server";

static const char g_empty_json[] = "{}";

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json_ruuvi(void)
{
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!gw_cfg_generate_json_str(&json_str))
    {
        return http_server_resp_503();
    }
    LOG_INFO("ruuvi.json: %s", json_str.p_str);
    const bool flag_no_cache = true;
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(json_str.p_str),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)json_str.p_str,
        flag_no_cache);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_json(const char *p_file_name)
{
    if (0 == strcmp(p_file_name, "ruuvi.json"))
    {
        return http_server_resp_json_ruuvi();
    }
    LOG_WARN("Request to unknown json: %s", p_file_name);
    return http_server_resp_404();
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_resp_metrics(void)
{
    const char *p_metrics = ruuvi_get_metrics();
    if (NULL == p_metrics)
    {
        LOG_ERR("Not enough memory");
        return http_server_resp_503();
    }
    const bool flag_no_cache = true;
    LOG_INFO("metrics: %s", p_metrics);
    return http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_TEXT_PLAIN,
        "version=0.0.4",
        strlen(p_metrics),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)p_metrics,
        flag_no_cache);
}

HTTP_SERVER_CB_STATIC
http_content_type_e
http_get_content_type_by_ext(const char *p_file_ext)
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
http_server_resp_file(const char *file_path)
{
    size_t file_len   = 0;
    bool   is_gzipped = false;
    LOG_DBG("Try to find file: %s", file_path);
    const uint8_t *p_file_content = embed_files_find(file_path, &file_len, &is_gzipped);
    if (NULL == p_file_content)
    {
        LOG_ERR("File not found: %s", file_path);
        return http_server_resp_404();
    }
    const char *p_file_ext = strrchr(file_path, '.');
    if (NULL == p_file_ext)
    {
        p_file_ext = "";
    }
    const http_content_type_e     content_type     = http_get_content_type_by_ext(p_file_ext);
    const http_content_encoding_e content_encoding = is_gzipped ? HTTP_CONENT_ENCODING_GZIP : HTTP_CONENT_ENCODING_NONE;
    return http_server_resp_data_in_flash(content_type, NULL, file_len, content_encoding, p_file_content);
}

http_server_resp_t
http_server_cb_on_get(const char *p_path)
{
    const char *p_file_ext = strrchr(p_path, '.');
    LOG_INFO("GET /%s", p_path);

    if ((NULL != p_file_ext) && (0 == strcmp(p_file_ext, ".json")))
    {
        return http_server_resp_json(p_path);
    }
    if (0 == strcmp(p_path, "metrics"))
    {
        return http_server_resp_metrics();
    }
    const char *p_file_path = ('\0' == p_path[0]) ? "index.html" : p_path;
    return http_server_resp_file(p_file_path);
}

HTTP_SERVER_CB_STATIC
http_server_resp_t
http_server_cb_on_post_ruuvi(const char *p_body)
{
    LOG_DBG("POST /ruuvi.json");
    if (!json_ruuvi_parse_http_body(p_body, &g_gateway_config))
    {
        return http_server_resp_503();
    }
    gw_cfg_print_to_log(&g_gateway_config);
    settings_save_to_flash(&g_gateway_config);
    ruuvi_send_nrf_settings(&g_gateway_config);
    ethernet_update_ip();
    return http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(g_empty_json),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)g_empty_json);
}

http_server_resp_t
http_server_cb_on_post(const char *p_file_name, const char *p_body)
{
    if (0 == strcmp(p_file_name, "ruuvi.json"))
    {
        return http_server_cb_on_post_ruuvi(p_body);
    }
    LOG_WARN("POST /%s", p_file_name);
    return http_server_resp_404();
}

http_server_resp_t
http_server_cb_on_delete(const char *p_path)
{
    (void)p_path;
    LOG_WARN("DELETE /%s", p_path);
    return http_server_resp_404();
}
