/**
 * @file test_hmac_sha256.cpp
 * @author TheSomeMan
 * @date 2021-07-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "hmac_sha256.h"
#include "gtest/gtest.h"
#include <string>
#include "ruuvi_device_id.h"

using namespace std;

class TestHMAC_SHA256;
static TestHMAC_SHA256* g_pObj;

/*** Google-test class implementation
 * *********************************************************************************/

class TestHMAC_SHA256 : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pObj             = this;
        malloc_fail_on_cnt = 0;
        malloc_cnt         = 0;
    }

    void
    TearDown() override
    {
        g_pObj = nullptr;
    }

public:
    uint32_t malloc_cnt;
    uint32_t malloc_fail_on_cnt;
    TestHMAC_SHA256();
    ~TestHMAC_SHA256() override;
};

TestHMAC_SHA256::TestHMAC_SHA256()
    : Test()
    , malloc_cnt(0)
    , malloc_fail_on_cnt(0)
{
}

TestHMAC_SHA256::~TestHMAC_SHA256() = default;

#ifdef __cplusplus
extern "C" {
#endif

void*
mbedtls_calloc(size_t n, size_t size)
{
    if (0 != g_pObj->malloc_fail_on_cnt)
    {
        if (++g_pObj->malloc_cnt >= g_pObj->malloc_fail_on_cnt)
        {
            return nullptr;
        }
    }
    return calloc(n, size);
}

void
mbedtls_free(void* ptr)
{
    free(ptr);
}

void*
os_malloc(const size_t size)
{
    if (0 != g_pObj->malloc_fail_on_cnt)
    {
        if (++g_pObj->malloc_cnt >= g_pObj->malloc_fail_on_cnt)
        {
            return nullptr;
        }
    }
    return malloc(size);
}

void
os_free_internal(void* ptr)
{
    free(ptr);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    if (0 != g_pObj->malloc_fail_on_cnt)
    {
        if (++g_pObj->malloc_cnt >= g_pObj->malloc_fail_on_cnt)
        {
            return nullptr;
        }
    }
    return calloc(nmemb, size);
}

#ifdef __cplusplus
}
#endif

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestHMAC_SHA256, test_hmac_sha256_in_bin_buf) // NOLINT
{
    // https://en.wikipedia.org/wiki/HMAC
    // HMAC_SHA256("key", "The quick brown fox jumps over the lazy dog") =
    // f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8

    ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi("key"));
    hmac_sha256_t hmac_sha256 = { 0 };
    const string  msg         = "The quick brown fox jumps over the lazy dog";
    ASSERT_TRUE(hmac_sha256_calc_for_http_ruuvi(msg.c_str(), &hmac_sha256));
    const std::vector<uint8_t> exp_hmac_sha256 = {
        0xf7, 0xbc, 0x83, 0xf4, 0x30, 0x53, 0x84, 0x24, 0xb1, 0x32, 0x98, 0xe6, 0xaa, 0x6f, 0xb1, 0x43,
        0xef, 0x4d, 0x59, 0xa1, 0x49, 0x46, 0x17, 0x59, 0x97, 0x47, 0x9d, 0xbc, 0x2d, 0x1a, 0x3c, 0xd8,
    };
    ASSERT_EQ(exp_hmac_sha256, std::vector<uint8_t>(&hmac_sha256.buf[0], &hmac_sha256.buf[32]));
    str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);
    ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
    ASSERT_EQ(string("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"), string(hmac_sha256_str.buf));
    str_buf_free_buf(&hmac_sha256_str);
}

TEST_F(TestHMAC_SHA256, test1_hmac_sha256_with_device_id_as_encryption_key) // NOLINT
{
    const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:38" };
    ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));

    hmac_sha256_t hmac_sha256 = { 0 };
    ASSERT_TRUE(hmac_sha256_calc_for_http_ruuvi("The quick brown fox jumps over the lazy dog", &hmac_sha256));
    str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);

    ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
    ASSERT_EQ(string("04c84e84c41c795e449b3ad9f11304f3d6665d74f33df10d2bf8e832ca26a814"), string(hmac_sha256_str.buf));
    str_buf_free_buf(&hmac_sha256_str);
}

TEST_F(TestHMAC_SHA256, test2_hmac_sha256_with_device_id_as_encryption_key) // NOLINT
{
    const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:38" };
    ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));

    const string json_str = R"({
	"status":	"online",
	"gw_mac":	"C8:25:2D:8E:9C:2C",
	"timestamp":	"1625822511",
	"nonce":	"1763874810"
})";

    hmac_sha256_t hmac_sha256 = { 0 };
    ASSERT_TRUE(hmac_sha256_calc_for_http_ruuvi(json_str.c_str(), &hmac_sha256));
    str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);

    ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
    ASSERT_EQ(string("040f70eaf7e23084d2ae1171bb48ca3ebf66271c4ec24c4cff03d3d5d96f0d5d"), string(hmac_sha256_str.buf));
    str_buf_free_buf(&hmac_sha256_str);
}

TEST_F(TestHMAC_SHA256, test1_hmac_sha256_twice_with_the_same_key) // NOLINT
{
    const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:38" };
    ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));
    ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));
    hmac_sha256_t hmac_sha256 = { 0 };
    ASSERT_TRUE(hmac_sha256_calc_for_http_ruuvi("The quick brown fox jumps over the lazy dog", &hmac_sha256));
    str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);
    ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
    ASSERT_EQ(string("04c84e84c41c795e449b3ad9f11304f3d6665d74f33df10d2bf8e832ca26a814"), string(hmac_sha256_str.buf));
    str_buf_free_buf(&hmac_sha256_str);
}

