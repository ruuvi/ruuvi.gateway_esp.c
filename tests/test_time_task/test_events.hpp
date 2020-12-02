#ifndef TEST_EVENTS_H
#define TEST_EVENTS_H

#include "lwip/arch.h"
#include "sntp.h"
#include "os_task.h"

typedef enum TestEventType_Tag
{
    TestEventType_SNTP_SetOperatingMode,
    TestEventType_SNTP_SetSyncMode,
    TestEventType_SNTP_SetServerName,
    TestEventType_SNTP_Init,
    TestEventType_SNTP_Stop,
    TestEventType_Delay,
} TestEventType_e;

/*** Test-Events classes implementation
 * *******************************************************************************/

class TestEvent
{
public:
    TestEventType_e eventType;

    explicit TestEvent(const TestEventType_e eventType)
        : eventType(eventType)
    {
    }
};

class TestEventSntpSetOperatingMode : public TestEvent
{
public:
    u8_t operating_mode;

    explicit TestEventSntpSetOperatingMode(const u8_t operating_mode)
        : TestEvent(TestEventType_SNTP_SetOperatingMode)
        , operating_mode(operating_mode)
    {
    }
};

class TestEventSntpSetSyncMode : public TestEvent
{
public:
    sntp_sync_mode_t sync_mode;

    explicit TestEventSntpSetSyncMode(const sntp_sync_mode_t sync_mode)
        : TestEvent(TestEventType_SNTP_SetSyncMode)
        , sync_mode(sync_mode)
    {
    }
};

class TestEventSntpSetServerName : public TestEvent
{
public:
    u8_t        idx;
    std::string server;

    TestEventSntpSetServerName(const u8_t idx, const char *server)
        : TestEvent(TestEventType_SNTP_SetServerName)
        , idx(idx)
        , server(server)
    {
    }
};

class TestEventSntpInit : public TestEvent
{
public:
    TestEventSntpInit()
        : TestEvent(TestEventType_SNTP_Init)
    {
    }
};

class TestEventSntpStop : public TestEvent
{
public:
    TestEventSntpStop()
        : TestEvent(TestEventType_SNTP_Stop)
    {
    }
};

class TestEventDelay : public TestEvent
{
public:
    os_delta_ticks_t delay_ticks;
    explicit TestEventDelay(const os_delta_ticks_t delay_ticks)
        : TestEvent(TestEventType_Delay)
        , delay_ticks(delay_ticks)
    {
    }
};

#endif // TEST_EVENTS_H
