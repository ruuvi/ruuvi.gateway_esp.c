/**
 * @file gw_cfg_json_parse_internal.c
 * @author TheSomeMan
 * @date 2021-09-29
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gw_cfg_json_parse_internal.h"
#include "cjson_wrap.h"

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

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
static const char TAG[] = "gw_cfg";
#endif

bool
gw_cfg_json_copy_string_val(
    const cJSON* const p_json_root,
    const char* const  p_attr_name,
    char* const        p_buf,
    const size_t       buf_len)
{
    if (!json_wrap_copy_string_val(p_json_root, p_attr_name, p_buf, buf_len))
    {
        LOG_DBG("%s: not found", p_attr_name);
        return false;
    }
    LOG_DBG("%s: %s", p_attr_name, p_buf);
    return true;
}

bool
gw_cfg_json_get_bool_val(const cJSON* p_json_root, const char* p_attr_name, bool* p_val)
{
    if (!json_wrap_get_bool_val(p_json_root, p_attr_name, p_val))
    {
        LOG_DBG("%s: not found", p_attr_name);
        return false;
    }
    LOG_DBG("%s: %d", p_attr_name, *p_val);
    return true;
}

bool
gw_cfg_json_get_uint32_val(const cJSON* const p_json_root, const char* const p_attr_name, uint32_t* const p_val)
{
    if (!json_wrap_get_uint32_val(p_json_root, p_attr_name, p_val))
    {
        LOG_DBG("%s: not found or invalid", p_attr_name);
        return false;
    }
    LOG_DBG("%s: %u", p_attr_name, (printf_uint_t)*p_val);
    return true;
}

bool
gw_cfg_json_get_uint16_val(const cJSON* const p_json_root, const char* const p_attr_name, uint16_t* const p_val)
{
    if (!json_wrap_get_uint16_val(p_json_root, p_attr_name, p_val))
    {
        LOG_DBG("%s: not found or invalid", p_attr_name);
        return false;
    }
    LOG_DBG("%s: %u", p_attr_name, *p_val);
    return true;
}

bool
gw_cfg_json_get_uint8_val(const cJSON* const p_json_root, const char* const p_attr_name, uint8_t* const p_val)
{
    if (!json_wrap_get_uint8_val(p_json_root, p_attr_name, p_val))
    {
        LOG_DBG("%s: not found or invalid", p_attr_name);
        return false;
    }
    LOG_DBG("%s: %u", p_attr_name, *p_val);
    return true;
}

bool
gw_cfg_json_get_int8_val(const cJSON* p_json_root, const char* p_attr_name, int8_t* p_val)
{
    if (!json_wrap_get_int8_val(p_json_root, p_attr_name, p_val))
    {
        LOG_DBG("%s: not found or invalid", p_attr_name);
        return false;
    }
    LOG_DBG("%s: %d", p_attr_name, (printf_int_t)*p_val);
    return true;
}
