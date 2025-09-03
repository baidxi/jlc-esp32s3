#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "core.h"
#include "shell.h"
#include "shell_platform.h"
#include "tty/tty.h"
#include "esp_log.h"

/**
 * @brief Shell上下文结构体
 */
struct shell_context {
    struct tty_device *tty;       /* TTY设备 */
    char rx_buffer[256];         /* 接收缓冲区 */
    int rx_buffer_len;            /* 接收缓冲区长度 */
    char prompt[32];             /* 提示符 */
    bool running;                /* 运行状态 */
    shell_task_t task;           /* Shell任务句柄 */
    struct list_head commands;    /* 命令链表 */
    int (*output)(struct shell_context *shell, const char *format, ...); /* 输出函数指针 */
    
    /* 历史命令相关 */
    char history[16][256];       /* 历史命令缓冲区 */
    int history_count;           /* 历史命令数量 */
    int history_index;           /* 当前历史命令索引 */
    char current_cmd[256];       /* 当前正在编辑的命令 */
    int current_cmd_len;         /* 当前正在编辑的命令长度 */
    
    /* 控制台接口 */
    struct console console;      /* 控制台接口 */
};

static const char *TAG = "shell";

/* 最大支持的Shell实例数 */
#define MAX_SHELL_INSTANCES 2

/* Shell实例数组 */
static struct shell_context shell_instances[MAX_SHELL_INSTANCES];
static shell_mutex_t shell_mutex = NULL;

/**
 * @brief Shell输出函数
 */
static int shell_output(struct shell_context *shell, const char *format, ...)
{
    va_list args;
    char buffer[256];
    int len;
    int ret;
    
    if (!shell || !shell->tty || !format) {
        return -1;
    }
    
    /* 格式化字符串 */
    va_start(args, format);
    len = shell_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    
    // shell_log_info(TAG, "Writing to TTY device '%s': %s", shell->tty->name, buffer);
    
    /* 写入TTY设备 */
    ret = shell->tty->ops->write(&shell->tty->dev, buffer, len);
    if (ret < 0) {
        shell_log_error(TAG, "Failed to write to TTY device '%s'", shell->tty->name);
        return -1;
    }
    
    // shell_log_info(TAG, "Wrote %d bytes to TTY device '%s'", ret, shell->tty->name);
    
    return ret;
}

/**
 * @brief 控制台输出函数
 */
static int console_output(struct shell_context *shell, const char *format, ...)
{
    va_list args;
    char buffer[256];
    int len;
    int ret;
    
    if (!shell || !shell->tty || !format) {
        return -1;
    }
    
    /* 格式化字符串 */
    va_start(args, format);
    len = shell_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    
    /* 写入TTY设备 */
    ret = shell->tty->ops->write(&shell->tty->dev, buffer, len);
    if (ret < 0) {
        shell_log_error(TAG, "Failed to write to TTY device '%s'", shell->tty->name);
        return -1;
    }
    
    return ret;
}

/**
 * @brief 控制台输入函数
 */
static int console_input(struct shell_context *shell, char *buffer, int size)
{
    int ret;
    char ch;
    int count = 0;
    
    if (!shell || !shell->tty || !buffer || size <= 0) {
        return -1;
    }
    
    /* 读取字符直到遇到回车或缓冲区满 */
    while (count < size - 1) {
        ret = shell->tty->ops->read(&shell->tty->dev, &ch, 1);
        if (ret <= 0) {
            /* 短暂延时，避免忙等待 */
            shell_task_delay(10);
            continue;
        }
        
        /* 确保读取到的字符是有效的 */
        if (ret != 1) {
            continue;
        }
        
        /* 处理回车键 */
        if (ch == '\r' || ch == '\n') {
            shell->output(shell, "\r\n");
            break;
        }
        
        /* 处理退格键 */
        if (ch == '\b' || ch == 127) {
            if (count > 0) {
                count--;
                buffer[count] = '\0';
                shell->output(shell, "\b \b");
            }
            continue;
        }
        
        /* 可打印字符 */
        if (ch >= 32 && ch < 127) {
            buffer[count++] = ch;
            buffer[count] = '\0';
            shell->output(shell, "%c", ch);
        }
    }
    
    /* 添加字符串结束符 */
    buffer[count] = '\0';
    
    return count;
}

