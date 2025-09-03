#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "sysfs.h"
#include "bus.h"
#include "device.h"
#include "shell_platform.h"

static const char *TAG = "sysfs";

/* sysfs根节点 */
static struct sysfs_root sysfs_root_node;

/* sysfs互斥锁 */
static shell_mutex_t sysfs_mutex = NULL;

/**
 * @brief 初始化sysfs节点
 */
static void sysfs_node_init(struct sysfs_node *node, const char *name, 
                          struct sysfs_node *parent, enum sysfs_node_type type)
{
    if (!node) {
        return;
    }
    
    node->name = name;
    node->type = type;
    node->parent = parent;
    node->show = NULL;
    node->store = NULL;
    node->priv = NULL;
    
    INIT_LIST_HEAD(&node->children);
    INIT_LIST_HEAD(&node->sibling);
    
    if (parent) {
        list_add_tail(&node->sibling, &parent->children);
    }
}

/**
 * @brief 设备节点显示函数
 */
static int sysfs_device_node_show(struct sysfs_node *node, char *buf, size_t size)
{
    struct sysfs_device_node *dev_node = (struct sysfs_device_node *)node;
    struct device *dev = dev_node->device;
    
    if (!dev || !buf || size == 0) {
        return -1;
    }
    
    return shell_snprintf(buf, size, "Device: %s\nBus: %s\nDriver: %s\n",
                         dev->init_name ? dev->init_name : "unknown",
                         dev->bus ? dev->bus->name : "none",
                         dev->driver ? "loaded" : "none");
}

/**
 * @brief 总线节点显示函数
 */
static int sysfs_bus_node_show(struct sysfs_node *node, char *buf, size_t size)
{
    struct sysfs_bus_node *bus_node = (struct sysfs_bus_node *)node;
    struct bus_type *bus = bus_node->bus;
    
    if (!bus || !buf || size == 0) {
        return -1;
    }
    
    return shell_snprintf(buf, size, "Bus: %s\nDevices: %d\n",
                         bus->name ? bus->name : "unknown", 0);
}

/**
 * @brief 目录节点显示函数
 */
static int sysfs_dir_node_show(struct sysfs_node *node, char *buf, size_t size)
{
    if (!node || !buf || size == 0) {
        return -1;
    }
    
    return shell_snprintf(buf, size, "Directory: %s\n", node->name);
}

/**
 * @brief 初始化sysfs虚拟文件系统
 */
int sysfs_init(void)
{
    shell_log_info(TAG, "Initializing sysfs virtual file system");
    
    /* 初始化互斥锁 */
    if (!sysfs_mutex) {
        sysfs_mutex = shell_mutex_create();
        if (!sysfs_mutex) {
            shell_log_error(TAG, "Failed to create sysfs mutex");
            return -1;
        }
    }
    
    /* 初始化根节点 */
    sysfs_node_init(&sysfs_root_node.node, "", NULL, SYSFS_NODE_DIR);
    sysfs_root_node.node.show = sysfs_dir_node_show;
    INIT_LIST_HEAD(&sysfs_root_node.buses);
    INIT_LIST_HEAD(&sysfs_root_node.devices);
    
    shell_log_info(TAG, "Sysfs virtual file system initialized");
    return 0;
}

/**
 * @brief 创建sysfs节点
 */
struct sysfs_node *sysfs_create_node(const char *name, struct sysfs_node *parent, enum sysfs_node_type type)
{
    struct sysfs_node *node;
    
    if (!name) {
        shell_log_error(TAG, "Invalid node name");
        return NULL;
    }
    
    if (!parent) {
        parent = &sysfs_root_node.node;
    }
    
    /* 分配节点内存 */
    node = shell_malloc(sizeof(struct sysfs_node));
    if (!node) {
        shell_log_error(TAG, "Failed to allocate memory for sysfs node");
        return NULL;
    }
    
    /* 初始化节点 */
    sysfs_node_init(node, name, parent, type);
    
