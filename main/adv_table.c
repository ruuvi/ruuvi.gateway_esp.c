/**
 * @file adv_table.c
 * @author TheSomeMan
 * @date 2021-01-19
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_table.h"
#include <string.h>

static adv_report_table_t g_adv_reports;

void
adv_table_init(void)
{
    g_adv_reports.num_of_advs = 0;
}

bool
adv_table_put(const adv_report_t *const p_adv)
{
    // Check if we already have advertisement with this MAC
    for (num_of_advs_t i = 0; i < g_adv_reports.num_of_advs; ++i)
    {
        const mac_address_bin_t *p_mac = &g_adv_reports.table[i].tag_mac;

        if (0 == memcmp(&p_adv->tag_mac, p_mac, sizeof(*p_mac)))
        {
            // Yes, update data.
            g_adv_reports.table[i] = *p_adv;
            return true;
        }
    }

    // not found from the table, insert if not full
    if (g_adv_reports.num_of_advs >= MAX_ADVS_TABLE)
    {
        return false;
    }
    g_adv_reports.table[g_adv_reports.num_of_advs] = *p_adv;
    g_adv_reports.num_of_advs += 1;
    return true;
}

void
adv_table_read_and_clear(adv_report_table_t *const p_reports)
{
    *p_reports = g_adv_reports;

    g_adv_reports.num_of_advs = 0; // clear the table
}
