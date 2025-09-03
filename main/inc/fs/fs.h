#pragma once

#include <stddef.h>
#include "list.h"

/**
 * @brief sysfs节点类型
 */
enum sysfs_node_type {
    SYSFS_NODE_DIR,      /* 目录节点 */
    SYSFS_NODE_FILE,     /* 文件节点 */
};

/**
 * @brief sysfs节点结构
 */
struct sysfs_node {
    const char *name;               /* 节点名称 */
    enum sysfs_node_type type;      /* 节点类型 */
    struct sysfs_node *parent;      /* 父节点 */
    struct list_head children;       /* 子节点链表 */
    struct list_head sibling;       /* 兄弟节点链表 */
    
    /* 节点操作 */
    int (*show)(struct sysfs_node *node, char *buf, size_t size);
    int (*store)(struct sysfs_node *node, const char *buf, size_t size);
    
    /* 节点私有数据 */
    void *priv;
};

/**
 * @brief sysfs设备节点结构
 */
struct sysfs_device_node {
    struct sysfs_node node;         /* 基础节点 */
    struct device *device;          /* 关联的设备 */
    struct list_head list;          /* 链表节点 */
};

/**
 * @brief sysfs总线节点结构
 */
struct sysfs_bus_node {
    struct sysfs_node node;         /* 基础节点 */
    struct bus_type *bus;           /* 关联的总线 */
    struct list_head devices;       /* 总线下的设备链表 */
    struct list_head list;          /* 链表节点 */
};

/**
 * @brief sysfs根节点结构
 */
struct sysfs_root {
    struct sysfs_node node;         /* 根节点 */
    struct list_head devices;       /* 设备链表 */
    struct list_head buses;         /* 总线链表 */
};

/**
 * @brief sysfs节点回调函数类型
 */
typedef void (*sysfs_node_callback_t)(struct sysfs_node *node, void *data);

/* 前向声明 */
struct device;
struct bus_type;

/**
 * @brief 初始化sysfs虚拟文件系统
 *
 * @return int 0表示成功，非0表示失败
 */
int sysfs_init(void);

/**
 * @brief 创建sysfs节点
 *
 * @param name 节点名称
 * @param parent 父节点，如果为NULL则使用根节点
 * @param type 节点类型
 * @return struct sysfs_node* 创建的节点，失败返回NULL
 */
struct sysfs_node *sysfs_create_node(const char *name, struct sysfs_node *parent, enum sysfs_node_type type);

/**
 * @brief 删除sysfs节点
 *
 * @param node 要删除的节点
 */
void sysfs_remove_node(struct sysfs_node *node);

/**
 * @brief 查找sysfs节点
 *
 * @param path 节点路径
 * @return struct sysfs_node* 找到的节点，失败返回NULL
 */
struct sysfs_node *sysfs_find_node(const char *path);

/**
 * @brief 读取sysfs节点内容
 *
 * @param node 节点
 * @param buf 缓冲区
 * @param size 缓冲区大小
 * @return int 读取的字节数，失败返回负数
 */
int sysfs_read_node(struct sysfs_node *node, char *buf, size_t size);

/**
 * @brief 写入sysfs节点内容
 *
 * @param node 节点
 * @param buf 缓冲区
 * @param size 缓冲区大小
 * @return int 写入的字节数，失败返回负数
 */
int sysfs_write_node(struct sysfs_node *node, const char *buf, size_t size);

/**
 * @brief 列出sysfs目录下的子节点
 *
 * @param node 目录节点
 * @param buf 缓冲区
 * @param size 缓冲区大小
 * @return int 写入的字节数，失败返回负数
 */
int sysfs_list_dir(struct sysfs_node *node, char *buf, size_t size);

/**
 * @brief 注册设备到sysfs
 *
 * @param device 设备
 * @return int 0表示成功，非0表示失败
 */
int sysfs_register_device(struct device *device);

/**
 * @brief 从sysfs注销设备
 *
 * @param device 设备
 */
void sysfs_unregister_device(struct device *device);

/**
 * @brief 注册总线到sysfs
 *
 * @param bus 总线
 * @return int 0表示成功，非0表示失败
 */
int sysfs_register_bus(struct bus_type *bus);

/**
 * @brief 从sysfs注销总线
 *
 * @param bus 总线
 */
void sysfs_unregister_bus(struct bus_type *bus);

/**
 * @brief 获取sysfs根节点
 *
 * @return struct sysfs_root* 根节点
 */
struct sysfs_root *sysfs_get_root(void);

/**
 * @brief 遍历sysfs节点
 *
 * @param parent 父节点
 * @param callback 回调函数
 * @param data 回调函数数据
 */
void sysfs_for_each_node(struct sysfs_node *parent, sysfs_node_callback_t callback, void *data);