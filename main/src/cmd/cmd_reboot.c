#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "shell_platform.h"
#include "cmd.h"

/**
 * @brief reboot命令处理函数
 * 
 * @param argc 参数个数
 * @param argv 参数数组
 * @return int 执行结果
 */
int cmd_reboot(shell_context_t *shell, int argc, char **argv)
{
    shell_printf(shell, "Rebooting system...\r\n");
    
    /* 延迟一段时间后重启 */
    shell_task_delay(1000);
    shell_system_reboot();
    
    return 0;
}

/**
 * @brief 注册reboot命令
 */
void cmd_reboot_init(void)
{
    /* 定义reboot命令 */
    static struct shell_command reboot_cmd = {
        .name = "reboot",
        .help = "Reboot the system",
        .func = cmd_reboot,
    };
    
    /* 注册命令 */
    shell_register_command(&reboot_cmd);
}