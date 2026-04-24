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
    size_t offset;
} http_header_generate_item_state_t;

typedef enum http_header_generate_stage_e {
    HTTP_HEADER_GENERATE_STAGE_INIT,
    HTTP_HEADER_GENERATE_STAGE_ADDING_ITEMS,
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
 * @brief      Get a value of header in header list
 *             The address of the value will be assign set to `value` parameter or NULL if no header with the key exists in the list
 *
 * @param[in]  header  The header
 * @param[in]  key     The key
 * @param[out] value   The value
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t http_header_get(http_header_handle_t header, const char *key, char **value);

/**
 * @brief      Create HTTP header string from the header with index, output string to buffer with buffer_len
 *             Also return the last index of header was generated
 *
 * @param[in]  header      The header
 * @param[in]  index       The index
 * @param      buffer      The buffer
 * @param      buffer_len  The buffer length
 *
 * @return
 *     - Content length if header was generated successfully (value greater than 0)
 *     - 0 if generation is complete and there is no more header to generate
 */
size_t http_header_generate_string(http_header_handle_t header, http_header_generate_state_t* const p_state, char * const p_buf, const size_t buf_len);

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
