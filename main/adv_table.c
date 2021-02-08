/**
 * @file adv_table.c
 * @author TheSomeMan
 * @date 2021-01-19
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_table.h"
#include <string.h>
#include <limits.h>
#include "os_mutex.h"

#define ADV_TABLE_HASH_SIZE (101)

typedef struct adv_report_hash_list_elem_t adv_report_hash_list_elem_t;
struct adv_report_hash_list_elem_t
{
    adv_report_hash_list_elem_t *p_next;
    adv_report_t *               p_adv_report;
};

typedef struct adv_hash_table_t
{
    adv_report_hash_list_elem_t *p_list[ADV_TABLE_HASH_SIZE];
} adv_hash_table_t;

static adv_report_table_t          g_adv_reports;
static os_mutex_t                  gp_adv_reports_mutex;
static os_mutex_static_t           g_adv_reports_mutex_mem;
static adv_hash_table_t            g_adv_hash_table;
static adv_report_hash_list_elem_t g_adv_hash_arr_of_elems[MAX_ADVS_TABLE];

void
adv_table_init(void)
{
    gp_adv_reports_mutex      = os_mutex_create_static(&g_adv_reports_mutex_mem);
    g_adv_reports.num_of_advs = 0;
    memset(&g_adv_hash_table, 0, sizeof(g_adv_hash_table));
    memset(g_adv_hash_arr_of_elems, 0, sizeof(g_adv_hash_arr_of_elems));
}

void
adv_table_deinit(void)
{
    os_mutex_delete(&gp_adv_reports_mutex);
}

static bool
mac_address_is_equal(const mac_address_bin_t *const p_mac1, const mac_address_bin_t *const p_mac2)
{
    if (0 == memcmp(p_mac1->mac, p_mac2->mac, MAC_ADDRESS_NUM_BYTES))
    {
        return true;
    }
    return false;
}

ADV_TABLE_STATIC
uint32_t
adv_report_calc_hash(const mac_address_bin_t *const p_mac)
{
    uint32_t     hash_val           = 0;
    const size_t mac_addr_half_size = sizeof(p_mac->mac) / sizeof(p_mac->mac[0]) / 2;
    for (uint32_t i = 0; i < mac_addr_half_size; ++i)
    {
        hash_val |= ((uint32_t)p_mac->mac[i] ^ (uint32_t)p_mac->mac[i + mac_addr_half_size]) << (i * CHAR_BIT);
    }
    return hash_val;
}

ADV_TABLE_STATIC
adv_report_hash_list_elem_t *
adv_hash_table_search(const mac_address_bin_t *const p_mac)
{
    const uint32_t               hash_val    = adv_report_calc_hash(p_mac);
    const uint32_t               hash_idx    = hash_val % ADV_TABLE_HASH_SIZE;
    adv_report_hash_list_elem_t *p_hash_elem = g_adv_hash_table.p_list[hash_idx];
    while (NULL != p_hash_elem)
    {
        if (mac_address_is_equal(p_mac, &p_hash_elem->p_adv_report->tag_mac))
        {
            return p_hash_elem;
        }
        p_hash_elem = p_hash_elem->p_next;
    }
    return NULL;
}

ADV_TABLE_STATIC
void
adv_hash_table_add(adv_report_hash_list_elem_t *p_elem, adv_report_t *p_adv)
{
    p_elem->p_adv_report    = p_adv;
    p_elem->p_next          = NULL;
    const uint32_t hash_val = adv_report_calc_hash(&p_adv->tag_mac);
    const uint32_t hash_idx = hash_val % ADV_TABLE_HASH_SIZE;

    adv_report_hash_list_elem_t *p_hash_elem = g_adv_hash_table.p_list[hash_idx];
    if (NULL == p_hash_elem)
    {
        g_adv_hash_table.p_list[hash_idx] = p_elem;
    }
    else
    {
        adv_report_hash_list_elem_t *p_prev_hash_elem = p_hash_elem;
        while (NULL != p_hash_elem)
        {
            p_prev_hash_elem = p_hash_elem;
            p_hash_elem      = p_hash_elem->p_next;
        }
        p_prev_hash_elem->p_next = p_elem;
    }
}

ADV_TABLE_STATIC
void
adv_hash_table_clear(void)
{
    for (uint32_t i = 0; i < ADV_TABLE_HASH_SIZE; ++i)
    {
        g_adv_hash_table.p_list[i] = NULL;
    }
}

static bool
adv_table_put_unsafe(const adv_report_t *const p_adv)
{
    // Check if we already have advertisement with this MAC
    adv_report_hash_list_elem_t *p_elem = adv_hash_table_search(&p_adv->tag_mac);
    if (NULL == p_elem)
    {
        // not found from the table, insert if not full
        if (g_adv_reports.num_of_advs >= MAX_ADVS_TABLE)
        {
            return false;
        }
        adv_report_t *p_adv_in_arr = &g_adv_reports.table[g_adv_reports.num_of_advs];
        *p_adv_in_arr              = *p_adv;
        adv_hash_table_add(&g_adv_hash_arr_of_elems[g_adv_reports.num_of_advs], p_adv_in_arr);
        g_adv_reports.num_of_advs += 1;
    }
    else
    {
        *p_elem->p_adv_report = *p_adv; // Update data
    }

    return true;
}

bool
adv_table_put(const adv_report_t *const p_adv)
{
    os_mutex_lock(gp_adv_reports_mutex);
    const bool res = adv_table_put_unsafe(p_adv);
    os_mutex_unlock(gp_adv_reports_mutex);
    return res;
}

static void
adv_table_read_and_clear_unsafe(adv_report_table_t *const p_reports)
{
    *p_reports = g_adv_reports;

    adv_hash_table_clear();
    g_adv_reports.num_of_advs = 0; // clear the table
}

void
adv_table_read_and_clear(adv_report_table_t *const p_reports)
{
    os_mutex_lock(gp_adv_reports_mutex);
    adv_table_read_and_clear_unsafe(p_reports);
    os_mutex_unlock(gp_adv_reports_mutex);
}
