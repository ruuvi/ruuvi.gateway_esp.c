#ifndef LEDC_H
#define LEDC_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LEDC_HIGH_SPEED_MODE = 0, /*!< LEDC high speed speed_mode */
    LEDC_LOW_SPEED_MODE,      /*!< LEDC low speed speed_mode */
    LEDC_SPEED_MODE_MAX,      /*!< LEDC speed limit */
} ledc_mode_t;

typedef enum
{
    LEDC_CHANNEL_0 = 0, /*!< LEDC channel 0 */
    LEDC_CHANNEL_1,     /*!< LEDC channel 1 */
    LEDC_CHANNEL_2,     /*!< LEDC channel 2 */
    LEDC_CHANNEL_3,     /*!< LEDC channel 3 */
    LEDC_CHANNEL_4,     /*!< LEDC channel 4 */
    LEDC_CHANNEL_5,     /*!< LEDC channel 5 */
    LEDC_CHANNEL_6,     /*!< LEDC channel 6 */
    LEDC_CHANNEL_7,     /*!< LEDC channel 7 */
    LEDC_CHANNEL_MAX,
} ledc_channel_t;

typedef enum
{
    LEDC_INTR_DISABLE = 0, /*!< Disable LEDC interrupt */
    LEDC_INTR_FADE_END,    /*!< Enable LEDC interrupt */
} ledc_intr_type_t;

typedef enum
{
    LEDC_TIMER_0 = 0, /*!< LEDC timer 0 */
    LEDC_TIMER_1,     /*!< LEDC timer 1 */
    LEDC_TIMER_2,     /*!< LEDC timer 2 */
    LEDC_TIMER_3,     /*!< LEDC timer 3 */
    LEDC_TIMER_MAX,
} ledc_timer_t;

/**
 * @brief Configuration parameters of LEDC channel for ledc_channel_config
 * function
 */
typedef struct
{
    int gpio_num;               /*!< the LEDC output gpio_num, if you want to use gpio16,
                                   gpio_num = 16 */
    ledc_mode_t speed_mode;     /*!< LEDC speed speed_mode, high-speed mode or
                                   low-speed mode */
    ledc_channel_t   channel;   /*!< LEDC channel (0 - 7) */
    ledc_intr_type_t intr_type; /*!< configure interrupt, Fade interrupt enable
                                   or Fade interrupt disable */
    ledc_timer_t timer_sel;     /*!< Select the timer source of channel (0 - 3) */
    uint32_t     duty;          /*!< LEDC channel duty, the range of duty setting is [0,
                                   (2**duty_resolution)] */
    int hpoint;                 /*!< LEDC channel hpoint value, the max value is 0xfffff */
} ledc_channel_config_t;

typedef enum
{
    LEDC_FADE_NO_WAIT = 0, /*!< LEDC fade function will return immediately */
    LEDC_FADE_WAIT_DONE,   /*!< LEDC fade function will block until fading to the
                              target duty */
    LEDC_FADE_MAX,
} ledc_fade_mode_t;

typedef enum
{
    LEDC_TIMER_1_BIT = 1, /*!< LEDC PWM duty resolution of  1 bits */
    LEDC_TIMER_2_BIT,     /*!< LEDC PWM duty resolution of  2 bits */
    LEDC_TIMER_3_BIT,     /*!< LEDC PWM duty resolution of  3 bits */
    LEDC_TIMER_4_BIT,     /*!< LEDC PWM duty resolution of  4 bits */
    LEDC_TIMER_5_BIT,     /*!< LEDC PWM duty resolution of  5 bits */
    LEDC_TIMER_6_BIT,     /*!< LEDC PWM duty resolution of  6 bits */
    LEDC_TIMER_7_BIT,     /*!< LEDC PWM duty resolution of  7 bits */
    LEDC_TIMER_8_BIT,     /*!< LEDC PWM duty resolution of  8 bits */
    LEDC_TIMER_9_BIT,     /*!< LEDC PWM duty resolution of  9 bits */
    LEDC_TIMER_10_BIT,    /*!< LEDC PWM duty resolution of 10 bits */
    LEDC_TIMER_11_BIT,    /*!< LEDC PWM duty resolution of 11 bits */
    LEDC_TIMER_12_BIT,    /*!< LEDC PWM duty resolution of 12 bits */
    LEDC_TIMER_13_BIT,    /*!< LEDC PWM duty resolution of 13 bits */
    LEDC_TIMER_14_BIT,    /*!< LEDC PWM duty resolution of 14 bits */
    LEDC_TIMER_15_BIT,    /*!< LEDC PWM duty resolution of 15 bits */
    LEDC_TIMER_16_BIT,    /*!< LEDC PWM duty resolution of 16 bits */
    LEDC_TIMER_17_BIT,    /*!< LEDC PWM duty resolution of 17 bits */
    LEDC_TIMER_18_BIT,    /*!< LEDC PWM duty resolution of 18 bits */
    LEDC_TIMER_19_BIT,    /*!< LEDC PWM duty resolution of 19 bits */
    LEDC_TIMER_20_BIT,    /*!< LEDC PWM duty resolution of 20 bits */
    LEDC_TIMER_BIT_MAX,
} ledc_timer_bit_t;

