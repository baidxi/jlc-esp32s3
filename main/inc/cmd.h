#pragma once

#include "shell.h"

/* 命令函数声明 */
int cmd_help(shell_context_t *shell, int argc, char **argv);
int cmd_top(shell_context_t *shell, int argc, char **argv);
int cmd_clear(shell_context_t *shell, int argc, char **argv);
int cmd_reboot(shell_context_t *shell, int argc, char **argv);
int cmd_ls(shell_context_t *shell, int argc, char **argv);

/* 命令初始化函数声明 */
void cmd_help_init(void);
void cmd_top_init(void);
void cmd_clear_init(void);
void cmd_reboot_init(void);
void cmd_ls_init(void);