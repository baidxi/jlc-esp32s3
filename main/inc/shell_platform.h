#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

/**
 * @brief 平台互斥锁类型
 */
typedef void* shell_mutex_t;

/**
 * @brief 平台任务句柄类型
 */
typedef void* shell_task_t;

/**
 * @brief 平台任务状态枚举
 */
typedef enum {
    SHELL_TASK_RUNNING = 0,
    SHELL_TASK_READY,
    SHELL_TASK_BLOCKED,
    SHELL_TASK_SUSPENDED,
    SHELL_TASK_DELETED,
    SHELL_TASK_UNKNOWN
} shell_task_state_t;

/**
 * @brief 平台内存信息结构体
 */
struct shell_memory_info {
    uint32_t total_heap;     /* 总堆内存 */
    uint32_t free_heap;      /* 空闲堆内存 */
    uint32_t min_free_heap;  /* 最小空闲堆内存 */
    uint32_t psram_total;    /* PSRAM总内存 */
    uint32_t psram_free;     /* PSRAM空闲内存 */
};

/**
 * @brief 平台任务信息结构体
 */
struct shell_task_info {
    char name[16];           /* 任务名称 */
    shell_task_state_t state; /* 任务状态 */
    uint32_t priority;       /* 任务优先级 */
    uint32_t stack_watermark;/* 堆栈水印 */
};

/**
 * @brief 平台抽象层接口
 */
struct shell_platform_ops {
    /**
     * @brief 创建互斥锁
     * @return 互斥锁句柄
     */
    shell_mutex_t (*mutex_create)(void);
    
    /**
     * @brief 获取互斥锁
     * @param mutex 互斥锁句柄
     * @param timeout 超时时间（毫秒），-1表示无限等待
     * @return 0成功，非0失败
     */
    int (*mutex_take)(shell_mutex_t mutex, int timeout);
    
    /**
     * @brief 释放互斥锁
     * @param mutex 互斥锁句柄
     * @return 0成功，非0失败
     */
    int (*mutex_give)(shell_mutex_t mutex);
    
    /**
     * @brief 删除互斥锁
     * @param mutex 互斥锁句柄
     */
    void (*mutex_delete)(shell_mutex_t mutex);
    
    /**
     * @brief 创建任务
     * @param name 任务名称
     * @param func 任务函数
     * @param arg 任务参数
     * @param stack_size 堆栈大小
     * @param priority 优先级
     * @param task 任务句柄指针
     * @return 0成功，非0失败
     */
    int (*task_create)(const char *name, void (*func)(void*), void *arg, 
                      uint32_t stack_size, uint32_t priority, shell_task_t *task);
    
    /**
     * @brief 删除任务
     * @param task 任务句柄
     */
    void (*task_delete)(shell_task_t task);
    
    /**
     * @brief 任务延时
     * @param ms 延时时间（毫秒）
     */
    void (*task_delay)(uint32_t ms);
    
    /**
     * @brief 获取任务数量
     * @return 任务数量
     */
    uint32_t (*task_get_count)(void);
    
    /**
     * @brief 获取任务信息
     * @param index 任务索引
     * @param info 任务信息结构体
     * @return 0成功，非0失败
     */
    int (*task_get_info)(uint32_t index, struct shell_task_info *info);
    
    /**
     * @brief 获取指定名称的任务信息
     * @param name 任务名称
     * @param info 任务信息结构体
     * @return 0成功，非0失败
     */
    int (*task_get_info_by_name)(const char *name, struct shell_task_info *info);
    
    /**
     * @brief 获取内存信息
     * @param info 内存信息结构体
     * @return 0成功，非0失败
     */
    int (*memory_get_info)(struct shell_memory_info *info);
    
    /**
     * @brief 系统重启
     */
    void (*system_reboot)(void);
    
    /**
     * @brief 日志输出
     * @param level 日志级别
     * @param tag 日志标签
     * @param format 格式化字符串
     * @param ... 可变参数
     */
    void (*log_print)(int level, const char *tag, const char *format, ...);
    
    /**
     * @brief 内存分配
     * @param size 分配大小
     * @return 分配的内存指针
     */
    void* (*malloc)(size_t size);
    
    /**
     * @brief 内存释放
     * @param ptr 内存指针
     */
    void (*free)(void *ptr);
    
    /**
     * @brief 内存设置
     * @param ptr 内存指针
     * @param value 设置值
     * @param size 设置大小
     * @return 内存指针
     */
    void* (*memset)(void *ptr, int value, size_t size);
    
    /**
     * @brief 字符串复制
     * @param dest 目标字符串
     * @param src 源字符串
     * @param size 复制大小
     * @return 目标字符串指针
     */
    char* (*strncpy)(char *dest, const char *src, size_t size);
    
    /**
     * @brief 字符串比较
     * @param s1 字符串1
     * @param s2 字符串2
     * @return 比较结果
     */
    int (*strcmp)(const char *s1, const char *s2);
    
    /**
     * @brief 字符串比较（指定长度）
     * @param s1 字符串1
     * @param s2 字符串2
     * @param n 比较长度
     * @return 比较结果
     */
    int (*strncmp)(const char *s1, const char *s2, size_t n);
    
    /**
     * @brief 字符串长度
     * @param s 字符串
     * @return 字符串长度
     */
    size_t (*strlen)(const char *s);
    
