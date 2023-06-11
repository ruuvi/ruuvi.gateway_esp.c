/**
 * @file http_json.c
 * @author TheSomeMan
 * @date 2021-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_json.h"
#include <math.h>
#include "bin2hex.h"
#include "os_malloc.h"
#include "runtime_stat.h"
#include "ruuvi_endpoint_5.h"
#include "ruuvi_endpoint_6.h"
#include "esp_type_wrapper.h"

#define NUM_BITS_PER_BYTE (8U)
#define BYTE_MASK         (0xFFU)

#define HTTP_JSON_STR_BUF_SIZE_FLOAT (24U)
#define HTTP_JSON_MULTIPLIER_10      (10)
#define HTTP_JSON_MULTIPLIER_1000    (1000)
#define HTTP_JSON_MULTIPLIER_10000   (10000)

static cJSON*
http_json_generate_records_data_attributes(cJSON* const p_json_data, const http_json_header_info_t header_info)
{
    if (NULL == cJSON_AddStringToObject(p_json_data, "coordinates", header_info.p_coordinates_str))
    {
        return NULL;
    }
    if (header_info.flag_use_timestamps && (!cjson_wrap_add_timestamp(p_json_data, "timestamp", header_info.timestamp)))
    {
        return NULL;
    }
    if (header_info.flag_use_nonce && (!cjson_wrap_add_uint32(p_json_data, "nonce", header_info.nonce)))
    {
        return NULL;
    }
    if (NULL == cJSON_AddStringToObject(p_json_data, "gw_mac", header_info.p_mac_addr->str_buf))
    {
        return NULL;
    }
    return cJSON_AddObjectToObject(p_json_data, "tags");
}

static bool
http_json_add_number_with_0_digits_after_point(cJSON* const p_cjson, const char* const p_name, const float val)
{
    if (isnanf(val) || isinff(val))
    {
        if (NULL == cJSON_AddRawToObject(p_cjson, p_name, "null"))
        {
            return false;
        }
        return true;
    }
    const uint32_t integral_part = (uint32_t)lrintf(fabsf(val));
    char           tmp_buf[HTTP_JSON_STR_BUF_SIZE_FLOAT];
    (void)snprintf(tmp_buf, sizeof(tmp_buf), "%s%lu", (val < 0) ? "-" : "", (printf_ulong_t)integral_part);
    if (NULL == cJSON_AddRawToObject(p_cjson, p_name, tmp_buf))
    {
        return false;
    }
    return true;
}

static bool
http_json_add_number_with_1_digit_after_point(cJSON* const p_cjson, const char* const p_name, const float val)
{
    if (isnanf(val) || isinff(val))
    {
        if (NULL == cJSON_AddRawToObject(p_cjson, p_name, "null"))
        {
            return false;
        }
        return true;
    }
    const uint32_t int_val         = (uint32_t)lrintf(fabsf(val * HTTP_JSON_MULTIPLIER_10));
    const uint32_t integral_part   = int_val / HTTP_JSON_MULTIPLIER_10;
    const uint32_t fractional_part = int_val % HTTP_JSON_MULTIPLIER_10;
    char           tmp_buf[HTTP_JSON_STR_BUF_SIZE_FLOAT];
    (void)snprintf(
        tmp_buf,
        sizeof(tmp_buf),
        "%s%lu.%u",
        (val < 0) ? "-" : "",
        (printf_ulong_t)integral_part,
        (printf_uint_t)fractional_part);
    if (NULL == cJSON_AddRawToObject(p_cjson, p_name, tmp_buf))
    {
        return false;
    }
    return true;
}

static bool
http_json_add_number_with_3_digits_after_point(cJSON* const p_cjson, const char* const p_name, const float val)
{
    if (isnanf(val) || isinff(val))
    {
        if (NULL == cJSON_AddRawToObject(p_cjson, p_name, "null"))
        {
            return false;
        }
        return true;
    }
    const uint32_t int_val         = (uint32_t)lrintf(fabsf(val * HTTP_JSON_MULTIPLIER_1000));
    const uint32_t integral_part   = int_val / HTTP_JSON_MULTIPLIER_1000;
    const uint32_t fractional_part = int_val % HTTP_JSON_MULTIPLIER_1000;
    char           tmp_buf[HTTP_JSON_STR_BUF_SIZE_FLOAT];
    (void)snprintf(
        tmp_buf,
        sizeof(tmp_buf),
        "%s%lu.%03u",
        (val < 0) ? "-" : "",
        (printf_ulong_t)integral_part,
        (printf_uint_t)fractional_part);
    if (NULL == cJSON_AddRawToObject(p_cjson, p_name, tmp_buf))
    {
        return false;
    }
    return true;
}

static bool
http_json_add_number_with_4_digits_after_point(cJSON* const p_cjson, const char* const p_name, const float val)
{
    if (isnanf(val) || isinff(val))
    {
        if (NULL == cJSON_AddRawToObject(p_cjson, p_name, "null"))
        {
            return false;
        }
        return true;
    }
    const uint32_t int_val         = (uint32_t)lrintf(fabsf(val * HTTP_JSON_MULTIPLIER_10000));
    const uint32_t integral_part   = int_val / HTTP_JSON_MULTIPLIER_10000;
    const uint32_t fractional_part = int_val % HTTP_JSON_MULTIPLIER_10000;
    char           tmp_buf[HTTP_JSON_STR_BUF_SIZE_FLOAT];
    (void)snprintf(
        tmp_buf,
        sizeof(tmp_buf),
        "%s%lu.%04u",
        (val < 0) ? "-" : "",
        (printf_ulong_t)integral_part,
        (printf_uint_t)fractional_part);
    if (NULL == cJSON_AddRawToObject(p_cjson, p_name, tmp_buf))
    {
        return false;
    }
    return true;
}

static bool
http_json_generate_records_decode_format_5(cJSON* const p_json_tag, const uint8_t* const p_decoded_buf)
{
    re_5_data_t       data          = { 0 };
    const re_status_t decode_status = re_5_decode(p_decoded_buf, &data);
    if (RE_SUCCESS != decode_status)
    {
        return false;
    }
    if (NULL == cJSON_AddNumberToObject(p_json_tag, "dataFormat", RE_5_DESTINATION))
    {
        return false;
    }
    if (!http_json_add_number_with_3_digits_after_point(p_json_tag, "temperature", data.temperature_c))
    {
        return false;
    }
    if (!http_json_add_number_with_4_digits_after_point(p_json_tag, "humidity", data.humidity_rh))
    {
        return false;
    }
    if (!http_json_add_number_with_0_digits_after_point(p_json_tag, "pressure", data.pressure_pa + RE_5_PRES_MIN))
    {
        return false;
    }
    if (!http_json_add_number_with_3_digits_after_point(p_json_tag, "accelX", data.accelerationx_g))
    {
        return false;
    }
    if (!http_json_add_number_with_3_digits_after_point(p_json_tag, "accelY", data.accelerationy_g))
    {
        return false;
    }
    if (!http_json_add_number_with_3_digits_after_point(p_json_tag, "accelZ", data.accelerationz_g))
    {
        return false;
    }
    if (!http_json_add_number_with_0_digits_after_point(p_json_tag, "movementCounter", data.movement_count))
    {
        return false;
    }
    if (!http_json_add_number_with_3_digits_after_point(p_json_tag, "voltage", data.battery_v))
    {
        return false;
    }
    if (!http_json_add_number_with_0_digits_after_point(p_json_tag, "txPower", RE_5_TXPWR_MIN + (data.tx_power * 2)))
    {
        return false;
    }
    if (NULL == cJSON_AddNumberToObject(p_json_tag, "measurementSequenceNumber", data.measurement_count))
    {
        return false;
    }
    mac_address_bin_t tag_mac = { 0 };
    for (uint32_t i = 0; i < MAC_ADDRESS_NUM_BYTES; ++i)
    {
        tag_mac.mac[i] = (data.address >> ((MAC_ADDRESS_NUM_BYTES - i - 1U) * NUM_BITS_PER_BYTE)) & BYTE_MASK;
    }
    const mac_address_str_t tag_mac_str = mac_address_to_str(&tag_mac);
    if (NULL == cJSON_AddStringToObject(p_json_tag, "id", tag_mac_str.str_buf))
    {
        return false;
    }
    return true;
}

static bool
http_json_generate_records_decode_format_6(cJSON* const p_json_tag, const uint8_t* const p_decoded_buf)
{
    re_6_data_t       data          = { 0 };
    const re_status_t decode_status = re_6_decode(p_decoded_buf, &data);
    if (RE_SUCCESS != decode_status)
    {
        return false;
    }
    if (NULL == cJSON_AddNumberToObject(p_json_tag, "dataFormat", RE_6_DESTINATION))
    {
        return false;
    }
    if (!http_json_add_number_with_1_digit_after_point(p_json_tag, "temperature", data.temperature_c))
    {
        return false;
    }
    if (!http_json_add_number_with_1_digit_after_point(p_json_tag, "humidity", data.humidity_rh))
    {
        return false;
    }
    if (!http_json_add_number_with_1_digit_after_point(p_json_tag, "PM1.0", data.pm1p0_ppm))
    {
        return false;
    }
    if (!http_json_add_number_with_1_digit_after_point(p_json_tag, "PM2.5", data.pm2p5_ppm))
    {
        return false;
    }
    if (!http_json_add_number_with_1_digit_after_point(p_json_tag, "PM4.0", data.pm4p0_ppm))
    {
        return false;
    }
    if (!http_json_add_number_with_1_digit_after_point(p_json_tag, "PM10.0", data.pm10p0_ppm))
    {
        return false;
    }
    if (!http_json_add_number_with_0_digits_after_point(p_json_tag, "CO2", data.co2))
    {
        return false;
    }
    if (!http_json_add_number_with_0_digits_after_point(p_json_tag, "VOC", data.voc_index))
    {
        return false;
    }
    if (!http_json_add_number_with_0_digits_after_point(p_json_tag, "NOx", data.nox_index))
    {
        return false;
    }
    if (NULL == cJSON_AddNumberToObject(p_json_tag, "measurementSequenceNumber", data.measurement_count))
    {
        return false;
    }
    mac_address_bin_t tag_mac = { 0 };
    for (uint32_t i = 0; i < MAC_ADDRESS_NUM_BYTES; ++i)
    {
        tag_mac.mac[i] = (data.address >> ((MAC_ADDRESS_NUM_BYTES - i - 1U) * NUM_BITS_PER_BYTE)) & BYTE_MASK;
    }
    const mac_address_str_t tag_mac_str = mac_address_to_str(&tag_mac);
    if (NULL == cJSON_AddStringToObject(p_json_tag, "id", tag_mac_str.str_buf))
    {
        return false;
    }
    return true;
}

static bool
http_json_generate_records_tag_mac_section(
    cJSON* const              p_json_tags,
    const adv_report_t* const p_adv,
    const bool                flag_use_timestamps,
    const bool                flag_decode)
{
    const mac_address_str_t mac_str    = mac_address_to_str(&p_adv->tag_mac);
    cJSON* const            p_json_tag = cJSON_AddObjectToObject(p_json_tags, mac_str.str_buf);
    if (NULL == p_json_tag)
    {
        return false;
    }
    if (NULL == cJSON_AddNumberToObject(p_json_tag, "rssi", p_adv->rssi))
    {
        return false;
    }
    if (flag_use_timestamps)
    {
        if (!cjson_wrap_add_timestamp(p_json_tag, "timestamp", p_adv->timestamp))
        {
            return false;
        }
    }
    else
    {
        if (!cjson_wrap_add_timestamp(p_json_tag, "counter", p_adv->timestamp))
        {
            return false;
        }
    }
    char* p_hex_str = bin2hex_with_malloc(p_adv->data_buf, p_adv->data_len);
    if (NULL == p_hex_str)
    {
        return false;
    }

    if (NULL == cJSON_AddStringToObject(p_json_tag, "data", p_hex_str))
    {
        os_free(p_hex_str);
        return false;
    }
    uint8_t* p_decoded_buf = NULL;
    if (flag_decode)
    {
        size_t decoded_buf_len = 0;
        p_decoded_buf          = hex2bin_with_malloc(p_hex_str, &decoded_buf_len);
    }
    os_free(p_hex_str);

    if (NULL != p_decoded_buf)
    {
        if (re_5_check_format(p_decoded_buf))
        {
            if (!http_json_generate_records_decode_format_5(p_json_tag, p_decoded_buf))
            {
                os_free(p_decoded_buf);
                return false;
            }
        }
        if (re_6_check_format(p_decoded_buf))
        {
            if (!http_json_generate_records_decode_format_6(p_json_tag, p_decoded_buf))
            {
                os_free(p_decoded_buf);
                return false;
            }
        }
        os_free(p_decoded_buf);
    }
    return true;
}

static bool
http_json_generate_records_data_section(
    cJSON* const                    p_json_root,
    const adv_report_table_t* const p_reports,
    const http_json_header_info_t   header_info,
    const bool                      flag_decode)
{
    cJSON* const p_json_data = cJSON_AddObjectToObject(p_json_root, "data");
    if (NULL == p_json_data)
    {
        return false;
    }

    cJSON* const p_json_tags = http_json_generate_records_data_attributes(p_json_data, header_info);
    if (NULL == p_json_tags)
    {
        return false;
    }

    if (NULL != p_reports)
    {
        for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
        {
            if (!http_json_generate_records_tag_mac_section(
                    p_json_tags,
                    &p_reports->table[i],
                    header_info.flag_use_timestamps,
                    flag_decode))
            {
                return false;
            }
        }
    }
    return true;
}

static cJSON*
http_json_generate_records(
    const adv_report_table_t* const p_reports,
    const http_json_header_info_t   header_info,
    const bool                      flag_decode)
{
    cJSON* p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        return NULL;
    }
    if (!http_json_generate_records_data_section(p_json_root, p_reports, header_info, flag_decode))
    {
        cjson_wrap_delete(&p_json_root);
        return NULL;
    }
    return p_json_root;
}

bool
http_json_create_records_str(
    const adv_report_table_t* const p_reports,
    const http_json_header_info_t   header_info,
    const bool                      flag_decode,
    cjson_wrap_str_t* const         p_json_str)
{
    cJSON* p_json_root = http_json_generate_records(p_reports, header_info, flag_decode);
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
