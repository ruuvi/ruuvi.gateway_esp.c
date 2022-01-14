#ifndef ESP_TIMER_H
#define ESP_TIMER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_timer *esp_timer_handle_t;

/**
 * @brief Timer callback function type
 * @param arg pointer to opaque user-specific data
 */
typedef void (*esp_timer_cb_t)(void *arg);

/**
 * @brief Method for dispatching timer callback
 */
typedef enum
{
    ESP_TIMER_TASK, //!< Callback is called from timer task

    /* Not supported for now, provision to allow callbacks to run directly
     * from an ISR:

        ESP_TIMER_ISR,      //!< Callback is called from timer ISR

     */
} esp_timer_dispatch_t;

/**
 * @brief Timer configuration passed to esp_timer_create
 */
typedef struct
{
    esp_timer_cb_t       callback;        //!< Function to call when timer expires
    void *               arg;             //!< Argument to pass to the callback
    esp_timer_dispatch_t dispatch_method; //!< Call the callback from task or from ISR
    const char *         name;            //!< Timer name, used in esp_timer_dump function
} esp_timer_create_args_t;

/**
 * @brief Initialize esp_timer library
 *
 * @note This function is called from startup code. Applications do not need
 * to call this function before using other esp_timer APIs.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NO_MEM if allocation has failed
 *      - ESP_ERR_INVALID_STATE if already initialized
 *      - other errors from interrupt allocator
 */
esp_err_t
esp_timer_init();

/**
 * @brief De-initialize esp_timer library
 *
 * @note Normally this function should not be called from applications
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if not yet initialized
 */
esp_err_t
esp_timer_deinit();

/**
 * @brief Create an esp_timer instance
 *
 * @note When done using the timer, delete it with esp_timer_delete function.
 *
 * @param create_args   Pointer to a structure with timer creation arguments.
 *                      Not saved by the library, can be allocated on the stack.
 * @param[out] out_handle  Output, pointer to esp_timer_handle_t variable which
 *                         will hold the created timer handle.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if some of the create_args are not valid
 *      - ESP_ERR_INVALID_STATE if esp_timer library is not initialized yet
 *      - ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t
esp_timer_create(const esp_timer_create_args_t *create_args, esp_timer_handle_t *out_handle);

/**
 * @brief Start a periodic timer
 *
 * Timer should not be running when this function is called. This function will
 * start the timer which will trigger every 'period' microseconds.
 *
 * @param timer timer handle created using esp_timer_create
 * @param period timer period, in microseconds
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if the handle is invalid
 *      - ESP_ERR_INVALID_STATE if the timer is already running
 */
esp_err_t
esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period);

/**
 * @brief Stop the timer
 *
 * This function stops the timer previously started using esp_timer_start_once
 * or esp_timer_start_periodic.
 *
 * @param timer timer handle created using esp_timer_create
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if the timer is not running
 */
esp_err_t
esp_timer_stop(esp_timer_handle_t timer);

int64_t
esp_timer_get_time();

#ifdef __cplusplus
}
#endif

#endif // ESP_TIMER_H