/**
 * @brief Shell任务函数
 *
 * @param arg 参数
 */
/**
 * @brief 添加命令到历史记录
 */
static void shell_add_history(struct shell_context *ctx, const char *cmd)
{
    if (!ctx || !cmd || cmd[0] == '\0') {
        return;
    }
    
    /* 检查是否与上一条命令相同，如果相同则不添加 */
    if (ctx->history_count > 0 && shell_strcmp(ctx->history[ctx->history_count - 1], cmd) == 0) {
        /* 重置历史命令索引 */
        ctx->history_index = ctx->history_count;
        return;
    }
    
    /* 如果历史记录已满，移除最旧的命令 */
    if (ctx->history_count >= sizeof(ctx->history) / sizeof(ctx->history[0])) {
        /* 移动所有历史命令，去掉最旧的一个 */
        for (int i = 0; i < ctx->history_count - 1; i++) {
            shell_strncpy(ctx->history[i], ctx->history[i + 1], sizeof(ctx->history[i]));
        }
        ctx->history_count--;
    }
    
    /* 添加新命令到历史记录 */
    shell_strncpy(ctx->history[ctx->history_count], cmd, sizeof(ctx->history[0]) - 1);
    ctx->history[ctx->history_count][sizeof(ctx->history[0]) - 1] = '\0';
    ctx->history_count++;
    
    /* 重置历史命令索引 */
    ctx->history_index = ctx->history_count;
}

/**
 * @brief 清除当前行显示
 */
static void shell_clear_line(struct shell_context *ctx)
{
    /* 清除当前行 */
    ctx->output(ctx, "\r\033[K");
}

/**
 * @brief 显示当前命令
 */
static void shell_display_command(struct shell_context *ctx, const char *cmd)
{
    /* 显示提示符和命令 */
    ctx->output(ctx, "%s%s", ctx->prompt, cmd);
}

