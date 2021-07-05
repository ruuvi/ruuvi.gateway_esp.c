/**
 * @file test_hmac_sha256.cpp
 * @author TheSomeMan
 * @date 2021-07-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "hmac_sha256.h"
#include "gtest/gtest.h"
#include <string>

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestHMAC_SHA256 : public ::testing::Test
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
    TestHMAC_SHA256();

    ~TestHMAC_SHA256() override;
};

TestHMAC_SHA256::TestHMAC_SHA256()
    : Test()
{
}

TestHMAC_SHA256::~TestHMAC_SHA256() = default;

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestHMAC_SHA256, test_1) // NOLINT
{
    // https://en.wikipedia.org/wiki/HMAC
    // HMAC_SHA256("key", "The quick brown fox jumps over the lazy dog") =
    // f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8

    ASSERT_TRUE(hmac_sha256_set_key("key"));
    const hmac_sha256_str_t hmac_sha256 = hmac_sha256_calc("The quick brown fox jumps over the lazy dog");
    ASSERT_EQ(string("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"), string(hmac_sha256.buf));
}
