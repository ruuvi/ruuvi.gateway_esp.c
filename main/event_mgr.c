/**
 * @file event_mgr.c
 * @author TheSomeMan
 * @date 2020-12-01
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "event_mgr.h"
#include "assert.h"
#include "os_mutex.h"
#include "os_signal.h"
#include "sys/queue.h"
#include "os_malloc.h"
#include "esp_type_wrapper.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

typedef struct event_mgr_ev_info_t
{
    TAILQ_ENTRY(event_mgr_ev_info_t) list;
    os_signal_t*    p_signal;
    os_signal_num_e sig_num;
    bool            is_static;
} event_mgr_ev_info_t;

_Static_assert(
    sizeof(event_mgr_ev_info_static_t) == sizeof(event_mgr_ev_info_t),
    "sizeof(event_mgr_ev_info_static_t) == sizeof(event_mgr_ev_info_t)");

typedef struct event_mgr_queue_of_subscribers_t
{
    TAILQ_HEAD(event_mgr_queue_of_subscribers_head_t, event_mgr_ev_info_t) head;
} event_mgr_queue_of_subscribers_t;

struct event_mgr_t
{
    os_mutex_t                       h_mutex;
    os_mutex_static_t                mutex_static;
    event_mgr_queue_of_subscribers_t events[EVENT_MGR_EV_LAST];
};

static const char* TAG = "event_mgr";

static event_mgr_t g_event_mgr;

bool
event_mgr_init(void)
{
    event_mgr_t* const p_obj = &g_event_mgr;
    if (NULL != p_obj->h_mutex)
    {
        return false;
    }
    p_obj->h_mutex = os_mutex_create_static(&p_obj->mutex_static);
    for (uint32_t i = EVENT_MGR_EV_NONE; i < EVENT_MGR_EV_LAST; ++i)
    {
        const event_mgr_ev_e ev = (const event_mgr_ev_e)i;
        TAILQ_INIT(&p_obj->events[ev].head);
    }
    return true;
}

ATTR_UNUSED
void
event_mgr_deinit(void)
{
    event_mgr_t* const p_obj = &g_event_mgr;
    for (uint32_t i = EVENT_MGR_EV_NONE; i < EVENT_MGR_EV_LAST; ++i)
    {
        const event_mgr_ev_e                    ev      = (const event_mgr_ev_e)i;
        event_mgr_queue_of_subscribers_t* const p_queue = &p_obj->events[ev];
        while (!TAILQ_EMPTY(&p_queue->head))
        {
            event_mgr_ev_info_t* p_ev_info = TAILQ_FIRST(&p_queue->head);
            TAILQ_REMOVE(&p_queue->head, p_ev_info, list);
            if (!p_ev_info->is_static)
            {
                os_free(p_ev_info);
            }
        }
    }
    os_mutex_delete(&p_obj->h_mutex);
}

static event_mgr_ev_info_t*
event_mgr_create_ev_info_sig(os_signal_t* const p_signal, const os_signal_num_e sig_num)
{
    event_mgr_ev_info_t* const p_ev_info = os_calloc(1, sizeof(*p_ev_info));
    if (NULL == p_ev_info)
    {
        return NULL;
    }
    p_ev_info->p_signal  = p_signal;
    p_ev_info->sig_num   = sig_num;
    p_ev_info->is_static = false;
    return p_ev_info;
}

static void
event_mgr_mutex_lock(event_mgr_t* const p_obj)
{
    os_mutex_lock(p_obj->h_mutex);
}

static void
event_mgr_mutex_unlock(event_mgr_t* const p_obj)
{
    os_mutex_unlock(p_obj->h_mutex);
}

bool
event_mgr_subscribe_sig(const event_mgr_ev_e event, os_signal_t* const p_signal, const os_signal_num_e sig_num)
{
    event_mgr_t* const p_obj = &g_event_mgr;
    assert((event > EVENT_MGR_EV_NONE) && (event < EVENT_MGR_EV_LAST));
    event_mgr_ev_info_t* const p_ev_info = event_mgr_create_ev_info_sig(p_signal, sig_num);
    if (NULL == p_ev_info)
    {
        LOG_ERR("%s failed", "event_mgr_create_ev_info_sig");
        return false;
    }
    event_mgr_mutex_lock(p_obj);
    event_mgr_queue_of_subscribers_t* const p_queue_of_subscribers = &p_obj->events[event];
    TAILQ_INSERT_TAIL(&p_queue_of_subscribers->head, p_ev_info, list);
    event_mgr_mutex_unlock(p_obj);
    return true;
}

void
event_mgr_subscribe_sig_static(
    event_mgr_ev_info_static_t* const p_ev_info_mem,
    const event_mgr_ev_e              event,
    os_signal_t* const                p_signal,
    const os_signal_num_e             sig_num)
{
    event_mgr_t* const p_obj = &g_event_mgr;
    assert((event > EVENT_MGR_EV_NONE) && (event < EVENT_MGR_EV_LAST));

    event_mgr_ev_info_t* const p_ev_info = (event_mgr_ev_info_t*)p_ev_info_mem;

    p_ev_info->p_signal  = p_signal;
    p_ev_info->sig_num   = sig_num;
    p_ev_info->is_static = true;

    event_mgr_mutex_lock(p_obj);
    event_mgr_queue_of_subscribers_t* const p_queue_of_subscribers = &p_obj->events[event];
    TAILQ_INSERT_TAIL(&p_queue_of_subscribers->head, p_ev_info, list);
    event_mgr_mutex_unlock(p_obj);
}

void
event_mgr_notify(const event_mgr_ev_e event)
{
    event_mgr_t* const p_obj = &g_event_mgr;
    if ((event <= EVENT_MGR_EV_NONE) || (event >= EVENT_MGR_EV_LAST))
    {
        LOG_ERR(
            "Event %d is out of range [%d..%d]",
            (printf_int_t)event,
            (printf_int_t)(EVENT_MGR_EV_NONE + 1),
            (printf_int_t)(EVENT_MGR_EV_LAST - 1));
        return;
    }
    event_mgr_queue_of_subscribers_t* const p_queue_of_subscribers = &p_obj->events[event];
    event_mgr_mutex_lock(p_obj);
    event_mgr_ev_info_t* p_ev_info = NULL;
    TAILQ_FOREACH(p_ev_info, &p_queue_of_subscribers->head, list)
    {
        os_signal_send(p_ev_info->p_signal, p_ev_info->sig_num);
    }
    event_mgr_mutex_unlock(p_obj);
}
