#ifndef ESP_LOG_H
#define ESP_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log level
 *
 */
typedef enum
{
    ESP_LOG_NONE,   /*!< No log output */
    ESP_LOG_ERROR,  /*!< Critical errors, software module can not recover on its
                       own */
    ESP_LOG_WARN,   /*!< Error conditions from which recovery measures have been
                       taken */
    ESP_LOG_INFO,   /*!< Information messages which describe normal flow of events
                     */
    ESP_LOG_DEBUG,  /*!< Extra information which is not necessary for normal use
                       (values, pointers, sizes, etc). */
    ESP_LOG_VERBOSE /*!< Bigger chunks of debugging information, or frequent
                       messages which can potentially flood the
                       output. */
} esp_log_level_t;

#define ESP_LOGI(tag, fmt, ...)
#define ESP_LOGE(tag, fmt, ...)

#ifdef __cplusplus
}
#endif

#endif // ESP_LOG_H
