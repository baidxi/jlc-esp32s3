#pragma once

#include "fs/vfs.h"

/* sysfs 节点类型 */
enum sysfs_node_type {
    SYSFS_NODE_TYPE_DIR,
    SYSFS_NODE_TYPE_FILE,
    SYSFS_NODE_TYPE_LINK
};

/* sysfs 节点结构体 */
struct sysfs_node {
    struct list_head list;          /* 链表节点 */
    struct list_head children;       /* 子节点链表（如果是目录） */
    struct list_head sibling;       /* 兄弟节点链表 */
    struct sysfs_node *parent;      /* 父节点 */
    const char *name;               /* 节点名称 */
    enum sysfs_node_type type;      /* 节点类型 */
    mode_t mode;                    /* 访问权限 */
    vfs_off_t size;                 /* 文件大小 */
    time_t atime;                   /* 访问时间 */
    time_t mtime;                   /* 修改时间 */
    time_t ctime;                   /* 创建时间 */
    void *priv;                     /* 私有数据 */
    
    /* 文件节点特有字段 */
    char *data;                     /* 文件数据 */
    size_t data_size;               /* 数据大小 */
    
    /* 链接节点特有字段 */
    const char *target;             /* 链接目标 */
};

/* sysfs 结构体 */
struct sysfs {
    struct vfs vfs;                 /* VFS 基类 */
    struct sysfs_node *root;        /* 根节点 */
};

/* sysfs 初始化函数 */
int sysfs_init(void);

/* sysfs 清理函数 */
void sysfs_cleanup(void);

/* sysfs 创建目录函数 */
int sysfs_mkdir(const char *path, mode_t mode);

/* sysfs 创建文件函数 */
int sysfs_create_file(const char *path, mode_t mode, const char *data, size_t data_size);

/* sysfs 创建链接函数 */
int sysfs_create_symlink(const char *path, const char *target);

/* sysfs 删除节点函数 */
int sysfs_remove(const char *path);

/* sysfs 查找节点函数 */
int sysfs_lookup(const char *path, struct sysfs_node **result);

/* sysfs 获取节点数据函数 */
int sysfs_get_data(struct sysfs_node *node, char **data, size_t *data_size);

/* sysfs 设置节点数据函数 */
int sysfs_set_data(struct sysfs_node *node, const char *data, size_t data_size);

int sysfs_register(void);
int sysfs_mount(const char *path);