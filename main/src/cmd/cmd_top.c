#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "shell_platform.h"
#include "cmd.h"

/**
 * @brief top命令处理函数
 * 
 * @param argc 参数个数
 * @param argv 参数数组
 * @return int 执行结果
 */
int cmd_top(shell_context_t *shell, int argc, char **argv)
{
    struct shell_memory_info mem_info;
    struct shell_task_info task_info;
    uint32_t task_num;
    char task_state[16];
    
    /* 获取内存信息 */
    if (shell_memory_get_info(&mem_info) != 0) {
        shell_printf(shell, "Failed to get memory information\r\n");
        return -1;
    }
    
    /* 获取任务数量 */
    task_num = shell_task_get_count();
    
    /* 打印内存信息 */
    shell_printf(shell, "\r\nMemory Usage:\r\n");
    shell_printf(shell, "  Total Heap: %d bytes\r\n", mem_info.total_heap);
    shell_printf(shell, "  Free Heap:  %d bytes (%.1f%%)\r\n", mem_info.free_heap,
                 (float)mem_info.free_heap / mem_info.total_heap * 100);
    shell_printf(shell, "  Min Free:   %d bytes\r\n", mem_info.min_free_heap);
    if (mem_info.psram_total > 0) {
        shell_printf(shell, "  PSRAM Total: %d bytes\r\n", mem_info.psram_total);
        shell_printf(shell, "  PSRAM Free:  %d bytes (%.1f%%)\r\n", mem_info.psram_free,
                     (float)mem_info.psram_free / mem_info.psram_total * 100);
    }
    
    /* 打印任务信息 */
    shell_printf(shell, "\r\nTask List (%d tasks):\r\n", task_num);
    shell_printf(shell, "  %-16s %-8s %-8s %-8s\r\n",
                 "Name", "State", "Prio", "Stack");
    
    /* 获取主任务信息 */
    if (shell_task_get_info_by_name("main", &task_info) == 0) {
        /* 转换任务状态为字符串 */
        switch (task_info.state) {
            case SHELL_TASK_RUNNING:
                shell_strncpy(task_state, "Running", sizeof(task_state));
                break;
            case SHELL_TASK_READY:
                shell_strncpy(task_state, "Ready", sizeof(task_state));
                break;
            case SHELL_TASK_BLOCKED:
                shell_strncpy(task_state, "Blocked", sizeof(task_state));
                break;
            case SHELL_TASK_SUSPENDED:
                shell_strncpy(task_state, "Suspended", sizeof(task_state));
                break;
            case SHELL_TASK_DELETED:
                shell_strncpy(task_state, "Deleted", sizeof(task_state));
                break;
            default:
                shell_strncpy(task_state, "Unknown", sizeof(task_state));
                break;
        }
        
        /* 打印任务信息 */
        shell_printf(shell, "  %-16s %-8s %-8d %-8d\r\n",
                     task_info.name,
                     task_state,
                     task_info.priority,
                     task_info.stack_watermark);
    } else {
        shell_printf(shell, "  %-16s %-8s %-8s %-8s\r\n",
                     "main", "Unknown", "?", "?");
    }
    
    /* 获取IDLE任务信息 */
    if (shell_task_get_info_by_name("idle", &task_info) == 0) {
        /* 转换任务状态为字符串 */
        switch (task_info.state) {
            case SHELL_TASK_RUNNING:
                shell_strncpy(task_state, "Running", sizeof(task_state));
                break;
            case SHELL_TASK_READY:
                shell_strncpy(task_state, "Ready", sizeof(task_state));
                break;
            case SHELL_TASK_BLOCKED:
                shell_strncpy(task_state, "Blocked", sizeof(task_state));
                break;
            case SHELL_TASK_SUSPENDED:
                shell_strncpy(task_state, "Suspended", sizeof(task_state));
                break;
            case SHELL_TASK_DELETED:
                shell_strncpy(task_state, "Deleted", sizeof(task_state));
                break;
            default:
                shell_strncpy(task_state, "Unknown", sizeof(task_state));
                break;
        }
        
        /* 打印任务信息 */
        shell_printf(shell, "  %-16s %-8s %-8d %-8d\r\n",
                     task_info.name,
                     task_state,
                     task_info.priority,
                     task_info.stack_watermark);
    } else {
        shell_printf(shell, "  %-16s %-8s %-8s %-8s\r\n",
                     "idle", "Unknown", "?", "?");
    }
    
    /* 获取定时器服务任务信息 */
    if (shell_task_get_info_by_name("Tmr Svc", &task_info) == 0) {
        /* 转换任务状态为字符串 */
        switch (task_info.state) {
            case SHELL_TASK_RUNNING:
                shell_strncpy(task_state, "Running", sizeof(task_state));
                break;
            case SHELL_TASK_READY:
                shell_strncpy(task_state, "Ready", sizeof(task_state));
                break;
            case SHELL_TASK_BLOCKED:
                shell_strncpy(task_state, "Blocked", sizeof(task_state));
                break;
            case SHELL_TASK_SUSPENDED:
                shell_strncpy(task_state, "Suspended", sizeof(task_state));
                break;
            case SHELL_TASK_DELETED:
                shell_strncpy(task_state, "Deleted", sizeof(task_state));
                break;
            default:
                shell_strncpy(task_state, "Unknown", sizeof(task_state));
                break;
        }
        
        /* 打印任务信息 */
        shell_printf(shell, "  %-16s %-8s %-8d %-8d\r\n",
                     task_info.name,
                     task_state,
                     task_info.priority,
                     task_info.stack_watermark);
    } else {
        shell_printf(shell, "  %-16s %-8s %-8s %-8s\r\n",
                     "Tmr Svc", "Unknown", "?", "?");
    }
    
    return 0;
}

/**
 * @brief 注册top命令
 */
void cmd_top_init(void)
{
    /* 定义top命令 */
    static struct shell_command top_cmd = {
        .name = "top",
        .help = "Show system tasks and memory usage",
        .func = cmd_top,
    };
    
    /* 注册命令 */
    shell_register_command(&top_cmd);
}