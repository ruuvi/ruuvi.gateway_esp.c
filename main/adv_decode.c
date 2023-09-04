/**
 * @file adv_decode.c
 * @author TheSomeMan
 * @date 2023-08-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_decode.h"
#include "ruuvi_endpoint_5.h"
#include "ruuvi_endpoint_6.h"

#define NUM_BITS_PER_BYTE (8U)
#define BYTE_MASK         (0xFFU)

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_df5_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv)
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
            tag_mac.mac[mac_idx] = (data.address >> ((MAC_ADDRESS_NUM_BYTES - mac_idx - 1U) * NUM_BITS_PER_BYTE))
                                   & BYTE_MASK;
        }
        const mac_address_str_t tag_mac_str = mac_address_to_str(&tag_mac);
        JSON_STREAM_GEN_ADD_STRING(p_gen, "id", tag_mac_str.str_buf);
    }
    JSON_STREAM_GEN_END_GENERATOR_SUB_FUNC();
}

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_df6_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv)
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
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(p_gen, "CO2", data.co2, JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
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
            tag_mac.mac[mac_idx] = (data.address >> ((MAC_ADDRESS_NUM_BYTES - mac_idx - 1U) * NUM_BITS_PER_BYTE))
                                   & BYTE_MASK;
        }
        const mac_address_str_t tag_mac_str = mac_address_to_str(&tag_mac);
        JSON_STREAM_GEN_ADD_STRING(p_gen, "id", tag_mac_str.str_buf);
    }
    JSON_STREAM_GEN_END_GENERATOR_SUB_FUNC();
}
