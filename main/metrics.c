#include "ruuvi_gateway.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#define BUF_LEN        2048
#define METRICS_PREFIX "ruuvigw_"

static const char TAG[] = "metrics";

gw_metrics_t gw_metrics = {};

size_t
get_total_free_bytes(const uint32_t caps)
{
    multi_heap_info_t x;
    heap_caps_get_info(&x, caps);
    return x.total_free_bytes;
}

// See:
// https://prometheus.io/docs/instrumenting/writing_exporters/
// https://prometheus.io/docs/instrumenting/exposition_formats/

int
gen_metrics(char *buf, size_t limit)
{
    return snprintf(
        buf,
        limit,
        METRICS_PREFIX "received_advertisements %lld\n" METRICS_PREFIX "uptime_us %lld\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_EXEC\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_32BIT\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_8BIT\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_DMA\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_PID2\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_PID3\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_PID4\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_PID5\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_PID6\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_PID7\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_SPIRAM\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_INTERNAL\"} %zu\n" METRICS_PREFIX
                       "heap_free_bytes{capability=\"MALLOC_CAP_DEFAULT\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_EXEC\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_32BIT\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_8BIT\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_DMA\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID2\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID3\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID4\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID5\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID6\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_PID7\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_SPIRAM\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_INTERNAL\"} %zu\n" METRICS_PREFIX
                       "heap_largest_free_block_bytes{capability=\"MALLOC_CAP_DEFAULT\"} %zu\n",
        gw_metrics.received_advertisements,
        esp_timer_get_time(),
        get_total_free_bytes(MALLOC_CAP_EXEC),
        get_total_free_bytes(MALLOC_CAP_32BIT),
        get_total_free_bytes(MALLOC_CAP_8BIT),
        get_total_free_bytes(MALLOC_CAP_DMA),
        get_total_free_bytes(MALLOC_CAP_PID2),
        get_total_free_bytes(MALLOC_CAP_PID3),
        get_total_free_bytes(MALLOC_CAP_PID4),
        get_total_free_bytes(MALLOC_CAP_PID5),
        get_total_free_bytes(MALLOC_CAP_PID6),
        get_total_free_bytes(MALLOC_CAP_PID7),
        get_total_free_bytes(MALLOC_CAP_SPIRAM),
        get_total_free_bytes(MALLOC_CAP_INTERNAL),
        get_total_free_bytes(MALLOC_CAP_DEFAULT),
        heap_caps_get_largest_free_block(MALLOC_CAP_EXEC),
        heap_caps_get_largest_free_block(MALLOC_CAP_32BIT),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
        heap_caps_get_largest_free_block(MALLOC_CAP_PID2),
        heap_caps_get_largest_free_block(MALLOC_CAP_PID3),
        heap_caps_get_largest_free_block(MALLOC_CAP_PID4),
        heap_caps_get_largest_free_block(MALLOC_CAP_PID5),
        heap_caps_get_largest_free_block(MALLOC_CAP_PID6),
        heap_caps_get_largest_free_block(MALLOC_CAP_PID7),
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

char *
ruuvi_get_metrics()
{
    char *buf  = malloc(BUF_LEN * sizeof(char));
    int   size = gen_metrics(buf, BUF_LEN);
    if (size >= BUF_LEN)
    {
        ESP_LOGW(
            TAG,
            "Initial buffer size of %d for metrics is insufficient, needed %d! Consider increasing BUF_LEN in "
            "metrics.c",
            BUF_LEN,
            size);
        realloc(buf, (size + 1) * sizeof(char));
        gen_metrics(buf, size + 1);
    }
    return buf;
}
