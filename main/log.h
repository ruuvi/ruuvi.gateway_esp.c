/**
 * @file log.h
 * @author TheSomeMan
 * @date 2020-10-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_LOG_H
#define RUUVI_LOG_H

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_ERR(fmt, ...) \
    do \
    { \
        const char *threadName = pcTaskGetTaskName(NULL); \
        if (NULL == threadName) \
        { \
            threadName = "???"; \
        } \
        ESP_LOGE(TAG, "[%s] %s:%d {%s}: " fmt, threadName, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

#define LOG_ERR_ESP(err, fmt, ...) \
    do \
    { \
        const char *threadName = pcTaskGetTaskName(NULL); \
        if (NULL == threadName) \
        { \
            threadName = "???"; \
        } \
        ESP_LOGE( \
            TAG, \
            "[%s] %s:%d {%s}: " fmt ", err=%d (%s)", \
            threadName, \
            __FILE__, \
            __LINE__, \
            __func__, \
            ##__VA_ARGS__, \
            err, \
            esp_err_to_name(err)); \
    } while (0)

#define LOG_DBG(fmt, ...) \
    do \
    { \
        const char *threadName = pcTaskGetTaskName(NULL); \
        if (NULL == threadName) \
        { \
            threadName = "???"; \
        } \
        ESP_LOGD(TAG, "[%s] %s:%d {%s}: " fmt, threadName, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

#define LOG_WARN(fmt, ...) \
    do \
    { \
        const char *threadName = pcTaskGetTaskName(NULL); \
        if (NULL == threadName) \
        { \
            threadName = "???"; \
        } \
        ESP_LOGW(TAG, "[%s] " fmt, threadName, ##__VA_ARGS__); \
    } while (0)

#define LOG_WARN_ESP(err, fmt, ...) \
    do \
    { \
        const char *threadName = pcTaskGetTaskName(NULL); \
        if (NULL == threadName) \
        { \
            threadName = "???"; \
        } \
        ESP_LOGW(TAG, "[%s] " fmt ", err=%d (%s)", threadName, ##__VA_ARGS__, err, esp_err_to_name(err)); \
    } while (0)

#define LOG_INFO(fmt, ...) \
    do \
    { \
        const char *threadName = pcTaskGetTaskName(NULL); \
        if (NULL == threadName) \
        { \
            threadName = "???"; \
        } \
        ESP_LOGI(TAG, "[%s] " fmt, threadName, ##__VA_ARGS__); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif // RUUVI_LOG_H
