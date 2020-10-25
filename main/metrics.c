/**
 * @file metrics.c
 * @author Scrin
 * @date 2020-08-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "esp_heap_caps.h"
#include "ruuvi_gateway.h"
#include "str_buf.h"
#include "log.h"

#define METRICS_PREFIX "ruuvigw_"

static const char TAG[] = "metrics";

gw_metrics_t gw_metrics = {};

size_t
get_total_free_bytes(const uint32_t caps)
{
    multi_heap_info_t x = { 0 };
    heap_caps_get_info(&x, caps);
    return x.total_free_bytes;
}

// See:
// https://prometheus.io/docs/instrumenting/writing_exporters/
// https://prometheus.io/docs/instrumenting/exposition_formats/

typedef unsigned long ulong_t;

typedef struct metrics_info_t
{
    uint64_t received_advertisements;
    int64_t  uptime_us;
    ulong_t  total_free_bytes_exec;
    ulong_t  total_free_bytes_32bit;
    ulong_t  total_free_bytes_8bit;
    ulong_t  total_free_bytes_dma;
    ulong_t  total_free_bytes_pid2;
    ulong_t  total_free_bytes_pid3;
    ulong_t  total_free_bytes_pid4;
    ulong_t  total_free_bytes_pid5;
    ulong_t  total_free_bytes_pid6;
    ulong_t  total_free_bytes_pid7;
    ulong_t  total_free_bytes_spiram;
    ulong_t  total_free_bytes_internal;
    ulong_t  total_free_bytes_default;

    ulong_t largest_free_block_exec;
    ulong_t largest_free_block_32bit;
    ulong_t largest_free_block_8bit;
    ulong_t largest_free_block_dma;
    ulong_t largest_free_block_pid2;
    ulong_t largest_free_block_pid3;
    ulong_t largest_free_block_pid4;
    ulong_t largest_free_block_pid5;
    ulong_t largest_free_block_pid6;
    ulong_t largest_free_block_pid7;
    ulong_t largest_free_block_spiram;
    ulong_t largest_free_block_internal;
    ulong_t largest_free_block_default;
} metrics_info_t;

static metrics_info_t
gen_metrics(void)
{
    const metrics_info_t metrics = {
        .received_advertisements     = gw_metrics.received_advertisements,
        .uptime_us                   = esp_timer_get_time(),
        .total_free_bytes_exec       = (ulong_t)get_total_free_bytes(MALLOC_CAP_EXEC),
        .total_free_bytes_32bit      = (ulong_t)get_total_free_bytes(MALLOC_CAP_32BIT),
        .total_free_bytes_8bit       = (ulong_t)get_total_free_bytes(MALLOC_CAP_8BIT),
        .total_free_bytes_dma        = (ulong_t)get_total_free_bytes(MALLOC_CAP_DMA),
        .total_free_bytes_pid2       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID2),
        .total_free_bytes_pid3       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID3),
        .total_free_bytes_pid4       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID4),
        .total_free_bytes_pid5       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID5),
        .total_free_bytes_pid6       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID6),
        .total_free_bytes_pid7       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID7),
        .total_free_bytes_spiram     = (ulong_t)get_total_free_bytes(MALLOC_CAP_SPIRAM),
        .total_free_bytes_internal   = (ulong_t)get_total_free_bytes(MALLOC_CAP_INTERNAL),
        .total_free_bytes_default    = (ulong_t)get_total_free_bytes(MALLOC_CAP_DEFAULT),
        .largest_free_block_exec     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_EXEC),
        .largest_free_block_32bit    = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_32BIT),
        .largest_free_block_8bit     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        .largest_free_block_dma      = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
        .largest_free_block_pid2     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID2),
        .largest_free_block_pid3     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID3),
        .largest_free_block_pid4     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID4),
        .largest_free_block_pid5     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID5),
        .largest_free_block_pid6     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID6),
        .largest_free_block_pid7     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID7),
        .largest_free_block_spiram   = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        .largest_free_block_internal = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        .largest_free_block_default  = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
    };
    return metrics;
}

static void
metrics_print_total_free_bytes(str_buf_t *p_str_buf, const metrics_info_t *p_metrics)
{
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_EXEC\"} %lu\n",
        p_metrics->total_free_bytes_exec);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_32BIT\"} %lu\n",
        p_metrics->total_free_bytes_32bit);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_8BIT\"} %lu\n",
        p_metrics->total_free_bytes_8bit);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_DMA\"} %lu\n",
        p_metrics->total_free_bytes_dma);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID2\"} %lu\n",
        p_metrics->total_free_bytes_pid2);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID3\"} %lu\n",
        p_metrics->total_free_bytes_pid3);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID4\"} %lu\n",
        p_metrics->total_free_bytes_pid4);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID5\"} %lu\n",
        p_metrics->total_free_bytes_pid5);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID6\"} %lu\n",
        p_metrics->total_free_bytes_pid6);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID7\"} %lu\n",
        p_metrics->total_free_bytes_pid7);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_SPIRAM\"} %lu\n",
        p_metrics->total_free_bytes_spiram);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_INTERNAL\"} %lu\n",
        p_metrics->total_free_bytes_internal);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_DEFAULT\"} %lu\n",
        p_metrics->total_free_bytes_default);
}

static void
metrics_print_largest_free_blk(str_buf_t *p_str_buf, const metrics_info_t *p_metrics)
{
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_EXEC\"} %lu\n",
        p_metrics->largest_free_block_exec);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_32BIT\"} %lu\n",
        p_metrics->largest_free_block_32bit);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_8BIT\"} %lu\n",
        p_metrics->largest_free_block_8bit);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_DMA\"} %lu\n",
        p_metrics->largest_free_block_dma);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID2\"} %lu\n",
        p_metrics->largest_free_block_pid2);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID3\"} %lu\n",
        p_metrics->largest_free_block_pid3);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID4\"} %lu\n",
        p_metrics->largest_free_block_pid4);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID5\"} %lu\n",
        p_metrics->largest_free_block_pid5);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID6\"} %lu\n",
        p_metrics->largest_free_block_pid6);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID7\"} %lu\n",
        p_metrics->largest_free_block_pid7);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_SPIRAM\"} %lu\n",
        p_metrics->largest_free_block_spiram);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_INTERNAL\"} %lu\n",
        p_metrics->largest_free_block_internal);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_DEFAULT\"} %lu\n",
        p_metrics->largest_free_block_default);
}

static void
metrics_print(str_buf_t *p_str_buf, const metrics_info_t *p_metrics)
{
    str_buf_printf(p_str_buf, METRICS_PREFIX "received_advertisements %lld\n", p_metrics->received_advertisements);
    str_buf_printf(p_str_buf, METRICS_PREFIX "uptime_us %lld\n", p_metrics->uptime_us);
    metrics_print_total_free_bytes(p_str_buf, p_metrics);
    metrics_print_largest_free_blk(p_str_buf, p_metrics);
}

char *
ruuvi_get_metrics(void)
{
    const metrics_info_t metrics_info = gen_metrics();

    str_buf_t str_buf = str_buf_init(NULL, 0);
    metrics_print(&str_buf, &metrics_info);
    const str_buf_size_t buf_size = str_buf_get_len(&str_buf) + 1;

    char *p_buf = malloc(buf_size * sizeof(char));
    if (NULL == p_buf)
    {
        LOG_ERR("Can't allocate memory");
        return NULL;
    }
    str_buf = str_buf_init(p_buf, buf_size);
    metrics_print(&str_buf, &metrics_info);
    return p_buf;
}
