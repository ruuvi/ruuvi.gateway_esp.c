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
#include "sys/queue.h"

#if defined(__XTENSA__)
#define ADV_REPORT_EXPECTED_SIZE (16U + 32U)
#elif defined(__linux__) && defined(__x86_64__)
#define ADV_REPORT_EXPECTED_SIZE (24U + 32U)
#endif

_Static_assert(sizeof(adv_report_t) == ADV_REPORT_EXPECTED_SIZE, "sizeof(adv_report_t)");

#define ADV_TABLE_HASH_SIZE (101)

typedef struct adv_reports_list_elem_t adv_reports_list_elem_t;

typedef STAILQ_HEAD(adv_report_list_t, adv_reports_list_elem_t) adv_report_list_t;
typedef TAILQ_HEAD(adv_report_hist_list_t, adv_reports_list_elem_t) adv_report_hist_list_t;

struct adv_reports_list_elem_t
{
    STAILQ_ENTRY(adv_reports_list_elem_t) hash_table_list;
    STAILQ_ENTRY(adv_reports_list_elem_t) retransmission_list1;
    STAILQ_ENTRY(adv_reports_list_elem_t) retransmission_list2;
    TAILQ_ENTRY(adv_reports_list_elem_t) hist_list;
    bool         is_in_hash_table;
    bool         is_in_retransmission_list1;
    bool         is_in_retransmission_list2;
    adv_report_t adv_report;
};

static os_mutex_t              gp_adv_reports_mutex;
static os_mutex_static_t       g_adv_reports_mutex_mem;
static adv_reports_list_elem_t g_arr_of_adv_reports[MAX_ADVS_TABLE];
static adv_report_list_t       g_adv_hash_table[ADV_TABLE_HASH_SIZE];
static adv_report_list_t       g_adv_reports_retransmission_list1;
static adv_report_list_t       g_adv_reports_retransmission_list2;
static adv_report_hist_list_t  g_adv_reports_hist_list;

void
adv_table_init(void)
{
    gp_adv_reports_mutex = os_mutex_create_static(&g_adv_reports_mutex_mem);

    for (uint32_t i = 0; i < (sizeof(g_adv_hash_table) / sizeof(g_adv_hash_table[0])); ++i)
    {
        STAILQ_INIT(&g_adv_hash_table[i]);
    }
    STAILQ_INIT(&g_adv_reports_retransmission_list1);
    STAILQ_INIT(&g_adv_reports_retransmission_list2);
    TAILQ_INIT(&g_adv_reports_hist_list);
    for (uint32_t i = 0; i < (sizeof(g_arr_of_adv_reports) / sizeof(g_arr_of_adv_reports[0])); ++i)
    {
        adv_reports_list_elem_t* p_elem = &g_arr_of_adv_reports[i];
        memset(p_elem, 0, sizeof(*p_elem));
        p_elem->is_in_hash_table           = false;
        p_elem->is_in_retransmission_list1 = false;
        p_elem->is_in_retransmission_list2 = false;
        p_elem->adv_report.timestamp       = 0;
        p_elem->adv_report.data_len        = 0; // mark adv_report as free in hist_list
        TAILQ_INSERT_TAIL(&g_adv_reports_hist_list, p_elem, hist_list);
    }
}

void
adv_table_deinit(void)
{
    os_mutex_delete(&gp_adv_reports_mutex);
}

static bool
mac_address_is_equal(const mac_address_bin_t* const p_mac1, const mac_address_bin_t* const p_mac2)
{
    if (0 == memcmp(p_mac1->mac, p_mac2->mac, MAC_ADDRESS_NUM_BYTES))
    {
        return true;
    }
    return false;
}

ADV_TABLE_STATIC
uint32_t
adv_report_calc_hash(const mac_address_bin_t* const p_mac)
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
adv_reports_list_elem_t*
adv_hash_table_search(const mac_address_bin_t* const p_mac)
{
    const uint32_t           hash_val    = adv_report_calc_hash(p_mac);
    const uint32_t           hash_idx    = hash_val % ADV_TABLE_HASH_SIZE;
    adv_reports_list_elem_t* p_hash_elem = NULL;
    STAILQ_FOREACH(p_hash_elem, &g_adv_hash_table[hash_idx], hash_table_list)
    {
        if (mac_address_is_equal(p_mac, &p_hash_elem->adv_report.tag_mac))
        {
            return p_hash_elem;
        }
    }
    return NULL;
}

