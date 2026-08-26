/**
 * @file http_stream_reader_nvs.c
 * @author TheSomeMan
 * @date 2026-05-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_stream_reader_nvs.h"
#include "esp_http_client_stream.h"
#include "esp_attr.h"
#include "nvs.h"
#include "ruuvi_nvs.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http";

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static ssize_t
http_stream_reader_nvs_handle_cmd_open(http_stream_reader_nvs_ctx_t* const p_context, const char* const p_filename)
{
    p_context->p_filename = p_filename;
    LOG_DBG("HTTP_STREAM_READER_CMD_OPEN: Read file '%s' from NVS", p_context->p_filename);
    if (!ruuvi_nvs_open_gw_cfg_storage(NVS_READONLY, &p_context->handle))
    {
        LOG_ERR("%s failed", "ruuvi_nvs_open_gw_cfg_storage");
        return -1;
    }
    LOG_DBG("p_context=%p, handle=%u", p_context, p_context->handle);
    const esp_err_t esp_err = nvs_get_blob(p_context->handle, p_context->p_filename, NULL, &p_context->file_size);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't find key '%s' in NVS", p_context->p_filename);
        nvs_close(p_context->handle);
        p_context->handle = 0;
        return -1;
    }
    LOG_DBG("File '%s' size=%zu", p_context->p_filename, p_context->file_size);
    if (0 == p_context->file_size)
    {
        LOG_ERR("File '%s' is empty", p_context->p_filename);
        nvs_close(p_context->handle);
        p_context->handle = 0;
        return -1;
    }

    p_context->data_offset = 0;
    return 0;
}

static ssize_t
http_stream_reader_nvs_handle_cmd_read(
    http_stream_reader_nvs_ctx_t* const p_context,
    char* const                         p_buf,
    const size_t                        buf_len)
{
    LOG_DBG("HTTP_STREAM_READER_CMD_READ: p_context=%p, handle=%u", p_context, p_context->handle);
    const size_t rem_len       = p_context->file_size - p_context->data_offset;
    const size_t bytes_to_read = ((buf_len - 1) < rem_len) ? (buf_len - 1) : rem_len;
    size_t       data_size     = bytes_to_read;

    const esp_err_t esp_err = nvs_get_blob_partial(
        p_context->handle,
        p_context->p_filename,
        p_buf,
        &data_size,
        p_context->data_offset);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "Can't read file from NVS by key '%s'", p_context->p_filename);
        return -1;
    }
    if (data_size != bytes_to_read)
    {
        LOG_ERR(
            "Read unexpected number of bytes from NVS file '%s': expected %zu, got %zu",
            p_context->p_filename,
            bytes_to_read,
            data_size);
        return -1;
    }
    p_buf[bytes_to_read] = '\0';
    LOG_DUMP_DBG(
        (const uint8_t*)arg.read.p_buf,
        data_size,
        "Data read from file '%s' from offset %zu (bytes_read=%zu)",
        p_context->p_filename,
        p_context->data_offset,
        bytes_to_read);
    p_context->data_offset += bytes_to_read;
    if (p_context->data_offset == p_context->file_size)
    {
        LOG_DBG("Finished reading file '%s' from NVS", p_context->p_filename);
    }
    return bytes_to_read;
}

static ssize_t
http_stream_reader_nvs_handle_cmd_close(http_stream_reader_nvs_ctx_t* const p_context)
{
    LOG_DBG("HTTP_STREAM_READER_CMD_CLOSE: p_context=%p, handle=%u", p_context, p_context->handle);
    nvs_close(p_context->handle);
    p_context->handle      = 0;
    p_context->data_offset = 0;
    return 0;
}

ssize_t
http_stream_reader_nvs(const http_stream_reader_cmd_e cmd, const http_stream_reader_arg_t arg, void* const p_ctx)
{
    http_stream_reader_nvs_ctx_t* const p_context = p_ctx;
    switch (cmd)
    {
        case HTTP_STREAM_READER_CMD_OPEN:
            return http_stream_reader_nvs_handle_cmd_open(p_context, arg.open.p_param);
        case HTTP_STREAM_READER_CMD_READ:
            return http_stream_reader_nvs_handle_cmd_read(p_context, arg.read.p_buf, arg.read.buf_len);
        case HTTP_STREAM_READER_CMD_CLOSE:
            return http_stream_reader_nvs_handle_cmd_close(p_context);
    }
    assert(0);
    return -1;
}