    /**
     * @brief 字符串分割
     * @param str 字符串
     * @param delim 分隔符
     * @param saveptr 保存指针
     * @return 分割后的字符串
     */
    char* (*strtok_r)(char *str, const char *delim, char **saveptr);
    
    /**
     * @brief 格式化字符串
     * @param str 目标字符串
     * @param size 目标字符串大小
     * @param format 格式化字符串
     * @param args 可变参数列表
     * @return 格式化后的字符串长度
     */
    int (*vsnprintf)(char *str, size_t size, const char *format, va_list args);
    
    /**
     * @brief 格式化字符串
     * @param str 目标字符串
     * @param size 目标字符串大小
     * @param format 格式化字符串
     * @param ... 可变参数
     * @return 格式化后的字符串长度
     */
    int (*snprintf)(char *str, size_t size, const char *format, ...);
    
    /**
     * @brief 早期日志输出（在TTY初始化之前使用）
     * @param format 格式化字符串
     * @param args 可变参数列表
     */
    void (*early_printf)(const char *format, va_list args);
};

/**
 * @brief 早期日志输出（在TTY初始化之前使用）
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void shell_early_printf(const char *format, ...);


/**
 * @brief 设置平台抽象层接口
 * @param ops 平台抽象层接口
 * @return 0成功，非0失败
 */
int shell_platform_set_ops(const struct shell_platform_ops *ops);

/**
 * @brief 获取平台抽象层接口
 * @return 平台抽象层接口
 */
const struct shell_platform_ops* shell_platform_get_ops(void);

/**
 * @brief 日志级别定义
 */
#define SHELL_LOG_ERROR  0
#define SHELL_LOG_WARN   1
#define SHELL_LOG_INFO   2
#define SHELL_LOG_DEBUG  3
#define SHELL_LOG_VERBOSE 4

/**
 * @brief 日志输出宏
 */
#define shell_log_error(tag, format, ...) \
    shell_platform_get_ops()->log_print(SHELL_LOG_ERROR, tag, format, ##__VA_ARGS__)

#define shell_log_warn(tag, format, ...) \
    shell_platform_get_ops()->log_print(SHELL_LOG_WARN, tag, format, ##__VA_ARGS__)

#define shell_log_info(tag, format, ...) \
    shell_platform_get_ops()->log_print(SHELL_LOG_INFO, tag, format, ##__VA_ARGS__)

#define shell_log_debug(tag, format, ...) \
    shell_platform_get_ops()->log_print(SHELL_LOG_DEBUG, tag, format, ##__VA_ARGS__)

#define shell_log_verbose(tag, format, ...) \
    shell_platform_get_ops()->log_print(SHELL_LOG_VERBOSE, tag, format, ##__VA_ARGS__)

/**
 * @brief 内存操作宏
 */
#define shell_malloc(size) \
    shell_platform_get_ops()->malloc(size)

#define shell_free(ptr) \
    shell_platform_get_ops()->free(ptr)

#define shell_memset(ptr, value, size) \
    shell_platform_get_ops()->memset(ptr, value, size)

#define shell_strncpy(dest, src, size) \
    shell_platform_get_ops()->strncpy(dest, src, size)

#define shell_strcmp(s1, s2) \
    shell_platform_get_ops()->strcmp(s1, s2)

#define shell_strncmp(s1, s2, n) \
    shell_platform_get_ops()->strncmp(s1, s2, n)

#define shell_strlen(s) \
    shell_platform_get_ops()->strlen(s)

#define shell_strtok_r(str, delim, saveptr) \
    shell_platform_get_ops()->strtok_r(str, delim, saveptr)

#define shell_vsnprintf(str, size, format, args) \
    shell_platform_get_ops()->vsnprintf(str, size, format, args)

#define shell_snprintf(str, size, format, ...) \
    shell_platform_get_ops()->snprintf(str, size, format, ##__VA_ARGS__)

/**
 * @brief 互斥锁操作宏
 */
#define shell_mutex_create() \
    shell_platform_get_ops()->mutex_create()

#define shell_mutex_take(mutex, timeout) \
    shell_platform_get_ops()->mutex_take(mutex, timeout)

#define shell_mutex_give(mutex) \
    shell_platform_get_ops()->mutex_give(mutex)

#define shell_mutex_delete(mutex) \
    shell_platform_get_ops()->mutex_delete(mutex)

/**
 * @brief 任务操作宏
 */
#define shell_task_create(name, func, arg, stack_size, priority, task) \
    shell_platform_get_ops()->task_create(name, func, arg, stack_size, priority, task)

#define shell_task_delete(task) \
    shell_platform_get_ops()->task_delete(task)

#define shell_task_delay(ms) \
    shell_platform_get_ops()->task_delay(ms)

#define shell_task_get_count() \
    shell_platform_get_ops()->task_get_count()

#define shell_task_get_info(index, info) \
    shell_platform_get_ops()->task_get_info(index, info)

#define shell_task_get_info_by_name(name, info) \
    shell_platform_get_ops()->task_get_info_by_name(name, info)

/**
 * @brief 系统操作宏
 */
#define shell_memory_get_info(info) \
    shell_platform_get_ops()->memory_get_info(info)

#define shell_system_reboot() \
    shell_platform_get_ops()->system_reboot()

/**
 * @brief 无限等待超时定义
 */
#define SHELL_WAIT_FOREVER -1