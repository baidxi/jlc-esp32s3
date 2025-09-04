#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "shell_platform.h"
#include "cmd.h"
#include "fs/vfs.h"

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
    struct vfs_node *node;
    int fd, ret;
    char buf[512] = {0};
    struct dirent dirent;
    
    /* 如果没有指定路径，默认为根目录 */
    if (argc < 2) {
        shell_strncpy(path, "/", sizeof(path));
    } else {
        shell_strncpy(path, argv[1], sizeof(path));
    }
    
    /* 查找节点 */
    ret = vfs_lookup(path, &node);
    if (ret != 0) {
        shell_printf(shell, "Path '%s' not found\r\n", path);
        return -1;
    }
    
    /* 如果是目录，列出子节点 */
    if (node->type == VFS_NODE_TYPE_DIR) {
        struct file *dir_file;
        
        /* 打开目录 */
        ret = vfs_open(path, O_RDONLY, 0, &dir_file);
        if (ret != 0) {
            shell_printf(shell, "Failed to open directory %s\r\n", path);
            return -1;
        }
        
        shell_printf(shell, "Contents of %s:\r\n", path);
        
        /* 读取目录内容 */
        while (vfs_readdir(dir_file, &dirent) == 0) {
            shell_printf(shell, "%s\r\n", dirent.d_name);
        }
        
        /* 关闭目录 */
        vfs_close(dir_file);
    } else {
        /* 如果是文件，读取内容 */
        struct file *file;
        
        /* 打开文件 */
        ret = vfs_open(path, O_RDONLY, 0, &file);
        if (ret != 0) {
            shell_printf(shell, "Failed to open file %s\r\n", path);
            return -1;
        }
        
        shell_printf(shell, "Contents of %s:\r\n", path);
        
        /* 读取文件内容 */
        ret = vfs_read(file, buf, sizeof(buf) - 1);
        if (ret > 0) {
            buf[ret] = '\0';
            shell_printf(shell, "%s", buf);
        } else {
            shell_printf(shell, "Failed to read file %s\r\n", path);
        }
        
        /* 关闭文件 */
        vfs_close(file);
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