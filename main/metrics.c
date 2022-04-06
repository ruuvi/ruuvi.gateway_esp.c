/**
 * @file metrics.c
 * @author Scrin
 * @date 2020-08-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "gw_mac.h"
#include "str_buf.h"
#include "os_malloc.h"
#include "os_mutex.h"
#include "esp_type_wrapper.h"
#include "mac_addr.h"
#include "nrf52fw.h"
#include "fw_ver.h"
#include "fw_update.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
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
    mac_address_str_t           mac_addr_str;
    ruuvi_nrf52_fw_ver_str_t    nrf_fw;
    ruuvi_esp32_fw_ver_str_t    esp_fw;
} metrics_info_t;

static const char TAG[] = "metrics";

static uint64_t          g_received_advertisements;
static os_mutex_t        g_p_metrics_mutex;
static os_mutex_static_t g_metrics_mutex_mem;

void
metrics_init(void)
{
    g_received_advertisements = 0;
    g_p_metrics_mutex         = os_mutex_create_static(&g_metrics_mutex_mem);
}

void
metrics_deinit(void)
{
    os_mutex_delete(&g_p_metrics_mutex);
}

static void
metrics_lock(void)
{
    if (NULL == g_p_metrics_mutex)
    {
        metrics_init();
    }
    os_mutex_lock(g_p_metrics_mutex);
}

static void
metrics_unlock(void)
{
    os_mutex_unlock(g_p_metrics_mutex);
}

void
metrics_received_advs_increment(void)
{
    metrics_lock();
    g_received_advertisements += 1;
    metrics_unlock();
}

static uint64_t
metrics_received_advs_get(void)
{
    metrics_lock();
    const uint64_t num_received_advertisements = g_received_advertisements;
    metrics_unlock();
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
        .received_advertisements        = metrics_received_advs_get(),
        .uptime_us                      = esp_timer_get_time(),
        .total_free_bytes.size_exec     = (ulong_t)get_total_free_bytes(MALLOC_CAP_EXEC),
        .total_free_bytes.size_32bit    = (ulong_t)get_total_free_bytes(MALLOC_CAP_32BIT),
        .total_free_bytes.size_8bit     = (ulong_t)get_total_free_bytes(MALLOC_CAP_8BIT),
        .total_free_bytes.size_dma      = (ulong_t)get_total_free_bytes(MALLOC_CAP_DMA),
        .total_free_bytes.size_pid2     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID2),
        .total_free_bytes.size_pid3     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID3),
        .total_free_bytes.size_pid4     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID4),
        .total_free_bytes.size_pid5     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID5),
        .total_free_bytes.size_pid6     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID6),
        .total_free_bytes.size_pid7     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID7),
        .total_free_bytes.size_spiram   = (ulong_t)get_total_free_bytes(MALLOC_CAP_SPIRAM),
        .total_free_bytes.size_internal = (ulong_t)get_total_free_bytes(MALLOC_CAP_INTERNAL),
        .total_free_bytes.size_default  = (ulong_t)get_total_free_bytes(MALLOC_CAP_DEFAULT),

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
        .mac_addr_str                     = g_gw_mac_sta_str,
        .esp_fw                           = *gw_cfg_get_esp32_fw_ver(),
        .nrf_fw                           = *gw_cfg_get_nrf52_fw_ver(),
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
metrics_print_gwinfo(str_buf_t *p_str_buf, const metrics_info_t *p_metrics)
{
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "info{mac=\"%s\",esp_fw=\"%s\",nrf_fw=\"%s\"} %u\n",
        p_metrics->mac_addr_str.str_buf,
        p_metrics->esp_fw.buf,
        p_metrics->nrf_fw.buf,
        1);
}

static void
metrics_print(str_buf_t *p_str_buf, const metrics_info_t *p_metrics)
{
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "received_advertisements %lld\n",
        (printf_long_long_t)p_metrics->received_advertisements);
    str_buf_printf(p_str_buf, METRICS_PREFIX "uptime_us %lld\n", (printf_long_long_t)p_metrics->uptime_us);
    metrics_print_total_free_bytes(p_str_buf, p_metrics);
    metrics_print_largest_free_blk(p_str_buf, p_metrics);
    metrics_print_gwinfo(p_str_buf, p_metrics);
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
