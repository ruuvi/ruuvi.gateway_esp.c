/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "tls_shared_buf.h"
#include <assert.h>
#include <esp_attr.h>
#include "os_mutex.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "tls_shared_buf";

typedef union tls_shared_buf_t
{
    struct
    {
        tls_shared_buf_https_post_t https_post;
        tls_shared_buf_mqtts_t      mqtts;
    } shared1;
    struct
    {
        tls_shared_buf_https_download_t https_download;
    } shared2;
} tls_shared_buf_t;

static tls_shared_buf_t     g_tls_shared_buf;
static os_mutex_t IRAM_ATTR g_p_tls_shared_buf_mutex_https_post;
static os_mutex_static_t    g_tls_shared_buf_mutex_https_post_mem;
static os_mutex_t IRAM_ATTR g_p_tls_shared_buf_mutex_mqtts;
static os_mutex_static_t    g_tls_shared_buf_mutex_mqtts_mem;

void
tls_shared_buf_init(void)
{
    LOG_INFO("%s", __func__);
    g_p_tls_shared_buf_mutex_https_post = os_mutex_create_static(&g_tls_shared_buf_mutex_https_post_mem);
    if (!os_mutex_try_lock(g_p_tls_shared_buf_mutex_https_post))
    {
        LOG_ERR("Failed to lock https_post mutex");
        assert(0);
    }
    os_mutex_unlock(g_p_tls_shared_buf_mutex_https_post);

    g_p_tls_shared_buf_mutex_mqtts = os_mutex_create_static(&g_tls_shared_buf_mutex_mqtts_mem);
    if (!os_mutex_try_lock(g_p_tls_shared_buf_mutex_mqtts))
    {
        LOG_ERR("Failed to lock mqtts mutex");
        assert(0);
    }
    os_mutex_unlock(g_p_tls_shared_buf_mutex_mqtts);
}

tls_shared_buf_https_post_t*
tls_shared_buf_get_https_post(void)
{
    if (!os_mutex_try_lock(g_p_tls_shared_buf_mutex_https_post))
    {
        LOG_ERR("Failed to lock https_post mutex");
        assert(0);
        return NULL;
    }
    return &g_tls_shared_buf.shared1.https_post;
}

void
tls_shared_buf_unlock_https_post(tls_shared_buf_https_post_t** p_p_buf)
{
    assert(NULL != p_p_buf);
    assert(*p_p_buf == &g_tls_shared_buf.shared1.https_post);
    *p_p_buf = NULL;
    os_mutex_unlock(g_p_tls_shared_buf_mutex_https_post);
}

tls_shared_buf_mqtts_t*
tls_shared_buf_get_mqtts(void)
{
    if (!os_mutex_try_lock(g_p_tls_shared_buf_mutex_mqtts))
    {
        LOG_ERR("Failed to lock mqtts mutex");
        assert(0);
        return NULL;
    }
    return &g_tls_shared_buf.shared1.mqtts;
}

void
tls_shared_buf_unlock_mqtts(tls_shared_buf_mqtts_t** p_p_buf)
{
    assert(NULL != p_p_buf);
    assert(*p_p_buf == &g_tls_shared_buf.shared1.mqtts);
    *p_p_buf = NULL;
    os_mutex_unlock(g_p_tls_shared_buf_mutex_mqtts);
}

tls_shared_buf_https_download_t*
tls_shared_buf_get_https_download(void)
{
    if (!os_mutex_try_lock(g_p_tls_shared_buf_mutex_https_post))
    {
        LOG_ERR("Failed to lock https_post mutex");
        assert(0);
        return NULL;
    }
    if (!os_mutex_try_lock(g_p_tls_shared_buf_mutex_mqtts))
    {
        LOG_ERR("Failed to lock mqtts mutex");
        assert(0);
        return NULL;
    }
    return &g_tls_shared_buf.shared2.https_download;
}

void
tls_shared_buf_unlock_https_download(tls_shared_buf_https_download_t** p_p_buf)
{
    assert(NULL != p_p_buf);
    assert(*p_p_buf == &g_tls_shared_buf.shared2.https_download);
    *p_p_buf = NULL;
    os_mutex_unlock(g_p_tls_shared_buf_mutex_mqtts);
    os_mutex_unlock(g_p_tls_shared_buf_mutex_https_post);
}
