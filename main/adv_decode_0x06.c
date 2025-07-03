/**
 * @author TheSomeMan
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_decode.h"
#include "ruuvi_endpoint_6.h"

#define NUM_BITS_PER_BYTE (8U)
#define BYTE_MASK         (0xFFU)

static mac_address_str_t
re_mac_addr24_to_str(const re_6_mac_addr_24_t* const p_mac)
{
    mac_address_str_t mac_str = { 0 };
    str_buf_t         str_buf = {
                .buf  = mac_str.str_buf,
                .size = sizeof(mac_str.str_buf),
                .idx  = 0,
    };
    str_buf_printf(&str_buf, "%02X:%02X:%02X", p_mac->byte3, p_mac->byte4, p_mac->byte5);
    return mac_str;
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
            "pressure",
            data.pressure_pa,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "PM2.5",
            data.pm2p5_ppm,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(p_gen, "CO2", data.co2, JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(p_gen, "VOC", data.voc, JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(p_gen, "NOx", data.nox, JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);
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
        JSON_STREAM_GEN_ADD_INT32(p_gen, "measurementSequenceNumber", data.seq_cnt2);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "flag_calibration_in_progress", data.flags.flag_calibration_in_progress);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "flag_button_pressed", data.flags.flag_button_pressed);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "flag_rtc_running_on_boot", data.flags.flag_rtc_running_on_boot);
        const mac_address_str_t tag_mac_str = re_mac_addr24_to_str(&data.mac_addr_24);
        JSON_STREAM_GEN_ADD_STRING(p_gen, "id", tag_mac_str.str_buf);
    }
    JSON_STREAM_GEN_END_GENERATOR_SUB_FUNC();
}