static void shell_task(void *arg)
{
    struct shell_context *ctx = (struct shell_context *)arg;
    char cmd_buffer[256];
    int cmd_buffer_len = 0;
    char *argv[16];
    int argc = 0;
    struct shell_command *cmd;
    char *p;
    bool escape_seq = false;     /* 是否处于转义序列中 */
    int escape_seq_len = 0;      /* 转义序列长度 */
    char escape_seq_buf[8];      /* 转义序列缓冲区 */
    
    shell_log_info(TAG, "Shell task started");
    
    if (!ctx || !ctx->tty) {
        shell_log_error(TAG, "Invalid shell context");
        return;
    }
    
    /* 初始化历史命令相关变量 */
    ctx->history_count = 0;
    ctx->history_index = 0;  /* 初始为0，当有历史命令时会设置为history_count */
    ctx->current_cmd[0] = '\0';
    ctx->current_cmd_len = 0;
    
    /* 打印欢迎信息 */
    shell_log_info(TAG, "Printing welcome message");
    ctx->output(ctx, "\r\n");
    ctx->output(ctx, "ESP32 Shell Terminal\r\n");
    ctx->output(ctx, "Type 'help' for available commands\r\n");
    
    shell_log_info(TAG, "Entering main loop");
    while (ctx->running) {
        /* 打印提示符 */
        // shell_log_info(TAG, "Printing prompt");
        ctx->output(ctx, "%s", ctx->prompt);
        
        /* 读取命令 */
        cmd_buffer_len = 0;
        cmd_buffer[0] = '\0';
        ctx->current_cmd[0] = '\0';
        ctx->current_cmd_len = 0;
        ctx->history_index = ctx->history_count;  /* 设置为最新命令的下一个位置 */
        
        while (ctx->running) {
            char ch;
            int ret;
            
            /* 从TTY设备读取一个字符 */
            ret = ctx->tty->ops->read(&ctx->tty->dev, &ch, 1);
            if (ret <= 0) {
                /* 短暂延时，避免忙等待 */
                shell_task_delay(10);
                continue;
            }
            
            /* 确保读取到的字符是有效的 */
            if (ret != 1) {
                continue;
            }
            
            /* 调试日志：显示所有接收到的字符 */
            /*
            if (ch >= 32 && ch < 127) {
                shell_log_info(TAG, "Received character: '%c' (0x%02x)", ch, ch);
            } else {
                shell_log_info(TAG, "Received character: 0x%02x", ch);
            }
                */
            
            /* 处理转义序列 */
            if (escape_seq) {
                escape_seq_buf[escape_seq_len++] = ch;
                // shell_log_info(TAG, "Escape sequence: len=%d, char=%d (0x%02x)", escape_seq_len, ch, ch);
                
                /* 检查是否是完整的转义序列 */
                if (escape_seq_len == 1) {
                    /* 检查第二个字符是否是'[' */
                    if (ch == '[') {
                        /* 继续等待下一个字符 */
                        // shell_log_info(TAG, "Escape sequence: got [");
                        continue;
                    } else {
                        /* 无效的转义序列，重置状态 */
                        // shell_log_info(TAG, "Escape sequence: invalid second char %d (0x%02x)", ch, ch);
                        escape_seq = false;
                        escape_seq_len = 0;
                        continue;
                    }
                } else if (escape_seq_len == 2) {
                    /* 检查第三个字符是否是方向键 */
                    if (escape_seq_buf[0] == '[') {
                        // shell_log_info(TAG, "Escape sequence: got direction key %c", ch);
                        if (ch == 'A') {
                            /* 上方向键 */
                            // shell_log_info(TAG, "Up arrow pressed, history_count=%d, history_index=%d",
                                        //   ctx->history_count, ctx->history_index);
                            if (ctx->history_count > 0) {
                                /* 保存当前正在编辑的命令 */
                                if (ctx->history_index == ctx->history_count) {
                                    shell_strncpy(ctx->current_cmd, cmd_buffer, sizeof(ctx->current_cmd));
                                    ctx->current_cmd_len = cmd_buffer_len;
                                    // shell_log_info(TAG, "Saved current command: %s", ctx->current_cmd);
                                }
                                
                                /* 移动到上一个历史命令 */
                                if (ctx->history_index > 0) {
                                    ctx->history_index--;
                                }
                                
                                /* 清除当前行并显示历史命令 */
                                shell_clear_line(ctx);
                                shell_strncpy(cmd_buffer, ctx->history[ctx->history_index], sizeof(cmd_buffer));
                                cmd_buffer_len = shell_strlen(cmd_buffer);
                                shell_display_command(ctx, cmd_buffer);
                                // shell_log_info(TAG, "Displaying history command: %s", cmd_buffer);
                            }
                        } else if (ch == 'B') {
                            /* 下方向键 */
                            // shell_log_info(TAG, "Down arrow pressed, history_count=%d, history_index=%d",
                                        //   ctx->history_count, ctx->history_index);
                            if (ctx->history_count > 0) {
                                /* 移动到下一个历史命令 */
                                if (ctx->history_index < ctx->history_count) {
                                    ctx->history_index++;
                                }
                                
                                /* 清除当前行并显示历史命令或当前正在编辑的命令 */
                                shell_clear_line(ctx);
                                if (ctx->history_index == ctx->history_count) {
                                    shell_strncpy(cmd_buffer, ctx->current_cmd, sizeof(cmd_buffer));
                                    cmd_buffer_len = ctx->current_cmd_len;
                                    // shell_log_info(TAG, "Restoring current command: %s", cmd_buffer);
                                } else {
                                    shell_strncpy(cmd_buffer, ctx->history[ctx->history_index], sizeof(cmd_buffer));
                                    cmd_buffer_len = shell_strlen(cmd_buffer);
                                    // shell_log_info(TAG, "Displaying history command: %s", cmd_buffer);
                                }
                                shell_display_command(ctx, cmd_buffer);
                            }
                        } else if (ch == 'C') {
                            /* 右方向键，不处理 */
                            // shell_log_info(TAG, "Right arrow pressed");
                        } else if (ch == 'D') {
                            /* 左方向键，不处理 */
                            // shell_log_info(TAG, "Left arrow pressed");
                        }
                        
                        /* 重置转义序列状态 */
                        escape_seq = false;
                        escape_seq_len = 0;
                        continue;
                    } else {
                        /* 无效的转义序列，重置状态 */
                        // shell_log_info(TAG, "Escape sequence: invalid sequence %d,%d,%d",
                        //               escape_seq_buf[0], escape_seq_buf[1], ch);
                        escape_seq = false;
                        escape_seq_len = 0;
                        continue;
                    }
                } else {
                    /* 无效的转义序列，重置状态 */
                    // shell_log_info(TAG, "Escape sequence: invalid length %d", escape_seq_len);
                    escape_seq = false;
                    escape_seq_len = 0;
                    continue;
                }
            }
            
            /* 处理转义字符 */
            if (ch == 27) {
                // shell_log_info(TAG, "Escape character received (0x1b)");
                escape_seq = true;
                escape_seq_len = 0;
                continue;
            }
            
            /* 处理回车键 */
            if (ch == '\r' || ch == '\n') {
                ctx->output(ctx, "\r\n");
                break;
            }
            
            /* 处理退格键 */
            if (ch == '\b' || ch == 127) {
                if (cmd_buffer_len > 0) {
                    cmd_buffer_len--;
                    cmd_buffer[cmd_buffer_len] = '\0';
                    ctx->output(ctx, "\b \b");
                }
                continue;
            }
            
            /* 处理Ctrl+C */
            if (ch == 3) {
                ctx->output(ctx, "^C\r\n");
                cmd_buffer_len = 0;
                cmd_buffer[0] = '\0';
                break;
            }
            
            /* 可打印字符 */
            if (ch >= 32 && ch < 127) {
                if (cmd_buffer_len < sizeof(cmd_buffer) - 1) {
                    cmd_buffer[cmd_buffer_len++] = ch;
                    cmd_buffer[cmd_buffer_len] = '\0';
                    ctx->output(ctx, "%c", ch);
                }
            }
        }
        
        /* 命令缓冲区为空，继续等待输入 */
        if (cmd_buffer_len == 0) {
            continue;
        }
        
        /* 添加字符串结束符 */
        cmd_buffer[cmd_buffer_len] = '\0';
        
        /* 添加到历史记录 */
        shell_add_history(ctx, cmd_buffer);
        
        /* 解析命令参数 */
        argc = 0;
        p = cmd_buffer;
        while (argc < sizeof(argv) / sizeof(argv[0]) && *p) {
            /* 跳过空格 */
            while (*p == ' ') {
                p++;
            }
            
            if (*p == '\0') {
                break;
            }
            
            /* 保存参数起始位置 */
            argv[argc++] = p;
            
            /* 查找参数结束位置 */
            while (*p && *p != ' ') {
                p++;
            }
            
            /* 如果不是字符串结束，则添加结束符 */
            if (*p) {
                *p++ = '\0';
            }
        }
        
        /* 没有参数，继续等待输入 */
        if (argc == 0) {
            continue;
        }
        
        /* 查找并执行命令 */
        shell_mutex_take(shell_mutex, SHELL_WAIT_FOREVER);
        list_for_each_entry(cmd, &ctx->commands, node) {
            if (shell_strcmp(cmd->name, argv[0]) == 0) {
                /* 找到命令，执行它 */
                shell_mutex_give(shell_mutex);
                cmd->func(ctx, argc, argv);
                break;
            }
        }
        shell_mutex_give(shell_mutex);
        
        /* 未找到命令 */
        if (&cmd->node == &ctx->commands) {
            ctx->output(ctx, "Command '%s' not found. Type 'help' for available commands.\r\n", argv[0]);
        }
    }
    
    shell_log_info(TAG, "Shell task exiting");
    /* 不要删除当前任务，而是让任务自然退出 */
    ctx->running = false;
    ctx->task = NULL;
}

