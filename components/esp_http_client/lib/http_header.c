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
 * @brief Structure representing a single HTTP header item (key-value pair).
 *
 * This struct stores an HTTP header entry, where the value is always provided by a stream reader callback.
 * This allows the value to be generated dynamically or streamed from an external source.
 *
 * - @c key is the header name.
 * - @c cb_reader is the stream reader callback used to get the header value.
 * - @c p_param is a user-defined parameter passed to the stream reader.
 *      In the case of string reader, p_param points to the statically or dynamically allocated string.
 * - @c flag_param_dyn_allocated is true if p_param was dynamically allocated and should be freed when the item is deleted.
 *
 * The struct is designed for use in a singly linked tail queue (STAILQ).
 */
typedef struct http_header_item {
    char *key;                           /*!< Header key (name). */
    bool flag_param_dyn_allocated;       /*!< True if p_param is dynamically allocated and must be freed. */
    http_stream_reader_t cb_reader;      /*!< Stream reader callback for dynamic value. */
    void* p_param;                       /*!< Parameter to pass to the stream reader. */
    STAILQ_ENTRY(http_header_item) next; /*!< Pointer to the next entry in the queue. */
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

bool http_header_is_exist(http_header_handle_t header, const char *key) {
    http_header_item_handle_t item = http_header_get_item(header, key);
    if (NULL == item) {
        return false;
    }
    if ((NULL == item->cb_reader) || (NULL == item->p_param)) {
        return false;
    }
    return true;
}

static esp_err_t http_header_new_item_string(http_header_handle_t header, const char *key, const char *value)
{
    http_header_item_handle_t item;

    item = calloc(1, sizeof(http_header_item_t));
    HTTP_MEM_CHECK(TAG, item, return ESP_ERR_NO_MEM);
    item->flag_param_dyn_allocated = true;
    http_utils_assign_string(&item->key, key, -1);
    HTTP_MEM_CHECK(TAG, item->key, goto _header_new_item_exit);
    http_utils_trim_whitespace(&item->key);
    char* p_value = NULL;
    http_utils_assign_string(&p_value, value, -1);
    HTTP_MEM_CHECK(TAG, p_value, goto _header_new_item_exit);
    http_utils_trim_whitespace(&p_value);
    item->cb_reader = &http_stream_reader_string;
    item->p_param = p_value;
    STAILQ_INSERT_TAIL(header, item, next);
    return ESP_OK;
_header_new_item_exit:
    free(item->key);
    free(item->p_param);
    free(item);
    return ESP_ERR_NO_MEM;
}

static esp_err_t http_header_new_item_stream(http_header_handle_t header, const char *key,
                                             http_stream_reader_t cb_reader, void *const p_param)
{
    http_header_item_handle_t item;

    item = calloc(1, sizeof(http_header_item_t));
    HTTP_MEM_CHECK(TAG, item, return ESP_ERR_NO_MEM);
    item->flag_param_dyn_allocated = false;
    http_utils_assign_string(&item->key, key, -1);
    HTTP_MEM_CHECK(TAG, item->key, goto _header_new_item_exit);
    http_utils_trim_whitespace(&item->key);
    item->cb_reader = cb_reader;
    item->p_param = p_param;
    STAILQ_INSERT_TAIL(header, item, next);
    return ESP_OK;
_header_new_item_exit:
    free(item->key);
    free(item);
    return ESP_ERR_NO_MEM;
}


esp_err_t http_header_set(http_header_handle_t header, const char *key, const char *value)
{
    if (value == NULL) {
        return http_header_delete(header, key);
    }

    http_header_item_handle_t item = http_header_get_item(header, key);

    if (item) {
        if (item->flag_param_dyn_allocated) {
            free(item->p_param);
            item->p_param = NULL;
        }
        item->flag_param_dyn_allocated = true;
        item->p_param = strdup(value);
        if (NULL == item->p_param) {
            return ESP_ERR_NO_MEM;
        }
        http_utils_trim_whitespace((char**)&item->p_param);
        return ESP_OK;
    }
    return http_header_new_item_string(header, key, value);
}

esp_err_t http_header_set_from_stream(http_header_handle_t header, const char *key,
                                      http_stream_reader_t cb_reader, void *const p_param) {
    if (cb_reader == NULL) {
        return http_header_delete(header, key);
    }

    http_header_item_handle_t item = http_header_get_item(header, key);

    if (item) {
        if (item->flag_param_dyn_allocated) {
            free(item->p_param);
            item->p_param = NULL;
        }
        item->flag_param_dyn_allocated = false;
        item->cb_reader = cb_reader;
        item->p_param = p_param;
        return ESP_OK;
    }
    return http_header_new_item_stream(header, key, cb_reader, p_param);
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
        item->key = NULL;
        if (item->flag_param_dyn_allocated) {
            free(item->p_param);
            item->p_param = NULL;
        }
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
             __func__, p_state->stage, item->key,
             (&http_stream_reader_string == item->cb_reader) ? (char*)item->p_param : "<stream_reader>");
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
                    if (!http_stream_reader_wrap_open(item->cb_reader, p_stream_reader_ctx, item->p_param,
                                                      p_stream_reader_last_call)) {
                        return false;
                    }
                }
                break;
            case HTTP_HEADER_GENERATE_ITEM_STAGE_VALUE: {
                const bool res = http_stream_reader_wrap_read(item->cb_reader, p_stream_reader_ctx, p_buf, buf_len, p_buf_ofs, p_is_buf_full);
                if (!res) {
                    http_stream_reader_wrap_close(item->cb_reader, p_stream_reader_ctx);
                    return false;
                }
                if (!*p_is_buf_full) {
                    http_stream_reader_wrap_close(item->cb_reader, p_stream_reader_ctx);
                    p_state->stage = HTTP_HEADER_GENERATE_ITEM_STAGE_KEY_VAL_EOL;
                }
                break;
            }
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
        if ((NULL != item->cb_reader) && (NULL != item->p_param) && (idx >= p_state->item_idx)) {
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

bool http_header_generate_string_with_extra(void *const p_stream_reader_ctx,
                                            http_stream_last_call_t *const p_stream_reader_last_call,
                                            http_stream_reader_t const cb_extra_headers_stream_reader,
                                            void *const p_cb_extra_headers_stream_reader_param,
                                            http_header_handle_t header,
                                            http_header_generate_state_t *const p_state,
                                            char *const p_buf,
                                            const size_t buf_len,
                                            size_t *const p_buf_ofs) {
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
                    p_state->item_state.stage = HTTP_HEADER_GENERATE_ITEM_STAGE_FINISHED;
                    p_state->stage = HTTP_HEADER_GENERATE_STAGE_ADDING_EXTRA_HEADERS;
                    if (NULL != cb_extra_headers_stream_reader) {
                        if (!http_stream_reader_wrap_open(cb_extra_headers_stream_reader,
                                                          p_stream_reader_ctx,
                                                          p_cb_extra_headers_stream_reader_param,
                                                          p_stream_reader_last_call)) {
                            return false;
                        }
                    }
                }
                break;
            case HTTP_HEADER_GENERATE_STAGE_ADDING_EXTRA_HEADERS:
                if (NULL != cb_extra_headers_stream_reader) {
                    if (!http_stream_reader_wrap_read(cb_extra_headers_stream_reader,
                                                      p_stream_reader_ctx,
                                                      p_buf,
                                                      buf_len,
                                                      p_buf_ofs,
                                                      &flag_buf_full)) {
                        http_stream_reader_wrap_close(cb_extra_headers_stream_reader, p_stream_reader_ctx);
                        return false;
                    }
                    if (!flag_buf_full) {
                        http_stream_reader_wrap_close(cb_extra_headers_stream_reader, p_stream_reader_ctx);
                    }
                }
                if (!flag_buf_full) {
                    p_state->stage = HTTP_HEADER_GENERATE_STAGE_FINAL_EOL;
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

bool http_header_generate_string(void *const p_stream_reader_ctx,
                                 http_stream_last_call_t *const p_stream_reader_last_call,
                                 http_header_handle_t header,
                                 http_header_generate_state_t *const p_state,
                                 char *const p_buf,
                                 const size_t buf_len,
                                 size_t *const p_buf_ofs) {
    return http_header_generate_string_with_extra(p_stream_reader_ctx,
                                     p_stream_reader_last_call,
                                     NULL,
                                     NULL,
                                     header,
                                     p_state,
                                     p_buf,
                                     buf_len,
                                     p_buf_ofs);

}

esp_err_t http_header_clean(http_header_handle_t header)
{
    http_header_item_handle_t item = STAILQ_FIRST(header), tmp;
    while (item != NULL) {
        tmp = STAILQ_NEXT(item, next);
        free(item->key);
        if (item->flag_param_dyn_allocated) {
            free(item->p_param);
        }
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
