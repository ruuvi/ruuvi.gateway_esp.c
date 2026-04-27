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


#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_log.h"
#include "http_header.h"
#include "http_utils.h"
#include "esp_http_client_stream.h"

static const char *TAG = "HTTP_HEADER";
#define HEADER_BUFFER (1024)

/**
 * dictionary item struct, with key-value pair
 */
typedef struct http_header_item {
    char *key;                          /*!< key */
    char *value;                        /*!< value */
    STAILQ_ENTRY(http_header_item) next;   /*!< Point to next entry */
} http_header_item_t;

STAILQ_HEAD(http_header, http_header_item);


http_header_handle_t http_header_init(void)
{
    http_header_handle_t header = calloc(1, sizeof(struct http_header));
    HTTP_MEM_CHECK(TAG, header, return NULL);
    STAILQ_INIT(header);
    return header;
}

esp_err_t http_header_destroy(http_header_handle_t header)
{
    esp_err_t err = http_header_clean(header);
    free(header);
    return err;
}

http_header_item_handle_t http_header_get_item(http_header_handle_t header, const char *key)
{
    http_header_item_handle_t item;
    if (header == NULL || key == NULL) {
        return NULL;
    }
    STAILQ_FOREACH(item, header, next) {
        if (strcasecmp(item->key, key) == 0) {
            return item;
        }
    }
    return NULL;
}

esp_err_t http_header_get(http_header_handle_t header, const char *key, char **value)
{
    http_header_item_handle_t item;

    item = http_header_get_item(header, key);
    if (item) {
        *value = item->value;
    } else {
        *value = NULL;
    }

    return ESP_OK;
}

static esp_err_t http_header_new_item(http_header_handle_t header, const char *key, const char *value)
{
    http_header_item_handle_t item;

    item = calloc(1, sizeof(http_header_item_t));
    HTTP_MEM_CHECK(TAG, item, return ESP_ERR_NO_MEM);
    http_utils_assign_string(&item->key, key, -1);
    HTTP_MEM_CHECK(TAG, item->key, goto _header_new_item_exit);
    http_utils_trim_whitespace(&item->key);
    http_utils_assign_string(&item->value, value, -1);
    HTTP_MEM_CHECK(TAG, item->value, goto _header_new_item_exit);
    http_utils_trim_whitespace(&item->value);
    STAILQ_INSERT_TAIL(header, item, next);
    return ESP_OK;
_header_new_item_exit:
    free(item->key);
    free(item->value);
    free(item);
    return ESP_ERR_NO_MEM;
}

esp_err_t http_header_set(http_header_handle_t header, const char *key, const char *value)
{
    http_header_item_handle_t item;

    if (value == NULL) {
        return http_header_delete(header, key);
    }

    item = http_header_get_item(header, key);

    if (item) {
        free(item->value);
        item->value = strdup(value);
        http_utils_trim_whitespace(&item->value);
        return ESP_OK;
    }
    return http_header_new_item(header, key, value);
}

esp_err_t http_header_set_from_string(http_header_handle_t header, const char *key_value_data)
{
    char *eq_ch;
    char *p_str;

    p_str = strdup(key_value_data);
    HTTP_MEM_CHECK(TAG, p_str, return ESP_ERR_NO_MEM);
    eq_ch = strchr(p_str, ':');
    if (eq_ch == NULL) {
        free(p_str);
        return ESP_ERR_INVALID_ARG;
    }
    *eq_ch = 0;

    http_header_set(header, p_str, eq_ch + 1);
    free(p_str);
    return ESP_OK;
}