TEST_F(TestHMAC_SHA256, test1_hmac_sha256_twice_with_the_different_key) // NOLINT
{
    {
        const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:38" };
        ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));
        hmac_sha256_t hmac_sha256 = { 0 };
        ASSERT_TRUE(hmac_sha256_calc_for_http_ruuvi("The quick brown fox jumps over the lazy dog", &hmac_sha256));
        str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);
        ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
        ASSERT_EQ(
            string("04c84e84c41c795e449b3ad9f11304f3d6665d74f33df10d2bf8e832ca26a814"),
            string(hmac_sha256_str.buf));
        str_buf_free_buf(&hmac_sha256_str);
    }
    {
        const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:39" };
        ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));
        hmac_sha256_t hmac_sha256 = { 0 };
        ASSERT_TRUE(hmac_sha256_calc_for_http_ruuvi("The quick brown fox jumps over the lazy dog", &hmac_sha256));
        str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);
        ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
        ASSERT_EQ(
            string("b058603d92e7b3adb834f009b2e2bbf720d3046921203983cf49139b1c177dbd"),
            string(hmac_sha256_str.buf));
        str_buf_free_buf(&hmac_sha256_str);
    }
}

TEST_F(TestHMAC_SHA256, test_hmac_sha256_with_long_encryption_key) // NOLINT
{
    ASSERT_FALSE(
        hmac_sha256_set_key_for_http_ruuvi("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0"));
}

TEST_F(TestHMAC_SHA256, test_hmac_sha256_malloc_fail_1) // NOLINT
{
    g_pObj->malloc_fail_on_cnt           = 1;
    const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:38" };
    ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));
    hmac_sha256_t hmac_sha256 = { 0 };
    ASSERT_FALSE(hmac_sha256_calc_for_http_ruuvi("The quick brown fox jumps over the lazy dog", &hmac_sha256));
}

TEST_F(TestHMAC_SHA256, test_hmac_sha256_malloc_fail_2) // NOLINT
{
    g_pObj->malloc_fail_on_cnt           = 2;
    const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:38" };
    ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));
    hmac_sha256_t hmac_sha256 = { 0 };
    ASSERT_FALSE(hmac_sha256_calc_for_http_ruuvi("The quick brown fox jumps over the lazy dog", &hmac_sha256));
}

TEST_F(TestHMAC_SHA256, test_hmac_sha256_malloc_fail_3) // NOLINT
{
    g_pObj->malloc_fail_on_cnt           = 3;
    const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:38" };
    ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));
    hmac_sha256_t hmac_sha256 = { 0 };
    ASSERT_TRUE(hmac_sha256_calc_for_http_ruuvi("The quick brown fox jumps over the lazy dog", &hmac_sha256));
    str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);
    ASSERT_FALSE(hmac_sha256_is_str_valid(&hmac_sha256_str));
    str_buf_free_buf(&hmac_sha256_str);
}

TEST_F(TestHMAC_SHA256, test1_hmac_sha256_for_different_targets) // NOLINT
{
    {
        const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:38" };
        ASSERT_TRUE(hmac_sha256_set_key_for_http_ruuvi(hmac_key.str_buf));
    }
    {
        const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:39" };
        ASSERT_TRUE(hmac_sha256_set_key_for_http_custom(hmac_key.str_buf));
    }
    {
        const nrf52_device_id_str_t hmac_key = { "40:98:A7:78:58:1A:E1:3A" };
        ASSERT_TRUE(hmac_sha256_set_key_for_stats(hmac_key.str_buf));
    }
    {
        hmac_sha256_t hmac_sha256 = { 0 };
        ASSERT_TRUE(hmac_sha256_calc_for_http_ruuvi("The quick brown fox jumps over the lazy dog", &hmac_sha256));
        str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);
        ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
        ASSERT_EQ(
            string("04c84e84c41c795e449b3ad9f11304f3d6665d74f33df10d2bf8e832ca26a814"),
            string(hmac_sha256_str.buf));
        str_buf_free_buf(&hmac_sha256_str);
    }
    {
        hmac_sha256_t hmac_sha256 = { 0 };
        ASSERT_TRUE(hmac_sha256_calc_for_http_custom("The quick brown fox jumps over the lazy dog", &hmac_sha256));
        str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);
        ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
        ASSERT_EQ(
            string("b058603d92e7b3adb834f009b2e2bbf720d3046921203983cf49139b1c177dbd"),
            string(hmac_sha256_str.buf));
        str_buf_free_buf(&hmac_sha256_str);
    }
    {
        hmac_sha256_t hmac_sha256 = { 0 };
        ASSERT_TRUE(hmac_sha256_calc_for_stats("The quick brown fox jumps over the lazy dog", &hmac_sha256));
        str_buf_t hmac_sha256_str = hmac_sha256_to_str_buf(&hmac_sha256);
        ASSERT_TRUE(hmac_sha256_is_str_valid(&hmac_sha256_str));
        ASSERT_EQ(
            string("25a17419b39df7743637d76d75af90fc79fc74958831d83553fce1137e14fe82"),
            string(hmac_sha256_str.buf));
        str_buf_free_buf(&hmac_sha256_str);
    }
}
