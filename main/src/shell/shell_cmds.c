#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "shell_platform.h"
#include "shell_cmds.h"
#include "cmd.h"

static const char *TAG = "shell_cmds";

/**
 * @brief 注册Shell命令
 */
void shell_cmds_init(void)
{
    /* 注册各个命令 */
    cmd_help_init();
    cmd_top_init();
    cmd_clear_init();
    cmd_reboot_init();
}