/**
 * @file http_stream_reader_nvs.h
 * @author TheSomeMan
 * @date 2026-05-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_STREAM_READER_NVS_H
#define RUUVI_GATEWAY_ESP_HTTP_STREAM_READER_NVS_H

#include <sys/types.h>
#include "nvs.h"
#include "esp_http_client_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Context structure for file-based HTTP stream reader.
 */
typedef struct http_stream_reader_nvs_ctx_t
{
    const char*  p_filename;  /*!< The source file to read from */
    nvs_handle_t handle;      /*!< NVS handle for the opened file */
    size_t       data_offset; /*!< The current offset in the source file */
    size_t       file_size;   /*!< The total size of the file content */
} http_stream_reader_nvs_ctx_t;

ssize_t
http_stream_reader_nvs(const http_stream_reader_cmd_e cmd, const http_stream_reader_arg_t arg, void* const p_ctx);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_STREAM_READER_NVS_H
