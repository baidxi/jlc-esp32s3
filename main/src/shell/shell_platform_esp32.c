#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "soc/uart_channel.h"
#include "shell_platform.h"

/* 定义缺失的常量 */
#ifndef CONFIG_FREERTOS_HZ
#define CONFIG_FREERTOS_HZ 1000
#endif

#ifndef portTICK_PERIOD_MS
#define portTICK_PERIOD_MS (1000 / CONFIG_FREERTOS_HZ)
#endif

/* 定义缺失的日志级别常量 */
#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL 3
#endif

static const char *TAG = "shell_platform";

/* 平台抽象层接口实例 */
static struct shell_platform_ops platform_ops;

/**
 * @brief ESP32互斥锁创建
 */
static shell_mutex_t esp32_mutex_create(void)
{
    return xSemaphoreCreateMutex();
}

/**
 * @brief ESP32互斥锁获取
 */
static int esp32_mutex_take(shell_mutex_t mutex, int timeout)
{
    if (timeout == SHELL_WAIT_FOREVER) {
        return xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) == pdTRUE ? 0 : -1;
    } else {
        return xSemaphoreTake((SemaphoreHandle_t)mutex, timeout / portTICK_PERIOD_MS) == pdTRUE ? 0 : -1;
    }
}

/**
 * @brief ESP32互斥锁释放
 */
static int esp32_mutex_give(shell_mutex_t mutex)
{
    return xSemaphoreGive((SemaphoreHandle_t)mutex) == pdTRUE ? 0 : -1;
}

/**
 * @brief ESP32互斥锁删除
 */
static void esp32_mutex_delete(shell_mutex_t mutex)
{
    vSemaphoreDelete((SemaphoreHandle_t)mutex);
}

/**
 * @brief ESP32任务创建
 */
static int esp32_task_create(const char *name, void (*func)(void*), void *arg, 
                           uint32_t stack_size, uint32_t priority, shell_task_t *task)
{
    return xTaskCreate(func, name, stack_size, arg, priority, (TaskHandle_t*)task) == pdPASS ? 0 : -1;
}

/**
 * @brief ESP32任务删除
 */
static void esp32_task_delete(shell_task_t task)
{
    vTaskDelete((TaskHandle_t)task);
}

/**
 * @brief ESP32任务延时
 */
static void esp32_task_delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

/**
 * @brief ESP32获取任务数量
 */
static uint32_t esp32_task_get_count(void)
{
    return uxTaskGetNumberOfTasks();
}

/**
 * @brief ESP32任务状态转换
 */
static shell_task_state_t esp32_task_state_convert(eTaskState state)
{
    switch (state) {
        case eRunning:
            return SHELL_TASK_RUNNING;
        case eReady:
            return SHELL_TASK_READY;
        case eBlocked:
            return SHELL_TASK_BLOCKED;
        case eSuspended:
            return SHELL_TASK_SUSPENDED;
        case eDeleted:
            return SHELL_TASK_DELETED;
        default:
            return SHELL_TASK_UNKNOWN;
    }
}

/**
 * @brief ESP32获取任务信息
 */
static int esp32_task_get_info(uint32_t index, struct shell_task_info *info)
{
    /* ESP-IDF不提供按索引获取任务信息的API，这里简化实现 */
    if (!info) {
        return -1;
    }
    
    /* 由于ESP-IDF不提供按索引获取任务信息的API，我们返回错误 */
    return -1;
}

/**
 * @brief ESP32获取指定名称的任务信息
 */
