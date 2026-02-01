/**
 * @author TheSomeMan
 * @date 2026-02-01
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "mem_fragmentation_test.h"
#include "os_malloc.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if RUUVI_GATEWAY_ENABLE_MEM_FRAGMENTATION_TEST
static const char TAG[] = "ruuvi_gateway";

void* volatile g_p_fragmented_mem;

void
mem_fragmentation_test(void)
{
    static uint32_t block_size = 5 * 1024;
    void*           p_mem      = os_malloc(block_size);
    if (NULL == p_mem)
    {
        LOG_ERR("Failed to allocate memory for fragmentation test %u bytes", block_size);
        block_size /= 2;
        if (0 == block_size)
        {
            block_size = 1;
        }
    }

    g_p_fragmented_mem = os_malloc(8);
    if (NULL == g_p_fragmented_mem)
    {
        LOG_ERR("Failed to allocate memory for fragmentation test");
    }
    if (NULL != p_mem)
    {
        os_free(p_mem);
    }
    if (block_size < (20 * 1024))
    {
        block_size += 32;
    }
}
#endif // RUUVI_GATEWAY_ENABLE_MEM_FRAGMENTATION_TEST