ADV_TABLE_STATIC
void
adv_hash_table_add(adv_reports_list_elem_t* p_elem)
{
    const uint32_t hash_val = adv_report_calc_hash(&p_elem->adv_report.tag_mac);
    const uint32_t hash_idx = hash_val % ADV_TABLE_HASH_SIZE;

    STAILQ_INSERT_TAIL(&g_adv_hash_table[hash_idx], p_elem, hash_table_list);
    p_elem->is_in_hash_table = true;
}

ADV_TABLE_STATIC
void
adv_hash_table_remove(adv_reports_list_elem_t* p_elem)
{
    if (!p_elem->is_in_hash_table)
    {
        return;
    }
    const uint32_t hash_val = adv_report_calc_hash(&p_elem->adv_report.tag_mac);
    const uint32_t hash_idx = hash_val % ADV_TABLE_HASH_SIZE;

    STAILQ_REMOVE(&g_adv_hash_table[hash_idx], p_elem, adv_reports_list_elem_t, hash_table_list);
    p_elem->is_in_hash_table = false;
}

static bool
adv_table_put_unsafe(const adv_report_t* const p_adv)
{
    // Check if we already have advertisement with this MAC
    adv_reports_list_elem_t* p_elem = adv_hash_table_search(&p_adv->tag_mac);
    if (NULL == p_elem)
    {
        // not found in the table, insert if the retransmission list is not full
        p_elem = TAILQ_LAST(&g_adv_reports_hist_list, adv_report_hist_list_t);

        adv_hash_table_remove(p_elem);

        p_elem->adv_report = *p_adv;
        adv_hash_table_add(p_elem);
    }
    else
    {
        adv_counter_t prev_counter         = p_elem->adv_report.samples_counter;
        p_elem->adv_report                 = *p_adv; // Update data
        p_elem->adv_report.samples_counter = prev_counter + 1;
    }
    if (!p_elem->is_in_retransmission_list1)
    {
        STAILQ_INSERT_TAIL(&g_adv_reports_retransmission_list1, p_elem, retransmission_list1);
        p_elem->is_in_retransmission_list1 = true;
    }
    if (!p_elem->is_in_retransmission_list2)
    {
        STAILQ_INSERT_TAIL(&g_adv_reports_retransmission_list2, p_elem, retransmission_list2);
        p_elem->is_in_retransmission_list2 = true;
    }
    TAILQ_REMOVE(&g_adv_reports_hist_list, p_elem, hist_list);
    TAILQ_INSERT_HEAD(&g_adv_reports_hist_list, p_elem, hist_list);

    return true;
}

bool
adv_table_put(const adv_report_t* const p_adv)
{
    os_mutex_lock(gp_adv_reports_mutex);
    const bool res = adv_table_put_unsafe(p_adv);
    os_mutex_unlock(gp_adv_reports_mutex);
    return res;
}

static void
adv_table_read_retransmission_list1_and_clear_unsafe(adv_report_table_t* const p_reports)
{
    p_reports->num_of_advs = 0;
    for (;;)
    {
        adv_reports_list_elem_t* p_elem = STAILQ_FIRST(&g_adv_reports_retransmission_list1);
        if (NULL == p_elem)
        {
            break;
        }
        STAILQ_REMOVE_HEAD(&g_adv_reports_retransmission_list1, retransmission_list1);
        p_elem->is_in_retransmission_list1 = false;
        if (p_reports->num_of_advs < (sizeof(p_reports->table) / sizeof(p_reports->table[0])))
        {
            p_reports->table[p_reports->num_of_advs] = p_elem->adv_report;
            p_reports->num_of_advs += 1;
        }
    }
}

static void
adv_table_read_retransmission_list2_and_clear_unsafe(adv_report_table_t* const p_reports)
{
    p_reports->num_of_advs = 0;
    for (;;)
    {
        adv_reports_list_elem_t* p_elem = STAILQ_FIRST(&g_adv_reports_retransmission_list2);
        if (NULL == p_elem)
        {
            break;
        }
        STAILQ_REMOVE_HEAD(&g_adv_reports_retransmission_list2, retransmission_list2);
        p_elem->is_in_retransmission_list2 = false;
        if (p_reports->num_of_advs < (sizeof(p_reports->table) / sizeof(p_reports->table[0])))
        {
            p_reports->table[p_reports->num_of_advs] = p_elem->adv_report;
            p_reports->num_of_advs += 1;
        }
    }
}

void
adv_table_read_retransmission_list1_and_clear(adv_report_table_t* const p_reports)
{
    os_mutex_lock(gp_adv_reports_mutex);
    adv_table_read_retransmission_list1_and_clear_unsafe(p_reports);
    os_mutex_unlock(gp_adv_reports_mutex);
}