int shell_init(const char *name)
{
    struct tty_device *tty;
    struct shell_context *ctx = NULL;
    int i;
    
    if (!name) {
        shell_log_error(TAG, "Invalid TTY device name");
        return -1;
    }
    
    /* 初始化互斥锁 */
    if (!shell_mutex) {
        shell_mutex = shell_mutex_create();
        if (!shell_mutex) {
            shell_log_error(TAG, "Failed to create shell mutex");
            return -1;
        }
    }
    
    /* 查找TTY设备 */
    tty = tty_find_device(name);
    if (!tty) {
        shell_log_error(TAG, "TTY device '%s' not found", name);
        return -1;
    }
    
    /* 查找可用的Shell实例 */
    shell_mutex_take(shell_mutex, SHELL_WAIT_FOREVER);
    for (i = 0; i < MAX_SHELL_INSTANCES; i++) {
        if (!shell_instances[i].running) {
            ctx = &shell_instances[i];
            break;
        }
    }
    shell_mutex_give(shell_mutex);
    
    if (!ctx) {
        shell_log_error(TAG, "No available shell instance");
        return -1;
    }
    
    /* 初始化Shell上下文 */
    shell_memset(ctx, 0, sizeof(*ctx));
    ctx->tty = tty;
    shell_strncpy(ctx->prompt, "shell> ", sizeof(ctx->prompt) - 1);
    ctx->running = false;
    ctx->output = shell_output;  /* 初始化output函数指针 */
    INIT_LIST_HEAD(&ctx->commands);
    
    /* 初始化控制台接口 */
    ctx->console.output = console_output;
    ctx->console.input = console_input;
    
    /* 初始化历史命令相关变量 */
    ctx->history_count = 0;
    ctx->history_index = 0;  /* 初始为0，当有历史命令时会设置为history_count */
    ctx->current_cmd[0] = '\0';
    ctx->current_cmd_len = 0;
    
    shell_log_info(TAG, "Shell initialized for TTY device '%s'", name);
    return 0;
}

