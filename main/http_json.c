/**
 * @file http_json.c
 * @author TheSomeMan
 * @date 2021-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_json.h"
#include <string.h>
#include "os_malloc.h"
#include "runtime_stat.h"
#include "ruuvi_endpoint_5.h"
#include "ruuvi_endpoint_6.h"

#if defined(RUUVI_TESTS)
#define LOG_LOCAL_DISABLED 1
#endif
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";

#define NUM_BITS_PER_BYTE (8U)
#define BYTE_MASK         (0xFFU)

typedef struct http_json_stream_gen_advs_ctx_t
{
    bool                       flag_decode;
    bool                       flag_use_timestamps;
    bool                       flag_use_nonce;
    time_t                     timestamp;
    uint32_t                   nonce;
    mac_address_str_t          gw_mac;
    ruuvi_gw_cfg_coordinates_t coordinates;
    adv_report_table_t         reports;
} http_json_stream_gen_advs_ctx_t;

static bool
http_json_generate_task_info(const char* const p_task_name, const uint32_t min_free_stack_size, void* p_userdata)
{
    cJSON* const p_json_tasks = p_userdata;

    cJSON* const p_task_obj = cJSON_CreateObject();
    if (NULL == p_task_obj)
    {
        return false;
    }

    cJSON* const p_task_name_obj = cJSON_AddStringToObject(p_task_obj, "TASK_NAME", p_task_name);
    if (NULL == p_task_name_obj)
    {
        cJSON_Delete(p_task_obj);
        return false;
    }
    if (NULL == cJSON_AddNumberToObject(p_task_obj, "MIN_FREE_STACK_SIZE", min_free_stack_size))
    {
        cJSON_Delete(p_task_obj);
        return false;
    }
    cJSON_AddItemToArray(p_json_tasks, p_task_obj);
    return true;
}

static bool
http_json_generate_attributes_for_sensors(
    const adv_report_table_t* const p_reports,
    cJSON* const                    p_json_active_sensors,
    cJSON* const                    p_json_inactive_sensors,
    cJSON* const                    p_json_tasks)
{
    if (NULL == p_reports)
    {
        return true;
    }
    for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
    {
        const adv_report_t* const p_adv   = &p_reports->table[i];
        const mac_address_str_t   mac_str = mac_address_to_str(&p_adv->tag_mac);
        if (0 != p_adv->samples_counter)
        {
            cJSON* p_json_obj = cJSON_CreateObject();
            if (NULL == p_json_obj)
            {
                return false;
            }
            cJSON_AddItemToArray(p_json_active_sensors, p_json_obj);
            if (NULL == cJSON_AddStringToObject(p_json_obj, "MAC", mac_str.str_buf))
            {
                return false;
            }
            if (!cjson_wrap_add_uint32(p_json_obj, "COUNTER", p_adv->samples_counter))
            {
                return false;
            }
        }
        else
        {
            cJSON* p_json_str = cJSON_CreateString(mac_str.str_buf);
            if (NULL == p_json_str)
            {
                return false;
            }
            cJSON_AddItemToArray(p_json_inactive_sensors, p_json_str);
        }
    }
    if (!runtime_stat_for_each_accumulated_info(&http_json_generate_task_info, p_json_tasks))
    {
        return false;
    }
    return true;
}

static bool
http_json_generate_status_attributes(
    cJSON* const                             p_json_root,
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports)
{
    if (NULL == cJSON_AddStringToObject(p_json_root, "DEVICE_ADDR", p_stat_info->nrf52_mac_addr.str_buf))
    {
        return false;
    }
    if (NULL == cJSON_AddStringToObject(p_json_root, "ESP_FW", p_stat_info->esp_fw.buf))
    {
        return false;
    }
    if (NULL == cJSON_AddStringToObject(p_json_root, "NRF_FW", p_stat_info->nrf_fw.buf))
    {
        return false;
    }
    if (NULL == cJSON_AddBoolToObject(p_json_root, "NRF_STATUS", p_stat_info->nrf_status))
    {
        return false;
    }
    if (!cjson_wrap_add_uint32(p_json_root, "UPTIME", p_stat_info->uptime))
    {
        return false;
    }
    if (!cjson_wrap_add_uint32(p_json_root, "NONCE", p_stat_info->nonce))
    {
        return false;
    }
    const char* const p_connection_type = p_stat_info->is_connected_to_wifi ? "WIFI" : "ETHERNET";
    if (NULL == cJSON_AddStringToObject(p_json_root, "CONNECTION", p_connection_type))
    {
        return false;
    }
    if (!cjson_wrap_add_uint32(p_json_root, "NUM_CONN_LOST", p_stat_info->network_disconnect_cnt))
    {
        return false;
    }
    if (NULL == cJSON_AddStringToObject(p_json_root, "RESET_REASON", p_stat_info->reset_reason.buf))
    {
        return false;
    }
    if (!cjson_wrap_add_uint32(p_json_root, "RESET_CNT", p_stat_info->reset_cnt))
    {
        return false;
    }
    if (NULL == cJSON_AddStringToObject(p_json_root, "RESET_INFO", p_stat_info->p_reset_info))
    {
        return false;
    }
    uint32_t num_sensors_seen = 0;
    if (NULL != p_reports)
    {
        for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
        {
            const adv_report_t* const p_adv = &p_reports->table[i];
            if (0 != p_adv->samples_counter)
            {
                num_sensors_seen += 1;
            }
        }
    }
    if (!cjson_wrap_add_uint32(p_json_root, "SENSORS_SEEN", num_sensors_seen))
    {
        return false;
    }
    cJSON* p_json_active_sensors = cJSON_AddArrayToObject(p_json_root, "ACTIVE_SENSORS");
    if (NULL == p_json_active_sensors)
    {
        return false;
    }
    cJSON* p_json_inactive_sensors = cJSON_AddArrayToObject(p_json_root, "INACTIVE_SENSORS");
    if (NULL == p_json_inactive_sensors)
    {
        return false;
    }
    cJSON* p_json_tasks = cJSON_AddArrayToObject(p_json_root, "TASKS");
    if (NULL == p_json_tasks)
    {
        return false;
    }
    return http_json_generate_attributes_for_sensors(
        p_reports,
        p_json_active_sensors,
        p_json_inactive_sensors,
        p_json_tasks);
}

static cJSON*
http_json_generate_status(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports)
{
    cJSON* p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        return NULL;
    }
    if (!http_json_generate_status_attributes(p_json_root, p_stat_info, p_reports))
    {
        cjson_wrap_delete(&p_json_root);
        return NULL;
    }
    return p_json_root;
}

bool
http_json_create_status_str(
    const http_json_statistics_info_t* const p_stat_info,
    const adv_report_table_t* const          p_reports,
    cjson_wrap_str_t* const                  p_json_str)
{
    cJSON* p_json_root = http_json_generate_status(p_stat_info, p_reports);
    if (NULL == p_json_root)
    {
        return false;
    }
    *p_json_str = cjson_wrap_print_and_delete(&p_json_root);
    if (NULL == p_json_str->p_str)
    {
        return false;
    }
    return true;
}

static bool
cb_json_stream_gen_advs(json_stream_gen_t* const p_gen, const void* const p_user_ctx)
{
    const http_json_stream_gen_advs_ctx_t* const p_ctx = p_user_ctx;
    JSON_STREAM_GEN_BEGIN_GENERATOR_FUNC();
    JSON_STREAM_GEN_START_OBJECT(p_gen, "data");
    JSON_STREAM_GEN_ADD_STRING(p_gen, "coordinates", p_ctx->coordinates.buf);
    if (p_ctx->flag_use_timestamps)
    {
        JSON_STREAM_GEN_ADD_UINT32(p_gen, "timestamp", p_ctx->timestamp);
    }
    if (p_ctx->flag_use_nonce)
    {
        JSON_STREAM_GEN_ADD_UINT32(p_gen, "nonce", p_ctx->nonce);
    }
    JSON_STREAM_GEN_ADD_STRING(p_gen, "gw_mac", p_ctx->gw_mac.str_buf);

    JSON_STREAM_GEN_START_OBJECT(p_gen, "tags");

    for (num_of_advs_t i = 0; i < p_ctx->reports.num_of_advs; ++i)
    {
        const adv_report_t* const p_adv = &p_ctx->reports.table[i];

        const mac_address_str_t mac_str = mac_address_to_str(&p_adv->tag_mac);
        JSON_STREAM_GEN_START_OBJECT(p_gen, mac_str.str_buf);
        JSON_STREAM_GEN_ADD_INT32(p_gen, "rssi", p_adv->rssi);

        if (p_ctx->flag_use_timestamps)
        {
            JSON_STREAM_GEN_ADD_UINT32(p_gen, "timestamp", p_adv->timestamp);
        }
        else
        {
            JSON_STREAM_GEN_ADD_UINT32(p_gen, "counter", p_adv->timestamp);
        }
        JSON_STREAM_GEN_ADD_HEX_BUF(p_gen, "data", p_adv->data_buf, p_adv->data_len);

        if (p_ctx->flag_decode)
        {
            if (re_5_check_format(p_adv->data_buf))
            {
                re_5_data_t       data          = { 0 };
                const re_status_t decode_status = re_5_decode(p_adv->data_buf, &data);
                if (RE_SUCCESS == decode_status)
                {
                    JSON_STREAM_GEN_ADD_INT32(p_gen, "dataFormat", RE_5_DESTINATION);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "temperature",
                        data.temperature_c,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_3);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "humidity",
                        data.humidity_rh,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_4);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "pressure",
                        data.pressure_pa,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "accelX",
                        data.accelerationx_g,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_3);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "accelY",
                        data.accelerationy_g,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_3);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "accelZ",
                        data.accelerationz_g,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_3);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "movementCounter",
                        data.movement_count,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "voltage",
                        data.battery_v,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_3);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "txPower",
                        RE_5_TXPWR_MIN + (data.tx_power * 2),
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
                    JSON_STREAM_GEN_ADD_INT32(p_gen, "measurementSequenceNumber", data.measurement_count);
                    mac_address_bin_t tag_mac = { 0 };
                    for (uint32_t mac_idx = 0; mac_idx < MAC_ADDRESS_NUM_BYTES; ++mac_idx)
                    {
                        tag_mac.mac[mac_idx] = (data.address
                                                >> ((MAC_ADDRESS_NUM_BYTES - mac_idx - 1U) * NUM_BITS_PER_BYTE))
                                               & BYTE_MASK;
                    }
                    const mac_address_str_t tag_mac_str = mac_address_to_str(&tag_mac);
                    JSON_STREAM_GEN_ADD_STRING(p_gen, "id", tag_mac_str.str_buf);
                }
            }
            if (re_6_check_format(p_adv->data_buf))
            {
                re_6_data_t       data          = { 0 };
                const re_status_t decode_status = re_6_decode(p_adv->data_buf, &data);
                if (RE_SUCCESS == decode_status)
                {
                    JSON_STREAM_GEN_ADD_INT32(p_gen, "dataFormat", RE_6_DESTINATION);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "temperature",
                        data.temperature_c,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "humidity",
                        data.humidity_rh,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "PM1.0",
                        data.pm1p0_ppm,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "PM2.5",
                        data.pm2p5_ppm,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "PM4.0",
                        data.pm4p0_ppm,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "PM10.0",
                        data.pm10p0_ppm,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "CO2",
                        data.co2,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "VOC",
                        data.voc_index,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
                    JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
                        p_gen,
                        "NOx",
                        data.nox_index,
                        JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
                    JSON_STREAM_GEN_ADD_INT32(p_gen, "measurementSequenceNumber", data.measurement_count);
                    mac_address_bin_t tag_mac = { 0 };
                    for (uint32_t mac_idx = 0; mac_idx < MAC_ADDRESS_NUM_BYTES; ++mac_idx)
                    {
                        tag_mac.mac[mac_idx] = (data.address
                                                >> ((MAC_ADDRESS_NUM_BYTES - mac_idx - 1U) * NUM_BITS_PER_BYTE))
                                               & BYTE_MASK;
                    }
                    const mac_address_str_t tag_mac_str = mac_address_to_str(&tag_mac);
                    JSON_STREAM_GEN_ADD_STRING(p_gen, "id", tag_mac_str.str_buf);
                }
            }
        }
        JSON_STREAM_GEN_END_OBJECT(p_gen);
    }

    JSON_STREAM_GEN_END_OBJECT(p_gen);
    JSON_STREAM_GEN_END_OBJECT(p_gen);
    JSON_STREAM_GEN_END_GENERATOR_FUNC();
}

json_stream_gen_t*
http_json_create_stream_gen_advs(
    const adv_report_table_t* const                        p_reports,
    const http_json_create_stream_gen_advs_params_t* const p_params)
{
    const json_stream_gen_cfg_t cfg = {
        .max_chunk_size      = 768U,
        .flag_formatted_json = true,
        .indentation_mark    = ' ',
        .indentation         = 2,
        .max_nesting_level   = 4,
        .p_malloc            = &os_malloc,
        .p_free              = &os_free_internal,
        .p_localeconv        = NULL,
    };
    http_json_stream_gen_advs_ctx_t* p_ctx = NULL;
    json_stream_gen_t* p_gen = json_stream_gen_create(&cfg, &cb_json_stream_gen_advs, sizeof(*p_ctx), (void**)&p_ctx);
    if (NULL == p_gen)
    {
        LOG_ERR("Not enough memory");
        return NULL;
    }
    p_ctx->flag_decode         = p_params->flag_decode;
    p_ctx->flag_use_timestamps = p_params->flag_use_timestamps;
    p_ctx->flag_use_nonce      = p_params->flag_use_nonce;
    p_ctx->timestamp           = p_params->cur_time;
    p_ctx->nonce               = p_params->nonce;
    p_ctx->gw_mac              = *p_params->p_mac_addr;
    p_ctx->coordinates         = *p_params->p_coordinates;
    memcpy(&p_ctx->reports, p_reports, sizeof(*p_reports));
    return p_gen;
}
