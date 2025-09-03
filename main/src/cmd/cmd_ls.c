#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "shell_platform.h"
#include "cmd.h"
#include "fs/fs.h"

/**
 * @brief ls命令处理函数
 *
 * @param argc 参数个数
 * @param argv 参数数组
 * @return int 执行结果
 */
int cmd_ls(shell_context_t *shell, int argc, char **argv)
{
    char path[128] = {0};
    char buf[512] = {0};
    struct sysfs_node *node;
    int ret;
    
    /* 如果没有指定路径，默认为根目录 */
    if (argc < 2) {
        shell_strncpy(path, "/", sizeof(path));
    } else {
        shell_strncpy(path, argv[1], sizeof(path));
    }
    
    /* 查找节点 */
    node = sysfs_find_node(path);
    if (!node) {
        shell_printf(shell, "Path '%s' not found\r\n", path);
        return -1;
    }
    
    /* 如果是目录，列出子节点 */
    if (node->type == SYSFS_NODE_DIR) {
        ret = sysfs_list_dir(node, buf, sizeof(buf));
        if (ret > 0) {
            // shell_printf(shell, "Contents of %s:\r\n", path);
            shell_printf(shell, "%s", buf);
        } else {
            shell_printf(shell, "Directory %s is empty\r\n", path);
        }
    } else {
        /* 如果是文件，读取内容 */
        ret = sysfs_read_node(node, buf, sizeof(buf));
        if (ret > 0) {
            // shell_printf(shell, "Contents of %s:\r\n", path);
            shell_printf(shell, "%s", buf);
        } else {
            shell_printf(shell, "Failed to read file %s\r\n", path);
        }
    }
    
    return 0;
}

/**
 * @brief 注册ls命令
 */
void cmd_ls_init(void)
{
    /* 定义ls命令 */
    static struct shell_command ls_cmd = {
        .name = "ls",
        .help = "List directory contents",
        .func = cmd_ls,
    };
    
    /* 注册命令 */
    shell_register_command(&ls_cmd);
}