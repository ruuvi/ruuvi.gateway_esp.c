/**
 * @file metrics.c
 * @author Scrin
 * @date 2020-08-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "esp_heap_caps.h"
#include "ruuvi_gateway.h"
#include "str_buf.h"
#include "os_malloc.h"
#include "log.h"

#define METRICS_PREFIX "ruuvigw_"

// See:
// https://prometheus.io/docs/instrumenting/writing_exporters/
// https://prometheus.io/docs/instrumenting/exposition_formats/

typedef unsigned long ulong_t;

typedef struct metrics_total_free_info_t
{
    ulong_t size_exec;
    ulong_t size_32bit;
    ulong_t size_8bit;
    ulong_t size_dma;
    ulong_t size_pid2;
    ulong_t size_pid3;
    ulong_t size_pid4;
    ulong_t size_pid5;
    ulong_t size_pid6;
    ulong_t size_pid7;
    ulong_t size_spiram;
    ulong_t size_internal;
    ulong_t size_default;
} metrics_total_free_info_t;

typedef struct metrics_largest_free_info_t
{
    ulong_t size_exec;
    ulong_t size_32bit;
    ulong_t size_8bit;
    ulong_t size_dma;
    ulong_t size_pid2;
    ulong_t size_pid3;
    ulong_t size_pid4;
    ulong_t size_pid5;
    ulong_t size_pid6;
    ulong_t size_pid7;
    ulong_t size_spiram;
    ulong_t size_internal;
    ulong_t size_default;
} metrics_largest_free_info_t;

typedef struct metrics_info_t
{
    uint64_t                    received_advertisements;
    int64_t                     uptime_us;
    metrics_total_free_info_t   total_free_bytes;
    metrics_largest_free_info_t largest_free_block;
} metrics_info_t;

static const char TAG[] = "metrics";

static uint64_t     g_received_advertisements;
static portMUX_TYPE g_received_advertisements_mux = portMUX_INITIALIZER_UNLOCKED;

void
metrics_received_advs_increment(void)
{
    portENTER_CRITICAL(&g_received_advertisements_mux);
    g_received_advertisements += 1;
    portEXIT_CRITICAL(&g_received_advertisements_mux);
}

static uint64_t
metrics_received_advs_get(void)
{
    portENTER_CRITICAL(&g_received_advertisements_mux);
    const uint64_t num_received_advertisements = g_received_advertisements;
    portEXIT_CRITICAL(&g_received_advertisements_mux);
    return num_received_advertisements;
}

static size_t
get_total_free_bytes(const uint32_t caps)
{
    multi_heap_info_t x = { 0 };
    heap_caps_get_info(&x, caps);
    return x.total_free_bytes;
}

static metrics_info_t
gen_metrics(void)
{
    const metrics_info_t metrics = {
        .received_advertisements          = metrics_received_advs_get(),
        .uptime_us                        = esp_timer_get_time(),
        .total_free_bytes.size_exec       = (ulong_t)get_total_free_bytes(MALLOC_CAP_EXEC),
        .total_free_bytes.size_32bit      = (ulong_t)get_total_free_bytes(MALLOC_CAP_32BIT),
        .total_free_bytes.size_8bit       = (ulong_t)get_total_free_bytes(MALLOC_CAP_8BIT),
        .total_free_bytes.size_dma        = (ulong_t)get_total_free_bytes(MALLOC_CAP_DMA),
        .total_free_bytes.size_pid2       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID2),
        .total_free_bytes.size_pid3       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID3),
        .total_free_bytes.size_pid4       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID4),
        .total_free_bytes.size_pid5       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID5),
        .total_free_bytes.size_pid6       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID6),
        .total_free_bytes.size_pid7       = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID7),
        .total_free_bytes.size_spiram     = (ulong_t)get_total_free_bytes(MALLOC_CAP_SPIRAM),
        .total_free_bytes.size_internal   = (ulong_t)get_total_free_bytes(MALLOC_CAP_INTERNAL),
        .total_free_bytes.size_default    = (ulong_t)get_total_free_bytes(MALLOC_CAP_DEFAULT),
        .largest_free_block.size_exec     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_EXEC),
        .largest_free_block.size_32bit    = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_32BIT),
        .largest_free_block.size_8bit     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        .largest_free_block.size_dma      = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
        .largest_free_block.size_pid2     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID2),
        .largest_free_block.size_pid3     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID3),
        .largest_free_block.size_pid4     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID4),
        .largest_free_block.size_pid5     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID5),
        .largest_free_block.size_pid6     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID6),
        .largest_free_block.size_pid7     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID7),
        .largest_free_block.size_spiram   = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        .largest_free_block.size_internal = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        .largest_free_block.size_default  = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
    };
    return metrics;
}

static void
metrics_print_total_free_bytes(str_buf_t *p_str_buf, const metrics_info_t *p_metrics)
{
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_EXEC\"} %lu\n",
        p_metrics->total_free_bytes.size_exec);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_32BIT\"} %lu\n",
        p_metrics->total_free_bytes.size_32bit);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_8BIT\"} %lu\n",
        p_metrics->total_free_bytes.size_8bit);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_DMA\"} %lu\n",
        p_metrics->total_free_bytes.size_dma);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID2\"} %lu\n",
        p_metrics->total_free_bytes.size_pid2);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID3\"} %lu\n",
        p_metrics->total_free_bytes.size_pid3);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID4\"} %lu\n",
        p_metrics->total_free_bytes.size_pid4);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID5\"} %lu\n",
        p_metrics->total_free_bytes.size_pid5);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID6\"} %lu\n",
        p_metrics->total_free_bytes.size_pid6);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_PID7\"} %lu\n",
        p_metrics->total_free_bytes.size_pid7);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_SPIRAM\"} %lu\n",
        p_metrics->total_free_bytes.size_spiram);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_INTERNAL\"} %lu\n",
        p_metrics->total_free_bytes.size_internal);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_free_bytes{capability=\"MALLOC_CAP_DEFAULT\"} %lu\n",
        p_metrics->total_free_bytes.size_default);
}

static void
metrics_print_largest_free_blk(str_buf_t *p_str_buf, const metrics_info_t *p_metrics)
{
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_EXEC\"} %lu\n",
        p_metrics->largest_free_block.size_exec);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_32BIT\"} %lu\n",
        p_metrics->largest_free_block.size_32bit);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_8BIT\"} %lu\n",
        p_metrics->largest_free_block.size_8bit);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_DMA\"} %lu\n",
        p_metrics->largest_free_block.size_dma);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID2\"} %lu\n",
        p_metrics->largest_free_block.size_pid2);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID3\"} %lu\n",
        p_metrics->largest_free_block.size_pid3);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID4\"} %lu\n",
        p_metrics->largest_free_block.size_pid4);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID5\"} %lu\n",
        p_metrics->largest_free_block.size_pid5);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID6\"} %lu\n",
        p_metrics->largest_free_block.size_pid6);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID7\"} %lu\n",
        p_metrics->largest_free_block.size_pid7);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_SPIRAM\"} %lu\n",
        p_metrics->largest_free_block.size_spiram);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_INTERNAL\"} %lu\n",
        p_metrics->largest_free_block.size_internal);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_DEFAULT\"} %lu\n",
        p_metrics->largest_free_block.size_default);
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
metrics_generate(void)
{
    const metrics_info_t metrics_info = gen_metrics();

    str_buf_t str_buf = str_buf_init_null();
    metrics_print(&str_buf, &metrics_info);

    if (!str_buf_init_with_alloc(&str_buf))
    {
        LOG_ERR("Can't allocate memory");
        return NULL;
    }
    metrics_print(&str_buf, &metrics_info);
    return str_buf.buf;
}