    shell_log_info(TAG, "Created sysfs node '%s'", name);
    return node;
}

/**
 * @brief 删除sysfs节点
 */
void sysfs_remove_node(struct sysfs_node *node)
{
    struct sysfs_node *child, *tmp;
    
    if (!node) {
        return;
    }
    
    shell_mutex_take(sysfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 递归删除子节点 */
    list_for_each_entry_safe(child, tmp, &node->children, sibling) {
        sysfs_remove_node(child);
    }
    
    /* 从父节点的子节点链表中移除 */
    if (node->parent) {
        list_del(&node->sibling);
    }
    
    shell_mutex_give(sysfs_mutex);
    
    /* 释放节点内存 */
    shell_free(node);
    
    shell_log_info(TAG, "Removed sysfs node '%s'", node->name);
}

/**
 * @brief 查找sysfs节点
 */
struct sysfs_node *sysfs_find_node(const char *path)
{
    struct sysfs_node *node;
    char *path_copy, *token, *saveptr;
    const char *delim = "/";
    
    if (!path) {
        return NULL;
    }
    
    /* 复制路径字符串 */
    path_copy = shell_malloc(shell_strlen(path) + 1);
    if (!path_copy) {
        return NULL;
    }
    shell_strncpy(path_copy, path, shell_strlen(path) + 1);
    
    /* 从根节点开始查找 */
    node = &sysfs_root_node.node;
    
    /* 如果路径是根目录，直接返回根节点 */
    if (shell_strcmp(path, "/") == 0) {
        shell_free(path_copy);
        return node;
    }
    
    /* 解析路径 */
    token = shell_strtok_r(path_copy, delim, &saveptr);
    while (token) {
        struct sysfs_node *child;
        bool found = false;
        
        /* 跳过空标记（由前导/或连续/产生） */
        if (shell_strlen(token) == 0) {
            token = shell_strtok_r(NULL, delim, &saveptr);
            continue;
        }
        
        /* 在当前节点的子节点中查找 */
        list_for_each_entry(child, &node->children, sibling) {
            if (shell_strcmp(child->name, token) == 0) {
                node = child;
                found = true;
                break;
            }
        }
        
        if (!found) {
            shell_free(path_copy);
            return NULL;
        }
        
        token = shell_strtok_r(NULL, delim, &saveptr);
    }
    
    shell_free(path_copy);
    return node;
}

/**
 * @brief 读取sysfs节点内容
 */
int sysfs_read_node(struct sysfs_node *node, char *buf, size_t size)
{
    if (!node || !buf || size == 0) {
        return -1;
    }
    
    if (node->show) {
        return node->show(node, buf, size);
    }
    
    return shell_snprintf(buf, size, "%s\n", node->name);
}

/**
 * @brief 写入sysfs节点内容
 */
int sysfs_write_node(struct sysfs_node *node, const char *buf, size_t size)
{
    if (!node || !buf || size == 0) {
        return -1;
    }
    
    if (node->store) {
        return node->store(node, buf, size);
    }
    
    return -1;  /* 默认不支持写入 */
}

/**
 * @brief 列出sysfs目录下的子节点
 */
int sysfs_list_dir(struct sysfs_node *node, char *buf, size_t size)
{
    struct sysfs_node *child;
    int len = 0;
    
    if (!node || !buf || size == 0) {
        return -1;
    }
    
    if (node->type != SYSFS_NODE_DIR) {
        return -1;
    }
    
    shell_mutex_take(sysfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 确保缓冲区以空字符开头 */
    buf[0] = '\0';
    
    /* 遍历子节点 */
    list_for_each_entry(child, &node->children, sibling) {
        int ret;
        
        /* 确保至少有一个字节的空间用于空字符 */
        if (len >= size - 1) {
            break;
        }
        
        /* 检查链表节点是否有效 */
        if (!child->name) {
            continue;
        }
        
        ret = shell_snprintf(buf + len, size - len, "%s\n", child->name);
        if (ret < 0) {
            shell_mutex_give(sysfs_mutex);
            return -1;
        }
        
        len += ret;
    }
    
    /* 确保缓冲区以空字符结尾 */
    if (size > 0) {
        buf[len < size ? len : size - 1] = '\0';
    }
    
    shell_mutex_give(sysfs_mutex);
    return len;
}

/**
 * @brief 注册设备到sysfs
 */
int sysfs_register_device(struct device *device)
{
    struct sysfs_device_node *dev_node;
    struct sysfs_node *device_dir;
    struct sysfs_node *bus_device_dir;
    
    if (!device) {
        return -1;
    }
    
    shell_mutex_take(sysfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 查找或创建设备目录节点 */
    device_dir = NULL;
    struct sysfs_node *child_node;
    list_for_each_entry(child_node, &sysfs_root_node.node.children, sibling) {
        if (shell_strcmp(child_node->name, "devices") == 0 &&
            child_node->type == SYSFS_NODE_DIR) {
            device_dir = child_node;
            break;
        }
    }
    
    if (!device_dir) {
        device_dir = sysfs_create_node("devices", &sysfs_root_node.node, SYSFS_NODE_DIR);
        if (!device_dir) {
            shell_mutex_give(sysfs_mutex);
            return -1;
        }
    }
    
    /* 创建设备节点 */
    dev_node = (struct sysfs_device_node *)shell_malloc(sizeof(struct sysfs_device_node));
    if (!dev_node) {
        shell_mutex_give(sysfs_mutex);
        return -1;
    }
    
    /* 初始化设备节点 */
    sysfs_node_init(&dev_node->node, device->init_name, device_dir, SYSFS_NODE_FILE);
    dev_node->node.show = sysfs_device_node_show;
    dev_node->device = device;
    
    /* 如果设备有关联的总线，在总线目录下也创建设备节点 */
    if (device->bus) {
        struct sysfs_bus_node *bus_node;
        
        /* 查找总线节点 */
        list_for_each_entry(bus_node, &sysfs_root_node.buses, node.sibling) {
            if (bus_node->bus == device->bus) {
                /* 在总线目录下创建设备目录 */
                bus_device_dir = sysfs_create_node("devices", &bus_node->node, SYSFS_NODE_DIR);
                if (bus_device_dir) {
                    /* 创建设备节点 */
                    struct sysfs_device_node *bus_dev_node;
                    
                    bus_dev_node = (struct sysfs_device_node *)shell_malloc(sizeof(struct sysfs_device_node));
                    if (bus_dev_node) {
                        sysfs_node_init(&bus_dev_node->node, device->init_name, bus_device_dir, SYSFS_NODE_FILE);
                        bus_dev_node->node.show = sysfs_device_node_show;
                        bus_dev_node->device = device;
                        /* 添加到总线节点的设备链表 */
                        list_add_tail(&bus_dev_node->node.sibling, &bus_node->devices);
                    }
                }
                break;
            }
        }
    }
    
    shell_mutex_give(sysfs_mutex);
    
    shell_log_info(TAG, "Registered device '%s' to sysfs", device->init_name);
    return 0;
}

/**
 * @brief 从sysfs注销设备
 */
void sysfs_unregister_device(struct device *device)
{
    struct sysfs_node *device_dir;
    struct sysfs_node *child_node;
    
    if (!device) {
        return;
    }
    
    shell_mutex_take(sysfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 查找设备目录节点 */
    device_dir = NULL;
    list_for_each_entry(child_node, &sysfs_root_node.node.children, sibling) {
        if (shell_strcmp(child_node->name, "devices") == 0 &&
            child_node->type == SYSFS_NODE_DIR) {
            device_dir = child_node;
            break;
        }
    }
    
    if (device_dir) {
        /* 在设备目录下查找并删除设备节点 */
        list_for_each_entry(child_node, &device_dir->children, sibling) {
            if (child_node->type == SYSFS_NODE_FILE) {
                struct sysfs_device_node *dev_node = (struct sysfs_device_node *)child_node;
                if (dev_node->device == device) {
                    sysfs_remove_node(&dev_node->node);
                    shell_free(dev_node);
                    break;
                }
            }
        }
    }
    
    shell_mutex_give(sysfs_mutex);
    
    shell_log_info(TAG, "Unregistered device '%s' from sysfs", device->init_name);
}

/**
 * @brief 注册总线到sysfs
 */
int sysfs_register_bus(struct bus_type *bus)
{
    struct sysfs_bus_node *bus_node;
    struct sysfs_node *bus_dir;
    
    if (!bus) {
        return -1;
    }
    
    shell_mutex_take(sysfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 查找或创建总线目录节点 */
    bus_dir = NULL;
    struct sysfs_node *child_node;
    list_for_each_entry(child_node, &sysfs_root_node.node.children, sibling) {
        if (shell_strcmp(child_node->name, "bus") == 0 &&
            child_node->type == SYSFS_NODE_DIR) {
            bus_dir = child_node;
            break;
        }
    }
    
    if (!bus_dir) {
        bus_dir = sysfs_create_node("bus", &sysfs_root_node.node, SYSFS_NODE_DIR);
        if (!bus_dir) {
            shell_mutex_give(sysfs_mutex);
            return -1;
        }
    }
    
    /* 创建总线节点 */
    bus_node = (struct sysfs_bus_node *)shell_malloc(sizeof(struct sysfs_bus_node));
    if (!bus_node) {
        shell_mutex_give(sysfs_mutex);
        return -1;
    }
    
    /* 初始化总线节点 */
    sysfs_node_init(&bus_node->node, bus->name, bus_dir, SYSFS_NODE_FILE);
    bus_node->node.show = sysfs_bus_node_show;
    bus_node->bus = bus;
    INIT_LIST_HEAD(&bus_node->devices);
    
    shell_mutex_give(sysfs_mutex);
    
    shell_log_info(TAG, "Registered bus '%s' to sysfs", bus->name);
    return 0;
}

/**
 * @brief 从sysfs注销总线
 */
void sysfs_unregister_bus(struct bus_type *bus)
{
    struct sysfs_node *bus_dir;
    struct sysfs_node *child_node;
    
    if (!bus) {
        return;
    }
    
    shell_mutex_take(sysfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 查找总线目录节点 */
    bus_dir = NULL;
    list_for_each_entry(child_node, &sysfs_root_node.node.children, sibling) {
        if (shell_strcmp(child_node->name, "bus") == 0 &&
            child_node->type == SYSFS_NODE_DIR) {
            bus_dir = child_node;
            break;
        }
    }
    
    if (bus_dir) {
        /* 在总线目录下查找并删除总线节点 */
        list_for_each_entry(child_node, &bus_dir->children, sibling) {
            if (child_node->type == SYSFS_NODE_FILE) {
                struct sysfs_bus_node *bus_node = (struct sysfs_bus_node *)child_node;
                if (bus_node->bus == bus) {
                    sysfs_remove_node(&bus_node->node);
                    shell_free(bus_node);
                    break;
                }
            }
        }
    }
    
    shell_mutex_give(sysfs_mutex);
    
    shell_log_info(TAG, "Unregistered bus '%s' from sysfs", bus->name);
}

/**
 * @brief 获取sysfs根节点
 */
struct sysfs_root *sysfs_get_root(void)
{
    return &sysfs_root_node;
}

/**
 * @brief 遍历sysfs节点
 */
void sysfs_for_each_node(struct sysfs_node *parent, sysfs_node_callback_t callback, void *data)
{
    struct sysfs_node *child;
    
    if (!parent || !callback) {
        return;
    }
    
    shell_mutex_take(sysfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 遍历子节点 */
    list_for_each_entry(child, &parent->children, sibling) {
        callback(child, data);
        
        /* 递归遍历子节点的子节点 */
        if (child->type == SYSFS_NODE_DIR) {
            sysfs_for_each_node(child, callback, data);
        }
    }
    
    shell_mutex_give(sysfs_mutex);
}