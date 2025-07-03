/**
 * @file adv_decode.h
 * @author TheSomeMan
 * @date 2023-08-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_ADV_DECODE_H
#define RUUVI_GATEWAY_ESP_ADV_DECODE_H

#include "json_stream_gen.h"
#include "adv_table.h"

#ifdef __cplusplus
extern "C" {
#endif

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_df5_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv);

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_df6_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv);

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_dfxf0_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv);

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_dfxe0_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv);

JSON_STREAM_GEN_DECL_GENERATOR_SUB_FUNC(
    adv_decode_dfxe1_cb_json_stream_gen,
    json_stream_gen_t* const  p_gen,
    const adv_report_t* const p_adv);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_ADV_DECODE_H
