/**
 * @file reset_info.c
 * @author TheSomeMan
 * @date 2022-11-18
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "reset_info.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_system.h>
#include <esp_crc.h>
#include <esp_attr.h>
#include <hal/uart_hal.h>
#include <esp_private/system_internal.h>
#include "freertos/FreeRTOSConfig.h"
#include "str_buf.h"
#include "reset_reason.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define RESET_INFO_DATA_SOFTWARE_MSG_LEN (1024)
#define RESET_INFO_DATA_PANIC_MSG_LEN    (2048)

#define RESET_INFO_SIGNATURE   (0x52555556U)
#define RESET_INFO_VERSION_FMT (1U)

typedef struct reset_info_task_name_t
{
    char buf[configMAX_TASK_NAME_LEN];
} reset_info_task_name_t;

typedef struct reset_info_data_task_watchdog_t
{
    reset_info_task_name_t task_name;
    reset_info_task_name_t active_task;
} reset_info_data_task_watchdog_t;

typedef struct reset_info_data_software_t
{
    char msg[RESET_INFO_DATA_SOFTWARE_MSG_LEN];
} reset_info_data_software_t;

typedef struct reset_info_data_panic_t
{
    char msg[RESET_INFO_DATA_PANIC_MSG_LEN];
} reset_info_data_panic_t;

typedef union reset_info_data_t
{
    reset_info_data_software_t sw;
    reset_info_data_panic_t    panic;
} reset_info_data_t;

typedef enum reset_info_reason_e
{
    RESET_INFO_REASON_SW       = ESP_RST_SW,
    RESET_INFO_REASON_PANIC    = ESP_RST_PANIC,
    RESET_INFO_REASON_TASK_WDT = ESP_RST_TASK_WDT,
    RESET_INFO_REASON_OTHER    = ESP_RST_UNKNOWN,
} reset_info_reason_e;

typedef struct reset_info_t
{
    uint32_t                        signature_begin;
    uint32_t                        version_fmt;
    uint32_t                        len;
    uint32_t                        reset_cnt;
    reset_info_reason_e             reset_reason;
    reset_info_data_task_watchdog_t task_wdt;
    reset_info_data_t               data;
    uint32_t                        signature_end;
    uint32_t                        crc;
} reset_info_t;

/**
 * Declare the symbol pointing to the former implementation of panic_print_char function
 */
extern void
__real_panic_print_char(const char c);

/**
 * Declare the symbol pointing to the former implementation of esp_panic_handler function
 */
extern void
__real_esp_panic_handler(void* p_info);

/**
 * Declare the symbol pointing to the former implementation of esp_restart function
 */
extern void
__real_esp_restart(void);

/**
 * Declare the symbol pointing to the former implementation of panic_restart function
 */
extern void __attribute__((noreturn)) __real_panic_restart(void);

static const char* const TAG = "reset_info";

static RTC_NOINIT_ATTR reset_info_t g_reset_info;
static str_buf_t                    g_reset_info_data_panic_str_buf = STR_BUF_INIT(
    &g_reset_info.data.panic.msg[0],
    sizeof(g_reset_info.data.panic.msg));

static uint32_t
reset_info_calc_crc(const reset_info_t* const p_info)
{
    return esp_crc32_le(0, (const uint8_t*)p_info, offsetof(reset_info_t, crc));
}

static bool
reset_info_is_valid(const reset_info_t* const p_info)
{
    const uint32_t crc = reset_info_calc_crc(p_info);
    if (crc != p_info->crc)
    {
        LOG_WARN("Reset info in RTC RAM is not valid: expected CRC=0x%08x, actual CRC=0x%08x", crc, p_info->crc);
        return false;
    }
    if (RESET_INFO_SIGNATURE != p_info->signature_begin)
    {
        LOG_WARN(
            "Reset info in RTC RAM is not valid: expected signature=0x%08x, actual signature=0x%08x",
            RESET_INFO_SIGNATURE,
            p_info->signature_begin);
        return false;
    }
    if (~RESET_INFO_SIGNATURE != p_info->signature_end)
    {
        LOG_WARN(
            "Reset info in RTC RAM is not valid: expected signature=0x%08x, actual signature=0x%08x",
            ~RESET_INFO_SIGNATURE,
            p_info->signature_end);
        return false;
    }
    if (RESET_INFO_VERSION_FMT != p_info->version_fmt)
    {
        LOG_WARN(
            "Reset info in RTC RAM is not valid: expected version=0x%08x, actual version=0x%08x",
            RESET_INFO_VERSION_FMT,
            p_info->version_fmt);
        return false;
    }
    if (sizeof(*p_info) != p_info->len)
    {
        LOG_WARN(
            "Reset info in RTC RAM is not valid: expected len=0x%08x, actual len=0x%08x",
            sizeof(*p_info),
            p_info->len);
        return false;
    }
    return true;
}