typedef enum
{
    LEDC_AUTO_CLK,      /*!< The driver will automatically select the source
                           clock(REF_TICK or APB) based on the giving
                           resolution and duty parameter when init the timer*/
    LEDC_USE_REF_TICK,  /*!< LEDC timer select REF_TICK clock as source clock*/
    LEDC_USE_APB_CLK,   /*!< LEDC timer select APB clock as source clock*/
    LEDC_USE_RTC8M_CLK, /*!< LEDC timer select RTC8M_CLK as source clock. Only for
                           low speed channels and this parameter
                           must be the same for all low speed channels*/
} ledc_clk_cfg_t;

/**
 * @brief Configuration parameters of LEDC Timer timer for ledc_timer_config
 * function
 */
typedef struct
{
    ledc_mode_t speed_mode;           /*!< LEDC speed speed_mode, high-speed mode or
                                         low-speed mode */
    ledc_timer_bit_t duty_resolution; /*!< LEDC channel duty resolution */
    ledc_timer_t     timer_num;       /*!< The timer source of channel (0 - 3) */
    uint32_t         freq_hz;         /*!< LEDC timer frequency (Hz) */
    ledc_clk_cfg_t   clk_cfg;         /*!< Configure LEDC source clock.
                                           For low speed channels and high speed channels,
                                         you can specify
                                           the source clock using LEDC_USE_REF_TICK,
                                         LEDC_USE_APB_CLK or LEDC_AUTO_CLK.
                                           For low speed channels, you can also specify the
                                         source clock
                                           using LEDC_USE_RTC8M_CLK, in this case,
                                           all low speed channel's source clock must be
                                         RTC8M_CLK*/
} ledc_timer_config_t;

/**
 * @brief Set LEDC fade function, with a limited time.
 * @note  Call ledc_fade_func_install() once before calling this function.
 *        Call ledc_fade_start() after this to start fading.
 * @note  ledc_set_fade_with_step, ledc_set_fade_with_time and ledc_fade_start
 * are not thread-safe, do not call these
 * functions to control one LEDC channel in different tasks at the same time. A
 * thread-safe version of API is
 * ledc_set_fade_step_and_start
 * @note  If a fade operation is running in progress on that channel, the driver
 * would not allow it to be stopped.
 *        Other duty operations will have to wait until the fade operation has
 * finished.
 * @param speed_mode Select the LEDC speed_mode, high-speed mode and low-speed
 * mode,
 * @param channel LEDC channel index (0-7), select from ledc_channel_t
 * @param target_duty Target duty of fading.( 0 - (2 ** duty_resolution - 1)))
 * @param max_fade_time_ms The maximum time of the fading ( ms ).
 *
 * @return
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_STATE Fade function not installed.
 *     - ESP_FAIL Fade function init error
 */
esp_err_t
ledc_set_fade_with_time(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t target_duty, int max_fade_time_ms);

/**
 * @brief Start LEDC fading.
 * @note  Call ledc_fade_func_install() once before calling this function.
 *        Call this API right after ledc_set_fade_with_time or
 * ledc_set_fade_with_step before to start fading.
 * @note  If a fade operation is running in progress on that channel, the driver
 * would not allow it to be stopped.
 *        Other duty operations will have to wait until the fade operation has
 * finished.
 * @param speed_mode Select the LEDC speed_mode, high-speed mode and low-speed
 * mode
 * @param channel LEDC channel number
 * @param fade_mode Whether to block until fading done.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_STATE Fade function not installed.
 *     - ESP_ERR_INVALID_ARG Parameter error.
 */
esp_err_t
ledc_fade_start(ledc_mode_t speed_mode, ledc_channel_t channel, ledc_fade_mode_t fade_mode);

/**
 * @brief LEDC timer configuration
 *        Configure LEDC timer with the given source
 * timer/frequency(Hz)/duty_resolution
 *
 * @param  timer_conf Pointer of LEDC timer configure struct
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Can not find a proper pre-divider number base on the given
 * frequency and the current duty_resolution.
 */
esp_err_t
ledc_timer_config(const ledc_timer_config_t *timer_conf);

/**
 * @brief LEDC channel configuration
 *        Configure LEDC channel with the given channel/output
 * gpio_num/interrupt/source timer/frequency(Hz)/LEDC duty
 * resolution
 *
 * @param ledc_conf Pointer of LEDC channel configure struct
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t
ledc_channel_config(const ledc_channel_config_t *ledc_conf);

/**
 * @brief Install LEDC fade function. This function will occupy interrupt of
 * LEDC module.
 * @param intr_alloc_flags Flags used to allocate the interrupt. One or multiple
 * (ORred)
 *        ESP_INTR_FLAG_* values. See esp_intr_alloc.h for more info.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_STATE Fade function already installed.
 */
esp_err_t
ledc_fade_func_install(int intr_alloc_flags);

#ifdef __cplusplus
}
#endif

#endif // LEDC_H
