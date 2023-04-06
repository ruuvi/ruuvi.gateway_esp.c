/**
 * @file runtime_stat.h
 * @author TheSomeMan
 * @date 2023-03-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_RUNTIME_STAT_H
#define RUUVI_RUNTIME_STAT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
log_runtime_statistics(void);

bool
runtime_stat_for_each_accumulated_info(
    bool (*p_cb)(const char* const p_task_name, const uint32_t min_free_stack_size, void* p_userdata),
    void* p_userdata);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_RUNTIME_STAT_H
