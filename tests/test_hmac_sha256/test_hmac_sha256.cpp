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

TEST_F(TestHMAC_SHA256, test_hmac_sha256_in_bin_buf) // NOLINT
{
    // https://en.wikipedia.org/wiki/HMAC
    // HMAC_SHA256("key", "The quick brown fox jumps over the lazy dog") =
    // f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8

    ASSERT_TRUE(hmac_sha256_set_key("key"));
    hmac_sha256_t hmac_sha256 = { 0 };
    ASSERT_TRUE(hmac_sha256_calc("The quick brown fox jumps over the lazy dog", &hmac_sha256));
    const std::vector<uint8_t> exp_hmac_sha256 = {
        0xf7, 0xbc, 0x83, 0xf4, 0x30, 0x53, 0x84, 0x24, 0xb1, 0x32, 0x98, 0xe6, 0xaa, 0x6f, 0xb1, 0x43,
        0xef, 0x4d, 0x59, 0xa1, 0x49, 0x46, 0x17, 0x59, 0x97, 0x47, 0x9d, 0xbc, 0x2d, 0x1a, 0x3c, 0xd8,
    };
    ASSERT_EQ(exp_hmac_sha256, std::vector<uint8_t>(&hmac_sha256.buf[0], &hmac_sha256.buf[32]));
}

TEST_F(TestHMAC_SHA256, test_hmac_sha256_in_str_buf) // NOLINT
{
    // https://en.wikipedia.org/wiki/HMAC
    // HMAC_SHA256("key", "The quick brown fox jumps over the lazy dog") =
    // f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8

    ASSERT_TRUE(hmac_sha256_set_key("key"));
    const hmac_sha256_str_t hmac_sha256_str = hmac_sha256_calc_str("The quick brown fox jumps over the lazy dog");
    ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
    ASSERT_EQ(string("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"), string(hmac_sha256_str.buf));
}
