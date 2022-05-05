#ifndef TEST_EVENTS_H
#define TEST_EVENTS_H

#include "lwip/arch.h"
#include "lwip/ip_addr.h"
#include "sntp.h"
#include "os_task.h"

typedef enum TestEventType_Tag
{
    TestEventType_SNTP_SetOperatingMode,
    TestEventType_SNTP_SetSyncMode,
    TestEventType_SNTP_SetServerName,
    TestEventType_SNTP_SetServer,
    TestEventType_SNTP_SetServerMode,
    TestEventType_SNTP_Init,
    TestEventType_SNTP_Stop,
    TestEventType_NetworkReconnect,
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

class TestEventSntpSetServer : public TestEvent
{
    static const ip_addr_t ip_addr_zero;

public:
    u8_t            idx;
    const ip_addr_t addr;

    TestEventSntpSetServer(const u8_t idx, const ip_addr_t *p_addr)
        : TestEvent(TestEventType_SNTP_SetServer)
        , idx(idx)
        , addr((nullptr != p_addr) ? *p_addr : ip_addr_zero)
    {
    }
};

const ip_addr_t TestEventSntpSetServer::ip_addr_zero = { 0 };

class TestEventSntpSetServerMode : public TestEvent
{
public:
    int set_servers_from_dhcp;

    TestEventSntpSetServerMode(int set_servers_from_dhcp)
        : TestEvent(TestEventType_SNTP_SetServerMode)
        , set_servers_from_dhcp(set_servers_from_dhcp)
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

class TestEventNetworkReconnect : public TestEvent
{
public:
    TestEventNetworkReconnect()
        : TestEvent(TestEventType_NetworkReconnect)
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
