#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "shell_platform.h"
#include "cmd.h"

/**
 * @brief clear命令处理函数
 * 
 * @param argc 参数个数
 * @param argv 参数数组
 * @return int 执行结果
 */
int cmd_clear(shell_context_t *shell, int argc, char **argv)
{
    /* 清屏 */
    shell_printf(shell, "\033[2J\033[H");
    
    return 0;
}

/**
 * @brief 注册clear命令
 */
void cmd_clear_init(void)
{
    /* 定义clear命令 */
    static struct shell_command clear_cmd = {
        .name = "clear",
        .help = "Clear the screen",
        .func = cmd_clear,
    };
    
    /* 注册命令 */
    shell_register_command(&clear_cmd);
}