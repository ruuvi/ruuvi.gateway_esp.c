/**
 * @file mqtt_json.c
 * @author TheSomeMan
 * @date 2021-02-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "mqtt_json.h"
#include "bin2hex.h"
#include "os_malloc.h"

static bool
mqtt_create_json_attributes(
    cJSON *const                   p_json_root,
    const adv_report_t *const      p_adv,
    const time_t                   timestamp,
    const mac_address_str_t *const p_mac_addr,
    const char *const              p_coordinates_str)
{
    if (NULL == cJSON_AddStringToObject(p_json_root, "gw_mac", p_mac_addr->str_buf))
    {
        return false;
    }
    if (NULL == cJSON_AddNumberToObject(p_json_root, "rssi", p_adv->rssi))
    {
        return false;
    }
    if (NULL == cJSON_AddArrayToObject(p_json_root, "aoa"))
    {
        return false;
    }
    if (!cjson_wrap_add_timestamp(p_json_root, "gwts", timestamp))
    {
        return false;
    }
    if (!cjson_wrap_add_timestamp(p_json_root, "ts", p_adv->timestamp))
    {
        return false;
    }
    char *p_hex_str = bin2hex_with_malloc(p_adv->data_buf, p_adv->data_len);
    if (NULL == p_hex_str)
    {
        return false;
    }
    if (NULL == cJSON_AddStringToObject(p_json_root, "data", p_hex_str))
    {
        os_free(p_hex_str);
        return false;
    }
    os_free(p_hex_str);
    if (NULL == cJSON_AddStringToObject(p_json_root, "coords", p_coordinates_str))
    {
        return false;
    }
    return true;
}

static cJSON *
mqtt_create_json(
    const adv_report_t *const      p_adv,
    const time_t                   timestamp,
    const mac_address_str_t *const p_mac_addr,
    const char *const              p_coordinates_str)
{
    cJSON *p_json_root = cJSON_CreateObject();
    if (NULL == p_json_root)
    {
        return NULL;
    }

    if (!mqtt_create_json_attributes(p_json_root, p_adv, timestamp, p_mac_addr, p_coordinates_str))
    {
        cjson_wrap_delete(&p_json_root);
        return NULL;
    }
    return p_json_root;
}

bool
mqtt_create_json_str(
    const adv_report_t *const      p_adv,
    const time_t                   timestamp,
    const mac_address_str_t *const p_mac_addr,
    const char *const              p_coordinates_str,
    cjson_wrap_str_t *const        p_json_str)
{
    cJSON *p_json_root = mqtt_create_json(p_adv, timestamp, p_mac_addr, p_coordinates_str);
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
