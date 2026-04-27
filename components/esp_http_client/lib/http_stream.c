/**
 * @file http_stream.c
 * @author TheSomeMan
 * @date 2026-04-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "esp_http_client_stream.h"
#include <assert.h>
#include <string.h>

ssize_t http_stream_reader_string(const http_stream_reader_cmd_e cmd,
                                  const http_stream_reader_arg_t arg,
                                  void *const p_ctx) {
    http_stream_reader_string_ctx_t *const p_context = p_ctx;
    switch (cmd) {
        case HTTP_STREAM_READER_CMD_OPEN:
            p_context->p_str = arg.open.p_param;
            p_context->data_offset = 0;
            p_context->p_last_call = arg.open.p_last_call;
            if (NULL != p_context->p_last_call) {
                p_context->p_last_call->cb_stream_reader = &http_stream_reader_string;
                p_context->p_last_call->p_ctx = p_ctx;
            }
            return 0;
        case HTTP_STREAM_READER_CMD_READ: {
            const size_t len = strlen(&p_context->p_str[p_context->data_offset]);
            if (len >= arg.read.buf_len) {
                memcpy(arg.read.p_buf, &p_context->p_str[p_context->data_offset], arg.read.buf_len - 1);
                p_context->data_offset += (arg.read.buf_len - 1);
                arg.read.p_buf[arg.read.buf_len - 1] = '\0';
                return (ssize_t) (arg.read.buf_len - 1);
            }
            strcpy(arg.read.p_buf, &p_context->p_str[p_context->data_offset]);
            p_context->data_offset += len;
            return (ssize_t) len;
        }
        case HTTP_STREAM_READER_CMD_CLOSE:
            p_context->p_str = NULL;
            p_context->data_offset = 0;
            if (NULL != p_context->p_last_call) {
                p_context->p_last_call->cb_stream_reader = NULL;
                p_context->p_last_call->p_ctx = NULL;
                p_context->p_last_call = NULL;
            }
            return 0;
    }
    assert(0);
    return -1;
}

void http_stream_reader_string_open(http_stream_reader_string_ctx_t *const p_ctx, const char *const p_str,
                                    http_stream_last_call_t *const p_stream_reader_last_call) {
    (void) http_stream_reader_string(HTTP_STREAM_READER_CMD_OPEN,
                                     (http_stream_reader_arg_t){
                                         .open = {
                                             .p_param = (void *) p_str,
                                             .p_last_call = p_stream_reader_last_call,
                                         },
                                     },
                                     p_ctx);
}

void http_stream_reader_string_close(http_stream_reader_string_ctx_t *const p_ctx) {
    (void) http_stream_reader_string(HTTP_STREAM_READER_CMD_CLOSE,
                                     (http_stream_reader_arg_t){
                                         .close = {},
                                     },
                                     p_ctx);
}

bool http_stream_reader_wrap_open(http_stream_reader_t stream_reader, void *const p_ctx, void *const p_param,
                                  http_stream_last_call_t *const p_stream_reader_last_call) {
    const ssize_t res = stream_reader(HTTP_STREAM_READER_CMD_OPEN, (http_stream_reader_arg_t){
                                          .open = {
                                              .p_param = p_param,
                                              .p_last_call = p_stream_reader_last_call,
                                          },
                                      }, p_ctx);
    if (0 != res) {
        return false;
    }
    return true;
}

void http_stream_reader_wrap_close(http_stream_reader_t stream_reader, void *const p_ctx) {
    (void) stream_reader(HTTP_STREAM_READER_CMD_CLOSE, (http_stream_reader_arg_t){
                             .close = {},
                         }, p_ctx);
}

bool http_stream_reader_wrap_read(http_stream_reader_t stream_reader,
                                  void *const p_ctx,
                                  char *const p_buf,
                                  const ssize_t buf_len,
                                  size_t *const p_buf_offset,
                                  bool *const p_is_buf_full) {
    if ((*p_buf_offset + 1) >= buf_len) {
        *p_is_buf_full = true;
        if (*p_buf_offset < buf_len) {
            p_buf[*p_buf_offset] = '\0';
        }
        return true;
    }
    const ssize_t len = stream_reader(HTTP_STREAM_READER_CMD_READ, (http_stream_reader_arg_t){
                                          .read = {
                                              .p_buf = &p_buf[*p_buf_offset],
                                              .buf_len = buf_len - *p_buf_offset,
                                          },
                                      }, p_ctx);
    if (len < 0) {
        return false;
    }

    *p_buf_offset += len;
    if (*p_buf_offset < buf_len) {
        p_buf[*p_buf_offset] = '\0';
    }
    if (*p_buf_offset < (buf_len - 1)) {
        *p_is_buf_full = false;
    } else {
        *p_is_buf_full = true;
    }
    return true;
}

bool http_stream_reader_wrap_read_string(void *const p_ctx,
                                         char *const p_buf,
                                         const ssize_t buf_len,
                                         size_t *const p_buf_offset,
                                         bool *const p_is_buf_full) {
    return http_stream_reader_wrap_read(&http_stream_reader_string, p_ctx, p_buf, buf_len, p_buf_offset, p_is_buf_full);
}