static int esp32_task_get_info_by_name(const char *name, struct shell_task_info *info)
{
    TaskHandle_t task_handle;
    eTaskState state;
    
    if (!name || !info) {
        return -1;
    }
    
    task_handle = xTaskGetHandle(name);
    if (!task_handle) {
        return -1;
    }
    
    /* 获取任务状态 */
    state = eTaskGetState(task_handle);
    info->state = esp32_task_state_convert(state);
    
    /* 获取任务优先级和堆栈信息 */
    info->priority = uxTaskPriorityGet(task_handle);
    info->stack_watermark = uxTaskGetStackHighWaterMark(task_handle);
    
    /* 复制任务名称 */
    strncpy(info->name, name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    
    return 0;
}

/**
 * @brief ESP32获取内存信息
 */
static int esp32_memory_get_info(struct shell_memory_info *info)
{
    if (!info) {
        return -1;
    }
    
    /* 获取内存信息 */
    info->total_heap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    info->free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    info->min_free_heap = esp_get_minimum_free_heap_size();
    info->psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    info->psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    return 0;
}

/**
 * @brief ESP32系统重启
 */
static void esp32_system_reboot(void)
{
    esp_restart();
}

/**
 * @brief ESP32日志输出
 */
static void esp32_log_print(int level, const char *tag, const char *format, ...)
{
    va_list args;
    char buffer[512];
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    switch (level) {
        case SHELL_LOG_ERROR:
            printf("E (%s) %s\n", tag, buffer);
            break;
        case SHELL_LOG_WARN:
            printf("W (%s) %s\n", tag, buffer);
            break;
        case SHELL_LOG_INFO:
            printf("I (%s) %s\n", tag, buffer);
            break;
        case SHELL_LOG_DEBUG:
            printf("D (%s) %s\n", tag, buffer);
            break;
        case SHELL_LOG_VERBOSE:
            printf("V (%s) %s\n", tag, buffer);
            break;
        default:
            printf("I (%s) %s\n", tag, buffer);
            break;
    }
    
    /* 确保日志立即输出 */
    fflush(stdout);
}

/**
 * @brief ESP32内存分配
 */
static void* esp32_malloc(size_t size)
{
    return malloc(size);
}

/**
 * @brief ESP32内存释放
 */
static void esp32_free(void *ptr)
{
    free(ptr);
}

/**
 * @brief ESP32内存设置
 */
static void* esp32_memset(void *ptr, int value, size_t size)
{
    return memset(ptr, value, size);
}

/**
 * @brief ESP32字符串复制
 */
static char* esp32_strncpy(char *dest, const char *src, size_t size)
{
    return strncpy(dest, src, size);
}

/**
 * @brief ESP32字符串比较
 */
static int esp32_strcmp(const char *s1, const char *s2)
{
    return strcmp(s1, s2);
}

/**
 * @brief ESP32字符串比较（指定长度）
 */
static int esp32_strncmp(const char *s1, const char *s2, size_t n)
{
    return strncmp(s1, s2, n);
}

/**
 * @brief ESP32字符串长度
 */
static size_t esp32_strlen(const char *s)
{
    return strlen(s);
}

/**
 * @brief ESP32字符串分割
 */
static char* esp32_strtok_r(char *str, const char *delim, char **saveptr)
{
    return strtok_r(str, delim, saveptr);
}

/**
 * @brief ESP32格式化字符串
 */
static int esp32_vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    return vsnprintf(str, size, format, args);
}

/**
 * @brief ESP32格式化字符串
 */
static int esp32_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    int ret;
    
    va_start(args, format);
    ret = vsnprintf(str, size, format, args);
    va_end(args);
    
    return ret;
}

/**
 * @brief ESP32早期日志输出（在TTY初始化之前使用）
 */
static void esp32_early_printf(const char *format, ...)
{
    va_list args;
    char buf[256];
    int len;
    
    va_start(args, format);
    len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    /* 直接使用UART0输出，不经过TTY层 */
    uart_write_bytes(UART_NUM_0, buf, len);
    uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(100));
}

/**
 * @brief ESP32早期日志输出（在TTY初始化之前使用）
 */
static void esp32_early_printf_v(const char *format, va_list args)
{
    char buf[256];
    int len;
    
    len = vsnprintf(buf, sizeof(buf), format, args);
    
    /* 直接使用UART0输出，不经过TTY层 */
    uart_write_bytes(UART_NUM_0, buf, len);
    uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(100));
}

/**
 * @brief 初始化ESP32平台抽象层
 */
int shell_platform_esp32_init(void)
{
    /* 设置平台抽象层接口 */
    platform_ops.mutex_create = esp32_mutex_create;
    platform_ops.mutex_take = esp32_mutex_take;
    platform_ops.mutex_give = esp32_mutex_give;
    platform_ops.mutex_delete = esp32_mutex_delete;
    platform_ops.task_create = esp32_task_create;
    platform_ops.task_delete = esp32_task_delete;
    platform_ops.task_delay = esp32_task_delay;
    platform_ops.task_get_count = esp32_task_get_count;
    platform_ops.task_get_info = esp32_task_get_info;
    platform_ops.task_get_info_by_name = esp32_task_get_info_by_name;
    platform_ops.memory_get_info = esp32_memory_get_info;
    platform_ops.system_reboot = esp32_system_reboot;
    platform_ops.log_print = esp32_log_print;
    platform_ops.malloc = esp32_malloc;
    platform_ops.free = esp32_free;
    platform_ops.memset = esp32_memset;
    platform_ops.strncpy = esp32_strncpy;
    platform_ops.strcmp = esp32_strcmp;
    platform_ops.strncmp = esp32_strncmp;
    platform_ops.strlen = esp32_strlen;
    platform_ops.strtok_r = esp32_strtok_r;
    platform_ops.vsnprintf = esp32_vsnprintf;
    platform_ops.snprintf = esp32_snprintf;
    platform_ops.early_printf = esp32_early_printf_v;
    
    /* 注册平台抽象层接口 */
    return shell_platform_set_ops(&platform_ops);
}