static void
reset_info_clear(void)
{
    reset_info_t* const p_info = &g_reset_info;

    p_info->signature_begin = RESET_INFO_SIGNATURE;
    p_info->version_fmt     = RESET_INFO_VERSION_FMT;
    p_info->len             = sizeof(*p_info);
    p_info->reset_reason    = RESET_INFO_REASON_OTHER;
    p_info->reset_cnt       = 0;
    p_info->signature_end   = ~RESET_INFO_SIGNATURE;
    memset(&p_info->data, 0, sizeof(p_info->data));
    p_info->crc = reset_info_calc_crc(p_info);
}

void
reset_info_clear_extra_info(void)
{
    reset_info_t* const p_info = &g_reset_info;

    memset(&p_info->data, 0, sizeof(p_info->data));
    p_info->crc = reset_info_calc_crc(p_info);
}

void
reset_info_init(void)
{
    reset_info_t* const p_info = &g_reset_info;

    const esp_reset_reason_t reset_reason = esp_reset_reason();
    LOG_INFO("### Reset reason: %s (%d)", reset_reason_to_str(reset_reason), reset_reason);
    if (!reset_info_is_valid(p_info))
    {
        reset_info_clear();
    }

    p_info->reset_cnt += 1;
    p_info->crc = reset_info_calc_crc(p_info);

    switch (p_info->reset_reason)
    {
        case RESET_INFO_REASON_SW:
            LOG_INFO("### Reset by software: %s", p_info->data.sw.msg);
            break;
        case RESET_INFO_REASON_PANIC:
            LOG_INFO("### Reset by panic:\r\n%s", p_info->data.panic.msg);
            break;
        case RESET_INFO_REASON_TASK_WDT:
            LOG_INFO(
                "### Reset by task wdt: %s (active task: %s)",
                p_info->task_wdt.task_name.buf,
                p_info->task_wdt.active_task.buf);
            break;
        case RESET_INFO_REASON_OTHER:
            break;
    }
    LOG_INFO("### Reset cnt: %u", (printf_uint_t)p_info->reset_cnt);
}

void
reset_info_set_sw(const char* const p_msg)
{
    reset_info_t* const p_info      = &g_reset_info;
    p_info->reset_reason            = RESET_INFO_REASON_SW;
    g_reset_info_data_panic_str_buf = str_buf_init(p_info->data.panic.msg, sizeof(p_info->data.panic.msg));
    (void)snprintf(&p_info->data.sw.msg[0], sizeof(p_info->data.sw.msg), "%s", p_msg);
    p_info->crc = esp_crc32_le(0, (const uint8_t*)p_info, offsetof(reset_info_t, crc));
}

static void
reset_info_gen_str_buf(str_buf_t* const p_str_buf, const reset_info_t* const p_info)
{
    switch (p_info->reset_reason)
    {
        case RESET_INFO_REASON_SW:
            str_buf_printf(p_str_buf, "Reset by software: %s", p_info->data.sw.msg);
            break;
        case RESET_INFO_REASON_PANIC:
            str_buf_printf(p_str_buf, "Reset by panic: %s", p_info->data.panic.msg);
            break;
        case RESET_INFO_REASON_TASK_WDT:
            str_buf_printf(
                p_str_buf,
                "Reset by task wdt: %s (active task: %s)",
                p_info->task_wdt.task_name.buf,
                p_info->task_wdt.active_task.buf);
            break;
        case RESET_INFO_REASON_OTHER:
            str_buf_printf(p_str_buf, "%s", "");
            break;
    }
}

str_buf_t
reset_info_get(void)
{
    const reset_info_t* const p_info  = &g_reset_info;
    str_buf_t                 str_buf = STR_BUF_INIT_NULL();

    reset_info_gen_str_buf(&str_buf, p_info);
    if (!str_buf_init_with_alloc(&str_buf))
    {
        return str_buf;
    }
    reset_info_gen_str_buf(&str_buf, p_info);
    return str_buf;
}

uint32_t
reset_info_get_cnt(void)
{
    const reset_info_t* const p_info = &g_reset_info;
    return p_info->reset_cnt;
}