int shell_start(const char *name)
{
    struct shell_context *ctx = NULL;
    int i;
    
    if (!name) {
        shell_log_error(TAG, "Invalid TTY device name");
        return -1;
    }
    
    /* 查找Shell实例 */
    shell_mutex_take(shell_mutex, SHELL_WAIT_FOREVER);
    for (i = 0; i < MAX_SHELL_INSTANCES; i++) {
        if (shell_instances[i].tty && shell_strcmp(shell_instances[i].tty->name, name) == 0) {
            ctx = &shell_instances[i];
            break;
        }
    }
    shell_mutex_give(shell_mutex);
    
    if (!ctx) {
        shell_log_error(TAG, "Shell instance for TTY device '%s' not found", name);
        return -1;
    }
    
    /* 如果Shell已经在运行，则先停止 */
    if (ctx->running) {
        shell_stop(name);
    }
    
    /* 创建Shell任务 */
    ctx->running = true;
    shell_log_info(TAG, "Creating shell task");
    if (shell_task_create("shell", shell_task, ctx, 4096, 5, &ctx->task) != 0) {
        shell_log_error(TAG, "Failed to create shell task");
        ctx->running = false;
        return -1;
    }
    shell_log_info(TAG, "Shell task created successfully");
    
    shell_log_info(TAG, "Shell started for TTY device '%s'", name);
    return 0;
}

int shell_stop(const char *name)
{
    struct shell_context *ctx = NULL;
    int i;
    
    if (!name) {
        shell_log_error(TAG, "Invalid TTY device name");
        return -1;
    }
    
    /* 查找Shell实例 */
    shell_mutex_take(shell_mutex, SHELL_WAIT_FOREVER);
    for (i = 0; i < MAX_SHELL_INSTANCES; i++) {
        if (shell_instances[i].tty && shell_strcmp(shell_instances[i].tty->name, name) == 0) {
            ctx = &shell_instances[i];
            break;
        }
    }
    shell_mutex_give(shell_mutex);
    
    if (!ctx) {
        shell_log_error(TAG, "Shell instance for TTY device '%s' not found", name);
        return -1;
    }
    
    /* 停止Shell任务 */
    if (ctx->running) {
        ctx->running = false;
        if (ctx->task) {
            shell_task_delete(ctx->task);
            ctx->task = NULL;
        }
    }
    
    shell_log_info(TAG, "Shell stopped for TTY device '%s'", name);
    return 0;
}

int shell_register_command(struct shell_command *cmd)
{
    struct shell_context *ctx;
    int i;
    
    if (!cmd || !cmd->name || !cmd->func) {
        shell_log_error(TAG, "Invalid shell command");
        return -1;
    }
    
    /* 向所有Shell实例注册命令 */
    shell_mutex_take(shell_mutex, SHELL_WAIT_FOREVER);
    for (i = 0; i < MAX_SHELL_INSTANCES; i++) {
        ctx = &shell_instances[i];
        if (ctx->tty) {
            /* 检查命令是否已经存在 */
            struct shell_command *existing_cmd;
            list_for_each_entry(existing_cmd, &ctx->commands, node) {
                if (shell_strcmp(existing_cmd->name, cmd->name) == 0) {
                    shell_log_warn(TAG, "Command '%s' already registered", cmd->name);
                    continue;
                }
            }
            
            /* 添加命令到链表 */
            list_add_tail(&cmd->node, &ctx->commands);
            shell_log_info(TAG, "Command '%s' registered", cmd->name);
        }
    }
    shell_mutex_give(shell_mutex);
    
    return 0;
}

