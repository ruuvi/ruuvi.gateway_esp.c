/**
 * @file mqtt_json.c
 * @author TheSomeMan
 * @date 2021-02-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "mqtt_json.h"
#include "os_malloc.h"
#include "ruuvi_endpoint_5.h"
#include "ruuvi_endpoint_6.h"
#include "adv_decode.h"
#if defined(RUUVI_TESTS_MQTT_JSON) && RUUVI_TESTS_MQTT_JSON
#define LOG_LOCAL_DISABLED 1
#define LOG_LOCAL_LEVEL    LOG_LEVEL_NONE
#else
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#endif
#include "log.h"
static const char TAG[] = "mqtt";

typedef struct mqtt_json_stream_gen_adv_ctx_t
{
    const adv_report_t*       p_adv;
    bool                      flag_use_timestamps;
    time_t                    timestamp;
    const mac_address_str_t*  p_mac_addr;
    const char*               p_coordinates_str;
    gw_cfg_mqtt_data_format_e mqtt_data_format;
} mqtt_json_stream_gen_adv_ctx_t;

static json_stream_gen_callback_result_t
mqtt_cb_json_stream_gen_adv(json_stream_gen_t* const p_gen, const void* const p_user_ctx)
{
    const mqtt_json_stream_gen_adv_ctx_t* const p_ctx = p_user_ctx;
    JSON_STREAM_GEN_BEGIN_GENERATOR_FUNC(p_gen);

    JSON_STREAM_GEN_ADD_STRING(p_gen, "gw_mac", p_ctx->p_mac_addr->str_buf);
    JSON_STREAM_GEN_ADD_INT32(p_gen, "rssi", p_ctx->p_adv->rssi);
    JSON_STREAM_GEN_START_ARRAY(p_gen, "aoa");
    JSON_STREAM_GEN_END_ARRAY(p_gen);
    if (p_ctx->flag_use_timestamps)
    {
        JSON_STREAM_GEN_ADD_UINT32(p_gen, "gwts", p_ctx->timestamp);
        JSON_STREAM_GEN_ADD_UINT32(p_gen, "ts", p_ctx->p_adv->timestamp);
    }
    else
    {
        JSON_STREAM_GEN_ADD_UINT32(p_gen, "cnt", p_ctx->p_adv->timestamp);
    }
    if ((GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW == p_ctx->mqtt_data_format)
        || (GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED == p_ctx->mqtt_data_format))
    {
        JSON_STREAM_GEN_ADD_HEX_BUF(p_gen, "data", p_ctx->p_adv->data_buf, p_ctx->p_adv->data_len);
    }
    if ((GW_CFG_MQTT_DATA_FORMAT_RUUVI_RAW_AND_DECODED == p_ctx->mqtt_data_format)
        || (GW_CFG_MQTT_DATA_FORMAT_RUUVI_DECODED == p_ctx->mqtt_data_format))
    {
        if (re_5_check_format(p_ctx->p_adv->data_buf))
        {
            JSON_STREAM_GEN_CALL_GENERATOR_SUB_FUNC(adv_decode_df5_cb_json_stream_gen, p_gen, p_ctx->p_adv);
        }
        if (re_6_check_format(p_ctx->p_adv->data_buf))
        {
            JSON_STREAM_GEN_CALL_GENERATOR_SUB_FUNC(adv_decode_df6_cb_json_stream_gen, p_gen, p_ctx->p_adv);
        }
    }

    JSON_STREAM_GEN_ADD_STRING(p_gen, "coords", p_ctx->p_coordinates_str);

    JSON_STREAM_GEN_END_GENERATOR_FUNC();
}

str_buf_t
mqtt_create_json_str(
    const adv_report_t* const       p_adv,
    const bool                      flag_use_timestamps,
    const time_t                    timestamp,
    const mac_address_str_t* const  p_mac_addr,
    const char* const               p_coordinates_str,
    const gw_cfg_mqtt_data_format_e mqtt_data_format,
    const json_stream_gen_size_t    max_chunk_size)
{
    const json_stream_gen_cfg_t cfg = {
        .max_chunk_size      = max_chunk_size,
        .flag_formatted_json = false,
        .indentation_mark    = ' ',
        .indentation         = 0,
        .max_nesting_level   = 2,
        .p_malloc            = &os_malloc,
        .p_free              = &os_free_internal,
        .p_localeconv        = NULL,
    };
    mqtt_json_stream_gen_adv_ctx_t* p_ctx = NULL;
    json_stream_gen_t*              p_gen = json_stream_gen_create(
        &cfg,
        &mqtt_cb_json_stream_gen_adv,
        sizeof(*p_ctx),
        (void**)&p_ctx);
    if (NULL == p_gen)
    {
        LOG_ERR("Not enough memory");
        return str_buf_init_null();
    }
    p_ctx->p_adv               = p_adv;
    p_ctx->flag_use_timestamps = flag_use_timestamps;
    p_ctx->timestamp           = timestamp;
    p_ctx->p_mac_addr          = p_mac_addr;
    p_ctx->p_coordinates_str   = p_coordinates_str;
    p_ctx->mqtt_data_format    = mqtt_data_format;

    const char* p_chunk = json_stream_gen_get_next_chunk(p_gen);
    if (NULL == p_chunk) // Check if there is any errors
    {
        json_stream_gen_delete(&p_gen);
        LOG_ERR("Error while json generation (exceeding the nesting level, etc.)");
        return str_buf_init_null();
    }
    str_buf_t str_buf = str_buf_printf_with_alloc("%s", p_chunk);
    p_chunk           = json_stream_gen_get_next_chunk(p_gen);
    if (NULL == p_chunk) // Check if there is any errors
    {
        json_stream_gen_delete(&p_gen);
        str_buf_free_buf(&str_buf);
        LOG_ERR("Error while json generation (exceeding the nesting level, etc.)");
        return str_buf_init_null();
    }

    if ('\0' != *p_chunk)
    {
        json_stream_gen_reset(p_gen);
        const size_t json_len = json_stream_gen_calc_size(p_gen);
        json_stream_gen_delete(&p_gen);
        str_buf_free_buf(&str_buf);
        LOG_ERR("Json length %u exceeds the maximum chunk size %u", json_len, cfg.max_chunk_size);
        return str_buf_init_null();
    }

    json_stream_gen_delete(&p_gen);
    if (NULL == str_buf.buf)
    {
        LOG_ERR("Not enough memory");
        return str_buf_init_null();
    }
    return str_buf;
}
