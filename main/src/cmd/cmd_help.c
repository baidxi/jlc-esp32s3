#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "shell_platform.h"
#include "cmd.h"

/**
 * @brief help命令处理函数
 * 
 * @param argc 参数个数
 * @param argv 参数数组
 * @return int 执行结果
 */
int cmd_help(shell_context_t *shell, int argc, char **argv)
{
    struct shell_command *cmd;
    struct list_head *commands;
    
    /* 获取命令链表 */
    commands = shell_get_commands(shell);
    
    /* 打印帮助信息 */
    shell_printf(shell, "Available commands:\r\n");
    
    /* 遍历所有命令并打印帮助信息 */
    list_for_each_entry(cmd, commands, node) {
        shell_printf(shell, "  %-10s %s\r\n", cmd->name, cmd->help);
    }
    
    return 0;
}

/**
 * @brief 注册help命令
 */
void cmd_help_init(void)
{
    /* 定义help命令 */
    static struct shell_command help_cmd = {
        .name = "help",
        .help = "Show available commands",
        .func = cmd_help,
    };
    
    /* 注册命令 */
    shell_register_command(&help_cmd);
}