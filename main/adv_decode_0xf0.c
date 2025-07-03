/**
 * @author TheSomeMan
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_decode.h"
#include "ruuvi_endpoint_f0.h"

#define NUM_BITS_PER_BYTE (8U)
#define BYTE_MASK         (0xFFU)

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_dfxf0_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv)
{
    re_f0_data_t      data          = { 0 };
    const re_status_t decode_status = re_f0_decode(p_adv->data_buf, &data);
    if (RE_SUCCESS == decode_status)
    {
        JSON_STREAM_GEN_ADD_INT32(p_gen, "dataFormat", RE_F0_DESTINATION);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "temperature",
            data.temperature_c,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "humidity",
            data.humidity_rh,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "pressure",
            data.pressure_pa,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
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
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "luminosity",
            data.luminosity,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "sound_avg_dba",
            data.sound_avg_dba,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
        JSON_STREAM_GEN_ADD_INT32(p_gen, "measurementSequenceNumber", data.flag_seq_cnt);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "flag_usb_on", data.flag_usb_on);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "flag_low_battery", data.flag_low_battery);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "flag_calibration_in_progress", data.flag_calibration_in_progress);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "flag_boost_mode", data.flag_boost_mode);
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
