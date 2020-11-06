/**
 * @file test_os_task.cpp
 * @author TheSomeMan
 * @date 2020-11-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "flashfatfs.h"
#include <string>
#include <sys/stat.h>
#include "gtest/gtest.h"
#include "esp_log_wrapper.hpp"
#include "esp_err.h"
#include "os_task.h"

using namespace std;

/*** Google-test class implementation
 * *********************************************************************************/

class TestOsTask;
static TestOsTask *g_pTestClass;

class TestOsTask : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        g_pTestClass = this;
    }

    void
    TearDown() override
    {
        esp_log_wrapper_deinit();
        g_pTestClass = nullptr;
    }

public:
    TestOsTask();

    ~TestOsTask() override;

    string m_taskName;
    int    m_taskParam;

    TaskHandle_t           m_createdTaskHandle;
    TaskFunction_t         m_createdTaskFunc;
    string                 m_createdTaskName;
    configSTACK_DEPTH_TYPE m_createdTaskStackDepth;
    void *                 m_createdTaskParam;
    UBaseType_t            m_createdTaskPriority;
};

TestOsTask::TestOsTask()
    : m_taskParam(0)
    , m_createdTaskHandle(nullptr)
    , m_createdTaskFunc(nullptr)
    , m_createdTaskStackDepth(0)
    , m_createdTaskParam(nullptr)
    , m_createdTaskPriority(0)
    , Test()
{
}

TestOsTask::~TestOsTask() = default;

extern "C" {

char *
pcTaskGetName(TaskHandle_t xTaskToQuery)
{
    assert(nullptr == xTaskToQuery);
    if (g_pTestClass->m_taskName == string(""))
    {
        return nullptr;
    }
    return const_cast<char *>(g_pTestClass->m_taskName.c_str());
}

BaseType_t
xTaskCreate(
    TaskFunction_t               pxTaskCode,
    const char *const            pcName,
    const configSTACK_DEPTH_TYPE usStackDepth,
    void *const                  pvParameters,
    UBaseType_t                  uxPriority,
    TaskHandle_t *const          pxCreatedTask)
{
    if (nullptr == g_pTestClass->m_createdTaskHandle)
    {
        return pdFAIL;
    }
    *pxCreatedTask                  = g_pTestClass->m_createdTaskHandle;
    g_pTestClass->m_createdTaskFunc = pxTaskCode;
    g_pTestClass->m_createdTaskName.assign(pcName);
    g_pTestClass->m_createdTaskStackDepth = usStackDepth;
    g_pTestClass->m_createdTaskParam      = pvParameters;
    g_pTestClass->m_createdTaskPriority   = uxPriority;
    return pdPASS;
}

static ATTR_NORETURN void
task_func(void *p_param)
{
    (void)p_param;
    while (true)
    {
    }
}

} // extern "C"

#define TEST_CHECK_LOG_RECORD_EX(tag_, level_, msg_, flag_skip_file_info_) \
    do \
    { \
        ASSERT_FALSE(esp_log_wrapper_is_empty()); \
        const LogRecord log_record = esp_log_wrapper_pop(); \
        ASSERT_EQ(level_, log_record.level); \
        ASSERT_EQ(string(tag_), log_record.tag); \
        if (flag_skip_file_info_) \
        { \
            const char *p = strchr(log_record.message.c_str(), ' '); \
            assert(NULL != p); \
            p += 1; \
            p = strchr(p, ' '); \
            assert(NULL != p); \
            p += 1; \
            p = strchr(p, ' '); \
            assert(NULL != p); \
            p += 1; \
            ASSERT_EQ(string(msg_), p); \
        } \
        else \
        { \
            ASSERT_EQ(string(msg_), log_record.message); \
        } \
    } while (0)

#define TEST_CHECK_LOG_RECORD(level_, msg_)         TEST_CHECK_LOG_RECORD_EX("os_task", level_, msg_, false);
#define TEST_CHECK_LOG_RECORD_NO_FILE(level_, msg_) TEST_CHECK_LOG_RECORD_EX("os_task", level_, msg_, true);

/*** Unit-Tests
 * *******************************************************************************************************/

TEST_F(TestOsTask, os_task_get_name_ok) // NOLINT
{
    this->m_taskName.assign("my_task_name1");
    const char *taskName = os_task_get_name();
    ASSERT_NE(nullptr, taskName);
    ASSERT_EQ(string(taskName), string("my_task_name1"));
}

TEST_F(TestOsTask, os_task_get_name_null) // NOLINT
{
    const char *taskName = os_task_get_name();
    ASSERT_NE(nullptr, taskName);
    ASSERT_EQ(string(taskName), string("???"));
}

struct tskTaskControlBlock
{
    int stub;
};

TEST_F(TestOsTask, os_task_create_ok) // NOLINT
{
    const char *             task_name   = "my_task_name2";
    const uint32_t           stack_depth = 2048;
    const os_task_priority_t priority    = 3;
    this->m_taskName.assign(task_name);
    os_task_handle_t           h_task;
    struct tskTaskControlBlock taskControlBlock = { 0 };
    this->m_createdTaskHandle                   = &taskControlBlock;
    ASSERT_TRUE(os_task_create(&task_func, task_name, stack_depth, &this->m_taskParam, priority, &h_task));
    ASSERT_EQ(&taskControlBlock, h_task);
    ASSERT_EQ(g_pTestClass->m_createdTaskFunc, &task_func);
    ASSERT_EQ(g_pTestClass->m_createdTaskName, task_name);
    ASSERT_EQ(g_pTestClass->m_createdTaskStackDepth, stack_depth);
    ASSERT_EQ(g_pTestClass->m_createdTaskParam, &this->m_taskParam);
    ASSERT_EQ(g_pTestClass->m_createdTaskPriority, priority);
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_INFO,
        "[my_task_name2] Start thread 'my_task_name2' with priority 3, stack size 2048 bytes");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestOsTask, os_task_create_fail) // NOLINT
{
    const char *             task_name   = "my_task_name2";
    const uint32_t           stack_depth = 2048;
    const os_task_priority_t priority    = 3;
    this->m_taskName.assign(task_name);
    os_task_handle_t h_task;
    this->m_createdTaskHandle = nullptr;
    ASSERT_FALSE(os_task_create(&task_func, task_name, stack_depth, &this->m_taskParam, priority, &h_task));
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_INFO,
        "[my_task_name2] Start thread 'my_task_name2' with priority 3, stack size 2048 bytes");
    TEST_CHECK_LOG_RECORD_NO_FILE(ESP_LOG_ERROR, "Failed to start thread 'my_task_name2'");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
