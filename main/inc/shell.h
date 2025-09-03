#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "shell_platform.h"
#include "tty/tty.h"

/**
 * @brief Shell上下文不透明指针
 */
typedef struct shell_context shell_context_t;

/**
 * @brief 控制台接口结构体
 */
struct console {
    /**
     * @brief 控制台输出函数
     * @param shell Shell上下文
     * @param format 格式化字符串
     * @param ... 可变参数
     * @return 输出的字符数
     */
    int (*output)(shell_context_t *shell, const char *format, ...);
    
    /**
     * @brief 控制台输入函数
     * @param shell Shell上下文
     * @param buffer 输入缓冲区
     * @param size 缓冲区大小
     * @return 读取的字符数
     */
    int (*input)(shell_context_t *shell, char *buffer, int size);
};

/**
 * @brief Shell命令结构体
 */
struct shell_command {
    const char *name;             /* 命令名称 */
    const char *help;            /* 帮助信息 */
    int (*func)(shell_context_t *shell, int argc, char **argv); /* 命令处理函数 */
    struct list_head node;       /* 链表节点 */
};

/**
 * @brief 初始化Shell终端
 *
 * @param name TTY设备名称
 * @return int 0成功，非0失败
 */
int shell_init(const char *name);

/**
 * @brief 启动Shell终端
 *
 * @param name TTY设备名称
 * @return int 0成功，非0失败
 */
int shell_start(const char *name);

/**
 * @brief 停止Shell终端
 *
 * @param name TTY设备名称
 * @return int 0成功，非0失败
 */
int shell_stop(const char *name);

/**
 * @brief 注册Shell命令
 *
 * @param cmd 命令结构体
 * @return int 0成功，非0失败
 */
int shell_register_command(struct shell_command *cmd);

/**
 * @brief 注销Shell命令
 *
 * @param cmd 命令结构体
 * @return int 0成功，非0失败
 */
int shell_unregister_command(struct shell_command *cmd);

/**
 * @brief 获取Shell上下文
 *
 * @param name TTY设备名称
 * @return shell_context_t* Shell上下文指针
 */
shell_context_t *shell_get_context(const char *name);

/**
 * @brief 获取控制台接口
 *
 * @param shell Shell上下文
 * @return struct console* 控制台接口指针
 */
struct console *shell_get_console(shell_context_t *shell);

/**
 * @brief Shell输出函数
 *
 * @param shell Shell上下文
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return 输出的字符数
 */
int shell_printf(shell_context_t *shell, const char *format, ...);

/**
 * @brief Shell输出函数（通过设备名称）
 *
 * @param name TTY设备名称
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return 输出的字符数
 */
int shell_printf_by_name(const char *name, const char *format, ...);

/**
 * @brief Shell输入函数
 *
 * @param shell Shell上下文
 * @param buffer 输入缓冲区
 * @param size 缓冲区大小
 * @return 读取的字符数
 */
int shell_gets(shell_context_t *shell, char *buffer, int size);

/**
 * @brief 获取Shell命令链表
 *
 * @param shell Shell上下文
 * @return struct list_head* 命令链表指针
 */
struct list_head *shell_get_commands(shell_context_t *shell);

/**
 * @brief 定义Shell命令宏
 */
#define SHELL_COMMAND(name, help, func) \
    static struct shell_command __shell_cmd_##name = { \
        .name = #name, \
        .help = help, \
        .func = func, \
    }; \
    __attribute__((constructor)) static void __shell_cmd_register_##name(void) { \
        shell_register_command(&__shell_cmd_##name); \
    }

/**
 * @brief Shell命令处理函数类型
 */
typedef int (*shell_cmd_func_t)(shell_context_t *shell, int argc, char **argv);