esp_err_t http_header_delete(http_header_handle_t header, const char *key)
{
    http_header_item_handle_t item = http_header_get_item(header, key);
    if (item) {
        STAILQ_REMOVE(header, item, http_header_item, next);
        free(item->key);
        free(item->value);
        free(item);
    } else {
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}


int http_header_set_format(http_header_handle_t header, const char *key, const char *format, ...)
{
    va_list argptr;
    int len = 0;
    char *buf = NULL;
    va_start(argptr, format);
    len = vasprintf(&buf, format, argptr);
    va_end(argptr);
    HTTP_MEM_CHECK(TAG, buf, return 0);
    if (buf == NULL) {
        return 0;
    }
    http_header_set(header, key, buf);
    free(buf);
    return len;
}

static bool generate_item_token(const char *const p_token,
                                char *const p_buf,
                                const ssize_t buf_len,
                                size_t *const p_buf_offset,
                                bool *const p_is_buf_full,
                                void *const p_stream_reader_ctx,
                                http_stream_last_call_t *const p_stream_reader_last_call) {
    http_stream_reader_string_ctx_t *const p_ctx = p_stream_reader_ctx;
    if (NULL == p_ctx->p_str) {
        http_stream_reader_string_open(p_ctx, p_token, p_stream_reader_last_call);
    }
    const bool res = http_stream_reader_wrap_read_string(p_ctx, p_buf, buf_len, p_buf_offset, p_is_buf_full);
    if (!res) {
        http_stream_reader_string_close(p_ctx);
        return false;
    }
    if (!*p_is_buf_full) {
        http_stream_reader_string_close(p_ctx);
    }
    return true;
}

static bool http_header_generate_item(const http_header_item_handle_t item,
                                      http_header_generate_item_state_t *const p_state,
                                      char *const p_buf,
                                      const size_t buf_len,
                                      size_t *const p_buf_ofs,
                                      bool *const p_is_buf_full,
                                      void *const p_stream_reader_ctx,
                                      http_stream_last_call_t *const p_stream_reader_last_call) {
    ESP_LOGD(TAG, "%s: Header item (stage=%d) %s: %s",
             __func__, p_state->stage, item->key, item->value);
    while (p_state->stage != HTTP_HEADER_GENERATE_ITEM_STAGE_FINISHED) {
        const size_t rem_buf_len = buf_len - *p_buf_ofs;
        if (rem_buf_len <= 1) {
            if (0 != rem_buf_len) {
                p_buf[*p_buf_ofs] = '\0'; // Add final '\0' to buffer
            }
            *p_is_buf_full = true;
            return true;
        }
        switch (p_state->stage) {
            case HTTP_HEADER_GENERATE_ITEM_STAGE_INIT:
                p_state->stage = HTTP_HEADER_GENERATE_ITEM_STAGE_KEY;
                break;
            case HTTP_HEADER_GENERATE_ITEM_STAGE_KEY:
                if (!generate_item_token(item->key, p_buf, buf_len, p_buf_ofs, p_is_buf_full,
                                         p_stream_reader_ctx, p_stream_reader_last_call)) {
                    return false;
                }
                if (!*p_is_buf_full) {
                    p_state->stage = HTTP_HEADER_GENERATE_ITEM_STAGE_KEY_VAL_SEPARATOR;
                }
                break;
            case HTTP_HEADER_GENERATE_ITEM_STAGE_KEY_VAL_SEPARATOR:
                if (!generate_item_token(": ", p_buf, buf_len, p_buf_ofs, p_is_buf_full,
                                         p_stream_reader_ctx, p_stream_reader_last_call)) {
                    return false;
                }
                if (!*p_is_buf_full) {
                    p_state->stage = HTTP_HEADER_GENERATE_ITEM_STAGE_VALUE;
                }
                break;
            case HTTP_HEADER_GENERATE_ITEM_STAGE_VALUE:
                if (!generate_item_token(item->value, p_buf, buf_len, p_buf_ofs, p_is_buf_full,
                                         p_stream_reader_ctx, p_stream_reader_last_call)) {
                    return false;
                }
                if (!*p_is_buf_full) {
                    p_state->stage = HTTP_HEADER_GENERATE_ITEM_STAGE_KEY_VAL_EOL;
                }
                break;
            case HTTP_HEADER_GENERATE_ITEM_STAGE_KEY_VAL_EOL:
                if (!generate_item_token("\r\n", p_buf, buf_len, p_buf_ofs, p_is_buf_full,
                                         p_stream_reader_ctx, p_stream_reader_last_call)) {
                    return false;
                }
                if (!*p_is_buf_full) {
                    p_state->stage = HTTP_HEADER_GENERATE_ITEM_STAGE_FINISHED;
                }
                break;
            case HTTP_HEADER_GENERATE_ITEM_STAGE_FINISHED:
                assert(0);
                break;
        }
    }
    if (*p_buf_ofs < buf_len) {
        p_buf[*p_buf_ofs] = '\0'; // Add final '\0' to buffer
    }
    p_state->stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT;
    return true;
}

static bool http_header_generate_string_items(http_header_handle_t header,
                                              http_header_generate_state_t *const p_state,
                                              char *const p_buf, const size_t buf_len, size_t *const p_buf_ofs,
                                              bool *const p_is_buf_full,
                                              void *const p_stream_reader_ctx,
                                              http_stream_last_call_t *const p_stream_reader_last_call) {
    http_header_item_handle_t item;
    int32_t idx = 0;
    STAILQ_FOREACH(item, header, next) {
        if ((NULL != item->value) && (idx >= p_state->item_idx)) {
            if (!http_header_generate_item(item, &p_state->item_state, p_buf, buf_len, p_buf_ofs, p_is_buf_full,
                                           p_stream_reader_ctx, p_stream_reader_last_call)) {
                return false;
            }
            if (*p_is_buf_full) {
                p_state->item_idx = idx;
                return true;
            }
        }
        idx += 1;
    }
    p_state->item_idx = idx;
    return true;
}

bool http_header_generate_string(
    void* const p_stream_reader_ctx,
    http_stream_last_call_t* const p_stream_reader_last_call,
    http_header_handle_t header, http_header_generate_state_t *const p_state,
                                 char *const p_buf, const size_t buf_len, size_t *const p_buf_ofs) {
    bool flag_buf_full = false;
    while (!flag_buf_full) {
        switch (p_state->stage) {
            case HTTP_HEADER_GENERATE_STAGE_INIT:
                p_state->item_idx = 0;
                p_state->item_state.stage = HTTP_HEADER_GENERATE_ITEM_STAGE_INIT;
                p_state->stage = HTTP_HEADER_GENERATE_STAGE_ADDING_ITEMS;
                break;
            case HTTP_HEADER_GENERATE_STAGE_ADDING_ITEMS:
                if (!http_header_generate_string_items(header, p_state, p_buf, buf_len, p_buf_ofs, &flag_buf_full,
                                                       p_stream_reader_ctx, p_stream_reader_last_call)) {
                    return false;
                }
                if (!flag_buf_full) {
                    p_state->stage = HTTP_HEADER_GENERATE_STAGE_FINAL_EOL;
                    p_state->item_state.stage = HTTP_HEADER_GENERATE_ITEM_STAGE_FINISHED;
                }
                break;
            case HTTP_HEADER_GENERATE_STAGE_FINAL_EOL: {
                if (!generate_item_token("\r\n", p_buf, buf_len, p_buf_ofs, &flag_buf_full,
                                         p_stream_reader_ctx, p_stream_reader_last_call)) {
                    return false;
                }
                if (!flag_buf_full) {
                    p_state->stage = HTTP_HEADER_GENERATE_STAGE_FINISHED;
                }
                break;
            }
            case HTTP_HEADER_GENERATE_STAGE_FINISHED:
                flag_buf_full = true;
                break;
        }
    }
    if (*p_buf_ofs < buf_len) {
        p_buf[*p_buf_ofs] = '\0';
    }
    return true;
}

esp_err_t http_header_clean(http_header_handle_t header)
{
    http_header_item_handle_t item = STAILQ_FIRST(header), tmp;
    while (item != NULL) {
        tmp = STAILQ_NEXT(item, next);
        free(item->key);
        free(item->value);
        free(item);
        item = tmp;
    }
    STAILQ_INIT(header);
    return ESP_OK;
}

int http_header_count(http_header_handle_t header)
{
    http_header_item_handle_t item;
    int count = 0;
    STAILQ_FOREACH(item, header, next) {
        count ++;
    }
    return count;
}
