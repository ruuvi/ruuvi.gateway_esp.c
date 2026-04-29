// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _HTTP_HEADER_H_
#define _HTTP_HEADER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sys/queue.h"
#include "esp_err.h"
#include "esp_http_client_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_header *http_header_handle_t;
typedef struct http_header_item *http_header_item_handle_t;

typedef enum http_header_generate_item_stage_e {
    HTTP_HEADER_GENERATE_ITEM_STAGE_INIT,
    HTTP_HEADER_GENERATE_ITEM_STAGE_KEY,
    HTTP_HEADER_GENERATE_ITEM_STAGE_KEY_VAL_SEPARATOR,
    HTTP_HEADER_GENERATE_ITEM_STAGE_VALUE,
    HTTP_HEADER_GENERATE_ITEM_STAGE_KEY_VAL_EOL,
    HTTP_HEADER_GENERATE_ITEM_STAGE_FINISHED,
} http_header_generate_item_stage_e;

typedef struct http_header_generate_item_state_t {
    http_header_generate_item_stage_e stage;
} http_header_generate_item_state_t;

typedef enum http_header_generate_stage_e {
    HTTP_HEADER_GENERATE_STAGE_INIT,
    HTTP_HEADER_GENERATE_STAGE_ADDING_ITEMS,
    HTTP_HEADER_GENERATE_STAGE_ADDING_EXTRA_HEADERS,
    HTTP_HEADER_GENERATE_STAGE_FINAL_EOL,
    HTTP_HEADER_GENERATE_STAGE_FINISHED,
} http_header_generate_stage_e;

typedef struct http_header_generate_state_t {
    http_header_generate_stage_e stage;
    int32_t item_idx;
    http_header_generate_item_state_t item_state;
} http_header_generate_state_t;

/**
 * @brief      initialize and allocate the memory for the header object
 *
 * @return
 *     - http_header_handle_t
 *     - NULL if any errors
 */
http_header_handle_t http_header_init(void);

/**
 * @brief      Cleanup and free all http header pairs
 *
 * @param[in]  header  The header
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t http_header_clean(http_header_handle_t header);

/**
 * @brief      Cleanup with http_header_clean and destroy http header handle object
 *
 * @param[in]  header  The header
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t http_header_destroy(http_header_handle_t header);

/**
 * @brief      Add a key-value pair of http header to the list,
 *             note that with value = NULL, this function will remove the header with `key` already exists in the list.
 *
 * @param[in]  header  The header
 * @param[in]  key     The key
 * @param[in]  value   The value
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t http_header_set(http_header_handle_t header, const char *key, const char *value);

esp_err_t http_header_set_from_stream(http_header_handle_t header, const char *key,
                                      http_stream_reader_t cb_reader, void *const p_param);

/**
 * @brief      Sample as `http_header_set` but the value can be formated
 *
 * @param[in]  header     The header
 * @param[in]  key        The key
 * @param[in]  format     The format
 * @param[in]  ...        format parameters
 *
 * @return     Total length of value
 */
int http_header_set_format(http_header_handle_t header, const char *key, const char *format, ...);

/**
 * @brief Check if a header with the specified key exists in the header list.
 *
 * This function searches the HTTP header list for a header with the given key.
 *
 * @param[in] header  The HTTP header handle.
 * @param[in] key     The header key to search for.
 *
 * @return
 *     - true if the header with the specified key exists.
 *     - false if the header does not exist or on error.
 */
bool http_header_is_exist(http_header_handle_t header, const char *key);

/**
 * @brief Create an HTTP header string from the header list, optionally including extra headers, and output to a buffer.
 *
 * This function generates a complete HTTP header string from the provided header list and writes it to the given buffer.
 * It can also include additional headers provided by an extra stream reader. The function supports incremental generation
 * by maintaining state between calls, allowing it to handle large headers or small output buffers.
 *
 * @param[in]     p_stream_reader_ctx              Pointer to a stream reader context used for string generation.
 * @param[in]     p_stream_reader_last_call        Pointer to a structure for tracking the last stream reader called.
 * @param[in]     cb_extra_headers_stream_reader   Optional stream reader callback for extra headers (can be NULL).
 * @param[in]     p_cb_extra_headers_stream_reader_param Parameter for the extra headers stream reader (can be NULL).
 * @param[in]     header                          The HTTP header handle.
 * @param[in,out] p_state                         Pointer to the state variable to keep track of the generation process.
 *                                                Should be initialized with stage = HTTP_HEADER_GENERATE_STAGE_INIT and
 *                                                item_state.stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT before
 *                                                the first call to this function.
 * @param[out]    p_buf                           Pointer to the buffer to store the generated header string.
 *                                                The generated string will be null-terminated if there is enough space.
 * @param[in]     buf_len                         The length of the buffer.
 * @param[in,out] p_buf_ofs                       Pointer to the variable to update the offset in the buffer.
 *
 * @return
 *     - true if the operation completed successfully.
 *     - false if any error occurs during the generation process.
 */
bool http_header_generate_string_with_extra(void *const p_stream_reader_ctx,
                                            http_stream_last_call_t *const p_stream_reader_last_call,
                                            http_stream_reader_t const cb_extra_headers_stream_reader,
                                            void *const p_cb_extra_headers_stream_reader_param,
                                            http_header_handle_t header,
                                            http_header_generate_state_t *const p_state,
                                            char *const p_buf,
                                            const size_t buf_len,
                                            size_t *const p_buf_ofs);

/**
 * @brief Create an HTTP header string from the header list and output to a buffer.
 *
 * This function generates a complete HTTP header string from the provided header list and writes it to the given buffer.
 * The function supports incremental generation by maintaining state between calls, allowing it to handle large headers
 * or small output buffers.
 *
 * @param[in]     p_stream_reader_ctx       Pointer to a stream reader context used for string generation.
 * @param[in]     p_stream_reader_last_call Pointer to a structure for tracking the last stream reader called.
 * @param[in]     header                    The HTTP header handle.
 * @param[in,out] p_state                   Pointer to the state variable to keep track of the generation process.
 *                                          Should be initialized with stage = HTTP_HEADER_GENERATE_STAGE_INIT and
 *                                          item_state.stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT before
 *                                          the first call to this function.
 * @param[out]    p_buf                     Pointer to the buffer to store the generated header string.
 *                                          The generated string will be null-terminated if there is enough space.
 * @param[in]     buf_len                   The length of the buffer.
 * @param[in,out] p_buf_ofs                 Pointer to the variable to update the offset in the buffer.
 *
 * @return
 *     - true if the operation completed successfully.
 *     - false if any error occurs during the generation process.
 */
bool http_header_generate_string(void *const p_stream_reader_ctx,
                                 http_stream_last_call_t *const p_stream_reader_last_call,
                                 http_header_handle_t header,
                                 http_header_generate_state_t *const p_state,
                                 char *const p_buf,
                                 const size_t buf_len,
                                 size_t *const p_buf_ofs);

/**
 * @brief      Remove the header with key from the headers list
 *
 * @param[in]  header  The header
 * @param[in]  key     The key
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t http_header_delete(http_header_handle_t header, const char *key);

/**
 * @brief Count the number of header items in the HTTP header list.
 *
 * @param[in]  header  The header
 *
 * @return     The number of header items.
 */
int http_header_count(http_header_handle_t header);

#ifdef __cplusplus
}
#endif

#endif
