#include <stdio.h>
#include "gtest/gtest.h"

using namespace std;

class TestLeds : public ::testing::Test
{
private:

protected:

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }

public:
    TestLeds() : Test()
    {
    }

    ~TestLeds() override
    {
    }

};

TEST_F(TestLeds, test_1) // NOLINT
{
    ASSERT_EQ(1, 1);
}

TEST_F(TestLeds, test_2) // NOLINT
{
    ASSERT_EQ(1, 2);
}

