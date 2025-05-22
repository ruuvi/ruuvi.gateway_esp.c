/**
 * @file metrics.c
 * @author Scrin
 * @date 2020-08-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "metrics.h"
#include <inttypes.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp32/rom/crc.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include "str_buf.h"
#include "os_malloc.h"
#include "os_mutex.h"
#include "esp_type_wrapper.h"
#include "mac_addr.h"
#include "fw_ver.h"
#include "cjson_wrap.h"
#include "gw_cfg_ruuvi_json.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO

#include "log.h"

#define METRICS_PREFIX "ruuvigw_"

#define METRICS_SHA256_SIZE (32)

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

typedef struct metrics_crc32_t
{
    uint32_t val;
} metrics_crc32_t;

typedef struct metrics_crc32_str_t
{
    char buf[2 + (2 * sizeof(metrics_crc32_t)) + 1];
} metrics_crc32_str_t;

typedef struct metrics_sha256_t
{
    uint8_t buf[METRICS_SHA256_SIZE];
} metrics_sha256_t;

typedef struct metrics_sha256_str_t
{
    char buf[(METRICS_SHA256_SIZE * 2) + 1];
} metrics_sha256_str_t;

typedef struct metrics_info_t
{
    uint64_t                    received_advertisements;
    uint64_t                    received_ext_advertisements;
    uint64_t                    received_coded_advertisements;
    int64_t                     uptime_us;
    uint32_t                    nrf_self_reboot_cnt;
    uint32_t                    nrf_ext_hw_reset_cnt;
    uint64_t                    nrf_lost_ack_cnt;
    metrics_total_free_info_t   total_free_bytes;
    metrics_largest_free_info_t largest_free_block;
    mac_address_str_t           mac_addr_str;
    ruuvi_nrf52_fw_ver_str_t    nrf_fw;
    ruuvi_esp32_fw_ver_str_t    esp_fw;
    metrics_crc32_str_t         gw_cfg_crc32;
    metrics_sha256_str_t        gw_cfg_sha256;
    metrics_crc32_str_t         ruuvi_json_crc32;
    metrics_sha256_str_t        ruuvi_json_sha256;
} metrics_info_t;

typedef struct metrics_tmp_buf_t
{
    metrics_crc32_str_t    gw_cfg_crc32_str;
    metrics_sha256_str_t   gw_cfg_sha256_str;
    metrics_crc32_str_t    ruuvi_json_crc32_str;
    metrics_sha256_str_t   ruuvi_json_sha256_str;
    metrics_sha256_t       tmp_sha256;
    mbedtls_sha256_context tmp_sha256_ctx;
} metrics_tmp_buf_t;

static const char TAG[] = "metrics";

static uint64_t          g_received_advertisements;
static uint64_t          g_received_advertisements_ext;
static uint64_t          g_received_advertisements_coded;
static uint64_t          g_nrf_lost_ack_cnt;
static uint32_t          g_nrf_self_reboot_cnt;
static uint32_t          g_nrf_ext_hw_reset_cnt;
static os_mutex_t        g_p_metrics_mutex;
static os_mutex_static_t g_metrics_mutex_mem;

void
metrics_init(void)
{
    g_received_advertisements       = 0;
    g_received_advertisements_ext   = 0;
    g_received_advertisements_coded = 0;
    g_nrf_lost_ack_cnt              = 0;
    g_nrf_self_reboot_cnt           = 0;
    g_nrf_ext_hw_reset_cnt          = 0;
    g_p_metrics_mutex               = os_mutex_create_static(&g_metrics_mutex_mem);
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
metrics_received_advs_increment(const re_ca_uart_ble_phy_e secondary_phy)
{
    metrics_lock();
    switch (secondary_phy)
    {
        case RE_CA_UART_BLE_PHY_NOT_SET:
            g_received_advertisements += 1;
            break;
        case RE_CA_UART_BLE_PHY_2MBPS:
            g_received_advertisements_ext += 1;
            break;
        case RE_CA_UART_BLE_PHY_CODED:
            g_received_advertisements_coded += 1;
            break;
        default:
            break;
    }
    metrics_unlock();
}

uint64_t
metrics_received_advs_get(void)
{
    metrics_lock();
    const uint64_t num_received_advertisements = g_received_advertisements;
    metrics_unlock();
    return num_received_advertisements;
}

uint64_t
metrics_received_ext_advs_get(void)
{
    metrics_lock();
    const uint64_t num_received_ext_advertisements = g_received_advertisements_ext;
    metrics_unlock();
    return num_received_ext_advertisements;
}

uint64_t
metrics_received_coded_advs_get(void)
{
    metrics_lock();
    const uint64_t num_received_coded_advertisements = g_received_advertisements_coded;
    metrics_unlock();
    return num_received_coded_advertisements;
}

uint32_t
metrics_nrf_self_reboot_cnt_get(void)
{
    metrics_lock();
    const uint32_t nrf_self_reboot_cnt = g_nrf_self_reboot_cnt;
    metrics_unlock();
    return nrf_self_reboot_cnt;
}

void
metrics_nrf_self_reboot_cnt_inc(void)
{
    metrics_lock();
    g_nrf_self_reboot_cnt += 1;
    metrics_unlock();
}

uint32_t
metrics_nrf_ext_hw_reset_cnt_get(void)
{
    metrics_lock();
    const uint32_t nrf_ext_hw_reset_cnt = g_nrf_ext_hw_reset_cnt;
    metrics_unlock();
    return nrf_ext_hw_reset_cnt;
}

void
metrics_nrf_ext_hw_reset_cnt_inc(void)
{
    metrics_lock();
    g_nrf_ext_hw_reset_cnt += 1;
    metrics_unlock();
}

uint64_t
metrics_nrf_lost_ack_cnt_get(void)
{
    metrics_lock();
    const uint64_t lost_ack_cnt = g_nrf_lost_ack_cnt;
    metrics_unlock();
    return lost_ack_cnt;
}

void
metrics_nrf_lost_ack_cnt_inc(void)
{
    metrics_lock();
    g_nrf_lost_ack_cnt += 1;
    metrics_unlock();
}

static size_t
get_total_free_bytes(const uint32_t caps)
{
    multi_heap_info_t x = { 0 };
    heap_caps_get_info(&x, caps);
    return x.total_free_bytes;
}

static void
metrics_calc_gw_cfg_hash(
    metrics_crc32_str_t* const    p_crc32,
    metrics_sha256_str_t* const   p_sha256,
    metrics_sha256_t* const       p_tmp_sha256,
    mbedtls_sha256_context* const p_tmp_sha256_ctx)
{
    const gw_cfg_t* p_gw_cfg = gw_cfg_lock_ro();

    const metrics_crc32_t crc32 = {
        .val = crc32_le(0, (const void*)&p_gw_cfg->ruuvi_cfg, sizeof(*p_gw_cfg) - offsetof(gw_cfg_t, ruuvi_cfg)),
    };

    mbedtls_sha256_init(p_tmp_sha256_ctx);
    mbedtls_sha256_starts_ret(p_tmp_sha256_ctx, false);
    mbedtls_sha256_update_ret(
        p_tmp_sha256_ctx,
        (const void*)&p_gw_cfg->ruuvi_cfg,
        sizeof(*p_gw_cfg) - offsetof(gw_cfg_t, ruuvi_cfg));
    mbedtls_sha256_finish_ret(p_tmp_sha256_ctx, p_tmp_sha256->buf);
    mbedtls_sha256_free(p_tmp_sha256_ctx);

    gw_cfg_unlock_ro(&p_gw_cfg);

    (void)snprintf(p_crc32->buf, sizeof(p_crc32->buf), "%lu", (printf_ulong_t)crc32.val);
    str_buf_t str_buf = STR_BUF_INIT(p_sha256->buf, sizeof(p_sha256->buf));
    for (uint32_t i = 0; i < sizeof(p_tmp_sha256->buf); ++i)
    {
        str_buf_printf(&str_buf, "%02x", p_tmp_sha256->buf[i]);
    }
}

static void
metrics_calc_ruuvi_json_hash(
    metrics_crc32_str_t* const    p_crc32,
    metrics_sha256_str_t* const   p_sha256,
    metrics_sha256_t* const       p_tmp_sha256,
    mbedtls_sha256_context* const p_tmp_sha256_ctx)
{
    memset(p_crc32, 0, sizeof(*p_crc32));
    memset(p_sha256, 0, sizeof(*p_sha256));

    const gw_cfg_t*  p_gw_cfg = gw_cfg_lock_ro();
    cjson_wrap_str_t json_str = cjson_wrap_str_null();
    if (!gw_cfg_ruuvi_json_generate(p_gw_cfg, &json_str))
    {
        gw_cfg_unlock_ro(&p_gw_cfg);
        return;
    }
    gw_cfg_unlock_ro(&p_gw_cfg);

    const metrics_crc32_t crc32 = {
        .val = crc32_le(0, (const void*)json_str.p_str, strlen(json_str.p_str)),
    };

    mbedtls_sha256_init(p_tmp_sha256_ctx);
    mbedtls_sha256_starts_ret(p_tmp_sha256_ctx, false);
    mbedtls_sha256_update_ret(p_tmp_sha256_ctx, (const void*)json_str.p_str, strlen(json_str.p_str));
    mbedtls_sha256_finish_ret(p_tmp_sha256_ctx, p_tmp_sha256->buf);
    mbedtls_sha256_free(p_tmp_sha256_ctx);

    os_free(json_str.p_str);

    (void)snprintf(p_crc32->buf, sizeof(p_crc32->buf), "%lu", (printf_ulong_t)crc32.val);
    str_buf_t str_buf = STR_BUF_INIT(p_sha256->buf, sizeof(p_sha256->buf));
    for (uint32_t i = 0; i < sizeof(p_tmp_sha256->buf); ++i)
    {
        str_buf_printf(&str_buf, "%02x", p_tmp_sha256->buf[i]);
    }
}

static metrics_info_t*
gen_metrics(void)
{
    metrics_tmp_buf_t* p_tmp_buf = os_calloc(1, sizeof(*p_tmp_buf));
    if (NULL == p_tmp_buf)
    {
        return NULL;
    }
    metrics_info_t* p_metrics = os_calloc(1, sizeof(*p_metrics));
    if (NULL == p_metrics)
    {
        os_free(p_tmp_buf);
        return NULL;
    }

    metrics_calc_gw_cfg_hash(
        &p_tmp_buf->gw_cfg_crc32_str,
        &p_tmp_buf->gw_cfg_sha256_str,
        &p_tmp_buf->tmp_sha256,
        &p_tmp_buf->tmp_sha256_ctx);
    metrics_calc_ruuvi_json_hash(
        &p_tmp_buf->ruuvi_json_crc32_str,
        &p_tmp_buf->ruuvi_json_sha256_str,
        &p_tmp_buf->tmp_sha256,
        &p_tmp_buf->tmp_sha256_ctx);

    p_metrics->received_advertisements        = metrics_received_advs_get();
    p_metrics->received_ext_advertisements    = metrics_received_ext_advs_get();
    p_metrics->received_coded_advertisements  = metrics_received_coded_advs_get();
    p_metrics->nrf_self_reboot_cnt            = metrics_nrf_self_reboot_cnt_get();
    p_metrics->nrf_ext_hw_reset_cnt           = metrics_nrf_ext_hw_reset_cnt_get();
    p_metrics->nrf_lost_ack_cnt               = metrics_nrf_lost_ack_cnt_get();
    p_metrics->uptime_us                      = esp_timer_get_time();
    p_metrics->total_free_bytes.size_exec     = (ulong_t)get_total_free_bytes(MALLOC_CAP_EXEC);
    p_metrics->total_free_bytes.size_32bit    = (ulong_t)get_total_free_bytes(MALLOC_CAP_32BIT);
    p_metrics->total_free_bytes.size_8bit     = (ulong_t)get_total_free_bytes(MALLOC_CAP_8BIT);
    p_metrics->total_free_bytes.size_dma      = (ulong_t)get_total_free_bytes(MALLOC_CAP_DMA);
    p_metrics->total_free_bytes.size_pid2     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID2);
    p_metrics->total_free_bytes.size_pid3     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID3);
    p_metrics->total_free_bytes.size_pid4     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID4);
    p_metrics->total_free_bytes.size_pid5     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID5);
    p_metrics->total_free_bytes.size_pid6     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID6);
    p_metrics->total_free_bytes.size_pid7     = (ulong_t)get_total_free_bytes(MALLOC_CAP_PID7);
    p_metrics->total_free_bytes.size_spiram   = (ulong_t)get_total_free_bytes(MALLOC_CAP_SPIRAM);
    p_metrics->total_free_bytes.size_internal = (ulong_t)get_total_free_bytes(MALLOC_CAP_INTERNAL);
    p_metrics->total_free_bytes.size_default  = (ulong_t)get_total_free_bytes(MALLOC_CAP_DEFAULT);

    p_metrics->largest_free_block.size_exec     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_EXEC);
    p_metrics->largest_free_block.size_32bit    = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_32BIT);
    p_metrics->largest_free_block.size_8bit     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    p_metrics->largest_free_block.size_dma      = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    p_metrics->largest_free_block.size_pid2     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID2);
    p_metrics->largest_free_block.size_pid3     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID3);
    p_metrics->largest_free_block.size_pid4     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID4);
    p_metrics->largest_free_block.size_pid5     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID5);
    p_metrics->largest_free_block.size_pid6     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID6);
    p_metrics->largest_free_block.size_pid7     = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_PID7);
    p_metrics->largest_free_block.size_spiram   = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    p_metrics->largest_free_block.size_internal = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    p_metrics->largest_free_block.size_default  = (ulong_t)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    p_metrics->mac_addr_str                     = *gw_cfg_get_nrf52_mac_addr();
    p_metrics->esp_fw                           = *gw_cfg_get_esp32_fw_ver();
    p_metrics->nrf_fw                           = *gw_cfg_get_nrf52_fw_ver();
    p_metrics->gw_cfg_crc32                     = p_tmp_buf->gw_cfg_crc32_str;
    p_metrics->gw_cfg_sha256                    = p_tmp_buf->gw_cfg_sha256_str;
    p_metrics->ruuvi_json_crc32                 = p_tmp_buf->ruuvi_json_crc32_str;
    p_metrics->ruuvi_json_sha256                = p_tmp_buf->ruuvi_json_sha256_str;

    os_free(p_tmp_buf);

    return p_metrics;
}

static void
metrics_print_total_free_bytes(str_buf_t* p_str_buf, const metrics_info_t* p_metrics)
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
metrics_print_largest_free_blk(str_buf_t* p_str_buf, const metrics_info_t* p_metrics)
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
metrics_print_gwinfo(str_buf_t* p_str_buf, const metrics_info_t* p_metrics)
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
metrics_print_gw_cfg_info(str_buf_t* const p_str_buf, const metrics_info_t* const p_metrics)
{
    str_buf_printf(p_str_buf, METRICS_PREFIX "gw_cfg_crc32 %s\n", p_metrics->gw_cfg_crc32.buf);
    str_buf_printf(p_str_buf, METRICS_PREFIX "ruuvi_json_crc32 %s\n", p_metrics->ruuvi_json_crc32.buf);
#if 0
    str_buf_printf(p_str_buf, METRICS_PREFIX "tmp_sha256 %s\n", p_metrics->tmp_sha256.buf);
    str_buf_printf(p_str_buf, METRICS_PREFIX "tmp_sha256 %s\n", p_metrics->tmp_sha256.buf);
#endif
}

static void
metrics_print(str_buf_t* p_str_buf, const metrics_info_t* p_metrics)
{
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "received_advertisements %lld\n",
        (printf_long_long_t)p_metrics->received_advertisements);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "received_ext_advertisements %lld\n",
        (printf_long_long_t)p_metrics->received_ext_advertisements);
    str_buf_printf(
        p_str_buf,
        METRICS_PREFIX "received_coded_advertisements %lld\n",
        (printf_long_long_t)p_metrics->received_coded_advertisements);
    str_buf_printf(p_str_buf, METRICS_PREFIX "uptime_us %lld\n", (printf_long_long_t)p_metrics->uptime_us);
    str_buf_printf(p_str_buf, METRICS_PREFIX "nrf_self_reboot_cnt %" PRIu32 "\n", p_metrics->nrf_self_reboot_cnt);
    str_buf_printf(p_str_buf, METRICS_PREFIX "nrf_ext_hw_reset_cnt %" PRIu32 "\n", p_metrics->nrf_ext_hw_reset_cnt);
    str_buf_printf(p_str_buf, METRICS_PREFIX "nrf_lost_ack_cnt %" PRIu64 "\n", p_metrics->nrf_lost_ack_cnt);
    metrics_print_total_free_bytes(p_str_buf, p_metrics);
    metrics_print_largest_free_blk(p_str_buf, p_metrics);
    metrics_print_gwinfo(p_str_buf, p_metrics);
    metrics_print_gw_cfg_info(p_str_buf, p_metrics);
}

char*
metrics_generate(void)
{
    const metrics_info_t* p_metrics_info = gen_metrics();

    str_buf_t str_buf = str_buf_init_null();
    metrics_print(&str_buf, p_metrics_info);

    if (!str_buf_init_with_alloc(&str_buf))
    {
        LOG_ERR("Can't allocate memory");
        return NULL;
    }
    metrics_print(&str_buf, p_metrics_info);
    os_free(p_metrics_info);
    return str_buf.buf;
}