int shell_unregister_command(struct shell_command *cmd)
{
    struct shell_context *ctx;
    int i;
    
    if (!cmd || !cmd->name) {
        shell_log_error(TAG, "Invalid shell command");
        return -1;
    }
    
    /* 从所有Shell实例注销命令 */
    shell_mutex_take(shell_mutex, SHELL_WAIT_FOREVER);
    for (i = 0; i < MAX_SHELL_INSTANCES; i++) {
        ctx = &shell_instances[i];
        if (ctx->tty) {
            /* 查找并移除命令 */
            struct shell_command *existing_cmd;
            list_for_each_entry(existing_cmd, &ctx->commands, node) {
                if (shell_strcmp(existing_cmd->name, cmd->name) == 0) {
                    list_del(&existing_cmd->node);
                    shell_log_info(TAG, "Command '%s' unregistered", cmd->name);
                    break;
                }
            }
        }
    }
    shell_mutex_give(shell_mutex);
    
    return 0;
}

int shell_printf_by_name(const char *name, const char *format, ...)
{
    struct tty_device *tty;
    va_list args;
    char buffer[256];
    int len;
    int ret;
    
    if (!name || !format) {
        return -1;
    }
    
    /* 查找TTY设备 */
    tty = tty_find_device(name);
    if (!tty) {
        shell_log_error(TAG, "TTY device '%s' not found", name);
        return -1;
    }
    
    /* 格式化字符串 */
    va_start(args, format);
    len = shell_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    
    // shell_log_info(TAG, "Writing to TTY device '%s': %s", name, buffer);
    
    /* 写入TTY设备 */
    ret = tty->ops->write(&tty->dev, buffer, len);
    if (ret < 0) {
        shell_log_error(TAG, "Failed to write to TTY device '%s'", name);
        return -1;
    }
    
    // shell_log_info(TAG, "Wrote %d bytes to TTY device '%s'", ret, name);
    
    return ret;
}


/**
 * @brief 获取控制台接口
 */
struct console *shell_get_console(shell_context_t *shell)
{
    if (!shell) {
        return NULL;
    }
    
    return &((struct shell_context *)shell)->console;
}

/**
 * @brief Shell输出函数
 */
int shell_printf(shell_context_t *shell, const char *format, ...)
{
    va_list args;
    char buffer[256];
    int len;
    int ret;
    
    if (!shell || !format) {
        return -1;
    }
    
    /* 格式化字符串 */
    va_start(args, format);
    len = shell_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    
    /* 写入TTY设备 */
    ret = ((struct shell_context *)shell)->tty->ops->write(&((struct shell_context *)shell)->tty->dev, buffer, len);
    if (ret < 0) {
        shell_log_error(TAG, "Failed to write to TTY device '%s'", ((struct shell_context *)shell)->tty->name);
        return -1;
    }
    
    return ret;
}

/**
 * @brief Shell输入函数
 */
int shell_gets(shell_context_t *shell, char *buffer, int size)
{
    if (!shell || !buffer || size <= 0) {
        return -1;
    }
    
    return console_input((struct shell_context *)shell, buffer, size);
}

struct shell_context *shell_get_context(const char *name)
{
    struct shell_context *ctx = NULL;
    int i;
    
    if (!name) {
        return NULL;
    }
    
    /* 查找Shell实例 */
    shell_mutex_take(shell_mutex, SHELL_WAIT_FOREVER);
    for (i = 0; i < MAX_SHELL_INSTANCES; i++) {
        if (shell_instances[i].tty && shell_strcmp(shell_instances[i].tty->name, name) == 0) {
            ctx = &shell_instances[i];
            break;
        }
    }
    shell_mutex_give(shell_mutex);
    
    return ctx;
}

/**
 * @brief 获取命令链表
 */
struct list_head *shell_get_commands(shell_context_t *shell)
{
    if (!shell) {
        return NULL;
    }
    
    return &((struct shell_context *)shell)->commands;
}