/**
 * @author TheSomeMan
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_decode.h"
#include "ruuvi_endpoint_7.h"

static mac_address_str_t
re_mac_addr24_to_str(const uint64_t mac24)
{
    mac_address_str_t mac_str = { 0 };
    str_buf_t         str_buf = {
                .buf  = mac_str.str_buf,
                .size = sizeof(mac_str.str_buf),
                .idx  = 0,
    };
    str_buf_printf(
        &str_buf,
        "%02X:%02X:%02X",
        (uint8_t)((mac24 >> 16U) & 0xFFU),
        (uint8_t)((mac24 >> 8U) & 0xFFU),
        (uint8_t)((mac24 >> 0U) & 0xFFU));
    return mac_str;
}

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_df7_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv)
{
    re_7_data_t       data          = { 0 };
    const re_status_t decode_status = re_7_decode(p_adv->data_buf, &data);
    if (RE_SUCCESS == decode_status)
    {
        JSON_STREAM_GEN_ADD_INT32(p_gen, "dataFormat", RE_7_DESTINATION);
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
            "tiltX",
            data.tilt_x_deg,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);
        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "tiltY",
            data.tilt_y_deg,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_1);

        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "luminosity",
            data.luminosity_lux,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);

        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "colorTemperature",
            data.color_temp_k,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_0);

        JSON_STREAM_GEN_ADD_FLOAT_LIMITED_FIXED_POINT(
            p_gen,
            "battery",
            data.battery_v,
            JSON_STREAM_GEN_NUM_DECIMALS_FLOAT_3);

        JSON_STREAM_GEN_ADD_INT32(p_gen, "motionIntensity", data.motion_intensity);
        JSON_STREAM_GEN_ADD_INT32(p_gen, "motionCount", data.motion_count);
        JSON_STREAM_GEN_ADD_INT32(p_gen, "measurementSequenceNumber", data.sequence_counter);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "motionDetected", data.motion_detected);
        JSON_STREAM_GEN_ADD_BOOL(p_gen, "presenceDetected", data.presence_detected);

        const mac_address_str_t tag_mac_str = re_mac_addr24_to_str(data.address);
        JSON_STREAM_GEN_ADD_STRING(p_gen, "id", tag_mac_str.str_buf);
    }
    JSON_STREAM_GEN_END_GENERATOR_SUB_FUNC();
}