/**
 * @brief panic_print_char_override overrides panic_print_char from ESP-IDF/components/esp_system/panic.c
 * @note GCC linker option "--defsym,panic_print_char=panic_print_char_override" is used for overriding, see
 *  CMakeLists.txt panic_print_char is used in the same module where it is implemented, so, it's impossible to redefine
 *  it with just "-Wl,--wrap=" option. Fortunately, there is a workaround described here:
 *  https://stackoverflow.com/questions/13961774/ \
 *  gnu-gcc-ld-wrapping-a-call-to-symbol-with-caller-and-callee-defined-in-the-sam
 */
void
panic_print_char_override(const char c)
{
    static uart_hal_context_t s_panic_uart = { .dev = CONFIG_ESP_CONSOLE_UART_NUM == 0 ? &UART0 : &UART1 };
    uint32_t                  sz           = 0;
    while (0 == uart_hal_get_txfifo_len(&s_panic_uart))
    {
        // Wait while Tx FIFO buffer is busy
    }
    uart_hal_write_txfifo(&s_panic_uart, (const uint8_t*)&c, 1, &sz);

    str_buf_printf(&g_reset_info_data_panic_str_buf, "%c", c);
}

/**
 * @brief __wrap_esp_panic_handler overrides esp_panic_handler from ESP-IDF/components/esp_system/panic.c
 * @note GCC linker option "--wrap,esp_panic_handler" is used for overriding, see CMakeLists.txt
 */
void
__wrap_esp_panic_handler(void* p_param) // NOSONAR
{

    if (ESP_RST_UNKNOWN == esp_reset_reason_get_hint())
    {
        reset_info_t* const p_info      = &g_reset_info;
        p_info->reset_reason            = RESET_INFO_REASON_PANIC;
        g_reset_info_data_panic_str_buf = str_buf_init(p_info->data.panic.msg, sizeof(p_info->data.panic.msg));
        str_buf_printf(&g_reset_info_data_panic_str_buf, "%s", "");
    }

    __real_esp_panic_handler(p_param); /* Call the former implementation */
}

/**
 * @brief __wrap_panic_restart overrides panic_restart from ESP-IDF/components/esp_system/panic.c
 * @note GCC linker option "--wrap,panic_restart" is used for overriding, see CMakeLists.txt
 */
void __attribute__((noreturn)) __wrap_panic_restart(void) // NOSONAR
{
    reset_info_t* const p_info = &g_reset_info;
    p_info->crc                = esp_crc32_le(0, (const uint8_t*)p_info, offsetof(reset_info_t, crc));
    switch (p_info->reset_reason)
    {
        case RESET_INFO_REASON_SW:
            ESP_EARLY_LOGE(TAG, "__wrap_panic_restart: SW: %s", p_info->data.sw.msg);
            break;
        case RESET_INFO_REASON_PANIC:
            ESP_EARLY_LOGE(TAG, "__wrap_panic_restart: PANIC: %s", p_info->data.panic.msg);
            break;
        case RESET_INFO_REASON_TASK_WDT:
            ESP_EARLY_LOGE(
                TAG,
                "__wrap_panic_restart: TASK_WDT: task_name=%s, active_task=%s",
                p_info->task_wdt.task_name.buf,
                p_info->task_wdt.active_task.buf);
            break;
        case RESET_INFO_REASON_OTHER:
            ESP_EARLY_LOGE(TAG, "__wrap_panic_restart: OTHER");
            break;
    }

    __real_panic_restart();
}

/*
 * This function is called by task_wdt_isr function (ISR for when TWDT times out).
 * Note: It has the same limitations as the interrupt function.
 *       Do not use ESP_LOGI functions inside.
 */
void
esp_task_wdt_isr_user_handler(const char* const p_task_name)
{
    ESP_EARLY_LOGE(TAG, "esp_task_wdt_isr_user_handler: task_name=%s", p_task_name);
    reset_info_t* const p_info = &g_reset_info;
    p_info->reset_reason       = RESET_INFO_REASON_TASK_WDT;
    (void)snprintf(p_info->task_wdt.task_name.buf, sizeof(p_info->task_wdt.task_name.buf), "%s", p_task_name);
    (void)snprintf(
        p_info->task_wdt.active_task.buf,
        sizeof(p_info->task_wdt.active_task.buf),
        "%s",
        pcTaskGetTaskName(xTaskGetCurrentTaskHandleForCPU(0)));

    p_info->crc = reset_info_calc_crc(p_info);
}

/**
 * Redefine esp_restart function to print a message before actually restarting
 */
void
__wrap_esp_restart(void) // NOSONAR
{
    /* Call the former implementation to actually restart the board */
    __real_esp_restart();
}
