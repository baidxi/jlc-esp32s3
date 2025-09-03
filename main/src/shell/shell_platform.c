#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "shell_platform.h"

/* 平台抽象层接口指针 */
static const struct shell_platform_ops *platform_ops = NULL;

/**
 * @brief 设置平台抽象层接口
 */
int shell_platform_set_ops(const struct shell_platform_ops *ops)
{
    if (!ops) {
        return -1;
    }
    
    platform_ops = ops;
    return 0;
}

/**
 * @brief 获取平台抽象层接口
 */
const struct shell_platform_ops* shell_platform_get_ops(void)
{
    return platform_ops;
}

/**
 * @brief 早期日志输出（在TTY初始化之前使用）
 */
void shell_early_printf(const char *format, ...)
{
    va_list args;
    
    if (!platform_ops || !platform_ops->early_printf) {
        return;
    }
    
    va_start(args, format);
    platform_ops->early_printf(format, args);
    va_end(args);
}