void
adv_table_read_retransmission_list2_and_clear(adv_report_table_t* const p_reports)
{
    os_mutex_lock(gp_adv_reports_mutex);
    adv_table_read_retransmission_list2_and_clear_unsafe(p_reports);
    os_mutex_unlock(gp_adv_reports_mutex);
}

static void
adv_table_read_history_unsafe(
    adv_report_table_t* const p_reports,
    const time_t              cur_time,
    const bool                flag_use_timestamps,
    const uint32_t            filter,
    const bool                flag_use_filter)
{
    p_reports->num_of_advs = 0;

    adv_reports_list_elem_t* p_elem = NULL;
    TAILQ_FOREACH(p_elem, &g_adv_reports_hist_list, hist_list)
    {
        if (p_reports->num_of_advs >= (sizeof(p_reports->table) / sizeof(p_reports->table[0])))
        {
            break;
        }
        if (0 == p_elem->adv_report.data_len)
        {
            break;
        }
        if (flag_use_timestamps)
        {
            if (flag_use_filter && ((cur_time - p_elem->adv_report.timestamp) > filter))
            {
                break;
            }
        }
        else
        {
            if (flag_use_filter && ((int32_t)(p_elem->adv_report.timestamp - filter) <= 0))
            {
                break;
            }
        }
        p_reports->table[p_reports->num_of_advs] = p_elem->adv_report;
        p_reports->num_of_advs += 1;
    }
}

void
adv_table_history_read(
    adv_report_table_t* const p_reports,
    const time_t              cur_time,
    const bool                flag_use_timestamps,
    const uint32_t            filter,
    const bool                flag_use_filter)
{
    os_mutex_lock(gp_adv_reports_mutex);
    adv_table_read_history_unsafe(p_reports, cur_time, flag_use_timestamps, filter, flag_use_filter);
    os_mutex_unlock(gp_adv_reports_mutex);
}

static void
adv_table_read_statistics_unsafe(adv_report_table_t* const p_reports)
{
    p_reports->num_of_advs = 0;

    adv_reports_list_elem_t* p_elem = NULL;
    TAILQ_FOREACH(p_elem, &g_adv_reports_hist_list, hist_list)
    {
        if (p_reports->num_of_advs >= (sizeof(p_reports->table) / sizeof(p_reports->table[0])))
        {
            break;
        }
        if (0 == p_elem->adv_report.timestamp)
        {
            break;
        }
        p_reports->table[p_reports->num_of_advs] = p_elem->adv_report;
        p_elem->adv_report.samples_counter       = 0;
        p_reports->num_of_advs += 1;
    }
}

void
adv_table_statistics_read(adv_report_table_t* const p_reports)
{
    os_mutex_lock(gp_adv_reports_mutex);
    adv_table_read_statistics_unsafe(p_reports);
    os_mutex_unlock(gp_adv_reports_mutex);
}

static void
adv_retransmission_list1_clear_unsafe(void)
{
    while (1)
    {
        adv_reports_list_elem_t* p_elem = STAILQ_FIRST(&g_adv_reports_retransmission_list1);
        if (NULL == p_elem)
        {
            break;
        }
        STAILQ_REMOVE_HEAD(&g_adv_reports_retransmission_list1, retransmission_list1);
        p_elem->is_in_retransmission_list1 = false;
    }
}

static void
adv_retransmission_list2_clear_unsafe(void)
{
    while (1)
    {
        adv_reports_list_elem_t* p_elem = STAILQ_FIRST(&g_adv_reports_retransmission_list2);
        if (NULL == p_elem)
        {
            break;
        }
        STAILQ_REMOVE_HEAD(&g_adv_reports_retransmission_list2, retransmission_list2);
        p_elem->is_in_retransmission_list2 = false;
    }
}

void
adv_table_clear(void)
{
    os_mutex_lock(gp_adv_reports_mutex);
    adv_retransmission_list1_clear_unsafe();
    adv_retransmission_list2_clear_unsafe();

    adv_reports_list_elem_t* p_elem = NULL;
    TAILQ_FOREACH(p_elem, &g_adv_reports_hist_list, hist_list)
    {
        p_elem->adv_report.timestamp       = 0;
        p_elem->adv_report.samples_counter = 0;
        p_elem->adv_report.data_len        = 0; // mark adv_report as free in hist_list
    }

    os_mutex_unlock(gp_adv_reports_mutex);
}
