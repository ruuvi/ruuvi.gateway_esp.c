#ifndef TEST_EVENTS_H
#define TEST_EVENTS_H

typedef enum TestEventType_Tag
{
    TestEventType_Signal,
    TestEventType_ThreadExit,
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

enum TestEventThreadNum_e
{
    TEST_EVENT_THREAD_NUM_1 = 1,
    TEST_EVENT_THREAD_NUM_2 = 2,
};

class TestEventSignal : public TestEvent
{
public:
    TestEventThreadNum_e thread_num;
    os_signal_num_e      sig_num;

    explicit TestEventSignal(const TestEventThreadNum_e thread_num, const os_signal_num_e sig_num)
        : TestEvent(TestEventType_Signal)
        , thread_num(thread_num)
        , sig_num(sig_num)
    {
    }
};

class TestEventThreadExit : public TestEvent
{
public:
    TestEventThreadNum_e thread_num;

    explicit TestEventThreadExit(const TestEventThreadNum_e thread_num)
        : TestEvent(TestEventType_ThreadExit)
        , thread_num(thread_num)
    {
    }
};

#endif // TEST_EVENTS_H
