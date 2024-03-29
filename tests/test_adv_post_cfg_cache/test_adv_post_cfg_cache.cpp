/**
 * @file test_adv_post_cfg_cache.cpp
 * @author TheSomeMan
 * @date 2023-09-17
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "adv_post_cfg_cache.h"
#include "gtest/gtest.h"
#include <string>
#include "os_mutex.h"

using namespace std;

class TestAdvPostCfgCache;

static TestAdvPostCfgCache* g_pTestClass;

/*** Google-test class implementation
 * *********************************************************************************/

class TestAdvPostCfgCache : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        this->m_is_mutex_busy = false;
        g_pTestClass          = this;
    }

    void
    TearDown() override
    {
        adv_post_cfg_cache_deinit();
        g_pTestClass = nullptr;
    }

public:
    TestAdvPostCfgCache();

    ~TestAdvPostCfgCache() override;

    bool       m_is_mutex_busy = false;
    os_mutex_t p_mutex;
};

TestAdvPostCfgCache::TestAdvPostCfgCache()
    : Test()
{
}

TestAdvPostCfgCache::~TestAdvPostCfgCache() = default;

extern "C" {

os_mutex_t
os_mutex_create_static(os_mutex_static_t* const p_mutex_static)
{
    assert(nullptr != p_mutex_static);
    g_pTestClass->p_mutex = reinterpret_cast<os_mutex_t>(p_mutex_static);
    return g_pTestClass->p_mutex;
}

void
os_mutex_delete(os_mutex_t* const ph_mutex)
{
    assert(nullptr != ph_mutex);
    assert(nullptr != *ph_mutex);
    assert(g_pTestClass->p_mutex == *ph_mutex);
    *ph_mutex = nullptr;
}

void
os_mutex_lock(os_mutex_t const h_mutex)
{
    assert(nullptr != h_mutex);
    assert(g_pTestClass->p_mutex == h_mutex);
    assert(!g_pTestClass->m_is_mutex_busy);
    g_pTestClass->m_is_mutex_busy = true;
}

bool
os_mutex_try_lock(os_mutex_t const h_mutex)
{
    assert(nullptr != h_mutex);
    assert(g_pTestClass->p_mutex == h_mutex);
    if (g_pTestClass->m_is_mutex_busy)
    {
        return false;
    }
    g_pTestClass->m_is_mutex_busy = true;
    return true;
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
    assert(nullptr != h_mutex);
    assert(g_pTestClass->p_mutex == h_mutex);
    assert(g_pTestClass->m_is_mutex_busy);
    g_pTestClass->m_is_mutex_busy = false;
}

} // extern "C"

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAdvPostCfgCache, test_uninitialized_try_lock_successful) // NOLINT
{
    adv_post_cfg_cache_init();
    adv_post_cfg_cache_init();

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);
}

TEST_F(TestAdvPostCfgCache, test_uninitialized_try_lock_unsuccessful) // NOLINT
{
    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    g_pTestClass->m_is_mutex_busy = true;

    p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    ASSERT_EQ(nullptr, p_cfg_cache);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);
}

TEST_F(TestAdvPostCfgCache, test_uninitialized_lock) // NOLINT
{
    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    p_cfg_cache = adv_post_cfg_cache_mutex_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);
}

TEST_F(TestAdvPostCfgCache, test_initialized_try_lock_successful) // NOLINT
{
    adv_post_cfg_cache_init();

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_deinit();
}

TEST_F(TestAdvPostCfgCache, test_initialized_try_lock_unsuccessful) // NOLINT
{
    adv_post_cfg_cache_init();

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    g_pTestClass->m_is_mutex_busy = true;

    p_cfg_cache = adv_post_cfg_cache_mutex_try_lock();
    ASSERT_EQ(nullptr, p_cfg_cache);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_deinit();
}

TEST_F(TestAdvPostCfgCache, test_initialized_lock) // NOLINT
{
    adv_post_cfg_cache_init();

    adv_post_cfg_cache_t* p_cfg_cache = adv_post_cfg_cache_mutex_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    p_cfg_cache = adv_post_cfg_cache_mutex_lock();
    ASSERT_NE(nullptr, p_cfg_cache);
    ASSERT_TRUE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_mutex_unlock(&p_cfg_cache);
    ASSERT_EQ(nullptr, p_cfg_cache);
    ASSERT_FALSE(g_pTestClass->m_is_mutex_busy);

    adv_post_cfg_cache_deinit();
}
