/**
 * @file test_app_malloc.cpp
 * @author TheSomeMan
 * @date 2020-10-02
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "app_malloc.h"
#include "gtest/gtest.h"
#include <string>

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestAppMalloc : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
    }

    void
    TearDown() override
    {
    }

public:
    TestAppMalloc();

    ~TestAppMalloc() override;
};

TestAppMalloc::TestAppMalloc()
    : Test()
{
}

TestAppMalloc::~TestAppMalloc() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestAppMalloc, test_app_malloc) // NOLINT
{
    void *ptr = app_malloc(1000);
    ASSERT_NE(nullptr, ptr);
    app_free(ptr);
}

TEST_F(TestAppMalloc, test_app_calloc) // NOLINT
{
    void *ptr = app_calloc(sizeof(uint32_t), 1000);
    ASSERT_NE(nullptr, ptr);
    app_free(ptr);
}
