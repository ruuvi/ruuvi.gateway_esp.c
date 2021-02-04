/**
 * @file http_json.c
 * @author TheSomeMan
 * @date 2021-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_json.h"

static cJSON *
http_generate_json_data_attributes(
    cJSON *const             p_json_data,
    const time_t             timestamp,
    const mac_address_str_t *p_mac_addr,
    const char *             p_coordinates_str)
{
    if (NULL == cJSON_AddStringToObject(p_json_data, "coordinates", p_coordinates_str))
    {
        return NULL;
    }
    if (!cjson_wrap_add_timestamp(p_json_data, "timestamp", timestamp))
    {
        return NULL;
    }
    if (NULL == cJSON_AddStringToObject(p_json_data, "gw_mac", p_mac_addr->str_buf))
    {
        return NULL;
    }
    return cJSON_AddObjectToObject(p_json_data, "tags");
}

static bool
http_generate_json_tag_mac_section(cJSON *const p_json_tags, const adv_report_t *const p_adv)
{
    const mac_address_str_t mac_str    = mac_address_to_str(&p_adv->tag_mac);
    cJSON *const            p_json_tag = cJSON_AddObjectToObject(p_json_tags, mac_str.str_buf);
    if (NULL == p_json_tag)
    {
        return false;
    }
    if (NULL == cJSON_AddNumberToObject(p_json_tag, "rssi", p_adv->rssi))
    {
        return false;
    }
    if (!cjson_wrap_add_timestamp(p_json_tag, "timestamp", p_adv->timestamp))
    {
        return false;
    }
    if (NULL == cJSON_AddStringToObject(p_json_tag, "data", p_adv->data))
    {
        return false;
    }
    return true;
}

static bool
http_generate_json_data_section(
    cJSON *const                    p_json_root,
    const adv_report_table_t *const p_reports,
    const time_t                    timestamp,
    const mac_address_str_t *       p_mac_addr,
    const char *                    p_coordinates_str)
{
    cJSON *const p_json_data = cJSON_AddObjectToObject(p_json_root, "data");
    if (NULL == p_json_data)
    {
        return false;
    }

    cJSON *const p_json_tags = http_generate_json_data_attributes(
        p_json_data,
        timestamp,
        p_mac_addr,
        p_coordinates_str);
    if (NULL == p_json_tags)
    {
        return false;
    }

    for (num_of_advs_t i = 0; i < p_reports->num_of_advs; ++i)
    {
        if (!http_generate_json_tag_mac_section(p_json_tags, &p_reports->table[i]))
        {
            return false;
        }
    }
    return true;
}

static cJSON *
http_generate_json(
    const adv_report_table_t *const p_reports,
    const time_t                    timestamp,
    const mac_address_str_t *       p_mac_addr,
    const char *                    p_coordinates_str)
{
    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        return NULL;
    }
    if (!http_generate_json_data_section(p_json_root, p_reports, timestamp, p_mac_addr, p_coordinates_str))
    {
        cjson_wrap_delete(&p_json_root);
        return NULL;
    }
    return p_json_root;
}

bool
http_create_json_str(
    const adv_report_table_t *const p_reports,
    const time_t                    timestamp,
    const mac_address_str_t *const  p_mac_addr,
    const char *const               p_coordinates_str,
    cjson_wrap_str_t *const         p_json_str)
{
    cJSON *p_json_root = http_generate_json(p_reports, timestamp, p_mac_addr, p_coordinates_str);
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
