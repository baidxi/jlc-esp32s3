#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs/sysfs.h"
#include "shell_platform.h"
#include "fs/vfs.h"

static const char *TAG = "sysfs";

/* 全局sysfs实例 */
static struct sysfs *sysfs_instance = NULL;

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
    node->mode = (type == SYSFS_NODE_TYPE_DIR) ? S_DEFAULT_DIR_MODE : S_DEFAULT_FILE_MODE;
    node->size = 0;
    node->atime = 0;
    node->mtime = 0;
    node->ctime = 0;
    node->priv = NULL;
    node->data = NULL;
    node->data_size = 0;
    node->target = NULL;
    
    INIT_LIST_HEAD(&node->children);
    INIT_LIST_HEAD(&node->sibling);
    
    if (parent) {
        list_add_tail(&node->sibling, &parent->children);
    }
}

/**
 * @brief 创建sysfs节点
 */
static struct sysfs_node *sysfs_node_create(const char *name, struct sysfs_node *parent,
                                          enum sysfs_node_type type)
{
    struct sysfs_node *node;
    char *node_name;
    
    node = shell_malloc(sizeof(struct sysfs_node));
    if (!node) {
        return NULL;
    }
    
    /* 为节点名称分配内存 */
    node_name = shell_malloc(shell_strlen(name) + 1);
    if (!node_name) {
        shell_free(node);
        return NULL;
    }
    shell_strncpy(node_name, name, shell_strlen(name) + 1);
    
    sysfs_node_init(node, node_name, parent, type);
    
    return node;
}

/**
 * @brief 销毁sysfs节点
 */
static void sysfs_node_destroy(struct sysfs_node *node)
{
    if (!node) {
        return;
    }
    
    /* 递归销毁子节点 */
    if (node->type == SYSFS_NODE_TYPE_DIR) {
        struct sysfs_node *child, *tmp;
        list_for_each_entry_safe(child, tmp, &node->children, sibling) {
            sysfs_node_destroy(child);
        }
    }
    
    /* 从父节点的子节点链表中移除 */
    if (node->parent) {
        list_del(&node->sibling);
    }
    
    /* 释放节点名称 */
    if (node->name) {
        shell_free((void*)node->name);
    }
    
    /* 释放文件数据 */
    if (node->data) {
        shell_free(node->data);
    }
    
    /* 释放链接目标 */
    if (node->target) {
        shell_free((void*)node->target);
    }
    
    shell_free(node);
}

/**
 * @brief 查找sysfs节点
 */
static int sysfs_node_lookup(struct sysfs_node *parent, const char *name, struct sysfs_node **result)
{
    struct sysfs_node *child;
    
    if (!parent || !name || !result) {
        return -1;
    }
    
    if (parent->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    list_for_each_entry(child, &parent->children, sibling) {
        if (shell_strcmp(child->name, name) == 0) {
            *result = child;
            return 0;
        }
    }
    
    return -1;
}

/**
 * @brief 解析路径
 */
static int sysfs_path_resolve(const char *path, struct sysfs_node **result)
{
    char *path_copy, *token, *saveptr;
    const char *delim = "/";
    struct sysfs_node *node;
    int ret = 0;
    
    if (!path || !result) {
        return -1;
    }
    
    /* 复制路径字符串 */
    path_copy = shell_malloc(shell_strlen(path) + 1);
    if (!path_copy) {
        return -1;
    }
    shell_strncpy(path_copy, path, shell_strlen(path) + 1);
    
    /* 从根节点开始查找 */
    if (sysfs_instance && sysfs_instance->root) {
        node = sysfs_instance->root;
    } else {
        node = NULL;
        ret = -1;
        goto out;
    }
    
    /* 如果路径是根目录，直接返回根节点 */
    if (shell_strcmp(path, "/") == 0) {
        *result = node;
        goto out;
    }
    
    /* 解析路径 */
    token = strtok_r(path_copy, delim, &saveptr);
    while (token) {
        struct sysfs_node *child;
        bool found = false;
        
        /* 跳过空标记（由前导/或连续/产生） */
        if (shell_strlen(token) == 0) {
            token = strtok_r(NULL, delim, &saveptr);
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
            ret = -1;
            goto out;
        }
        
        token = strtok_r(NULL, delim, &saveptr);
    }
    
    *result = node;
    
out:
    shell_free(path_copy);
    return ret;
}

/**
 * @brief sysfs初始化函数
 */
int sysfs_init(void)
{
    shell_log_info(TAG, "Initializing sysfs");
    
    /* 分配sysfs实例 */
    sysfs_instance = shell_malloc(sizeof(struct sysfs));
    if (!sysfs_instance) {
        shell_log_error(TAG, "Failed to allocate sysfs instance");
        return -1;
    }
    
    /* 创建根节点 */
    sysfs_instance->root = sysfs_node_create("", NULL, SYSFS_NODE_TYPE_DIR);
    if (!sysfs_instance->root) {
        shell_log_error(TAG, "Failed to create sysfs root node");
        shell_free(sysfs_instance);
        sysfs_instance = NULL;
        return -1;
    }
    
    shell_log_info(TAG, "sysfs initialized");
    return 0;
}

/**
 * @brief sysfs清理函数
 */
void sysfs_cleanup(void)
{
    if (!sysfs_instance) {
        return;
    }
    
    shell_log_info(TAG, "Cleaning up sysfs");
    
    /* 销毁根节点 */
    if (sysfs_instance->root) {
        sysfs_node_destroy(sysfs_instance->root);
    }
    
    /* 释放sysfs实例 */
    shell_free(sysfs_instance);
    sysfs_instance = NULL;
    
    shell_log_info(TAG, "sysfs cleaned up");
}

/**
 * @brief sysfs创建目录函数
 */
int sysfs_mkdir(const char *path, mode_t mode)
{
    char *path_copy, *dir_path, *dirname;
    struct sysfs_node *parent, *new_dir;
    int ret = 0;
    
    if (!path) {
        return -1;
    }
    
    /* 复制路径字符串 */
    path_copy = shell_malloc(shell_strlen(path) + 1);
    if (!path_copy) {
        return -1;
    }
    shell_strncpy(path_copy, path, shell_strlen(path) + 1);
    
    /* 分离父目录路径和目录名 */
    dirname = path_copy + shell_strlen(path_copy) - 1;
    while (dirname >= path_copy && *dirname != '/') {
        dirname--;
    }
    if (dirname < path_copy) {
        shell_free(path_copy);
        return -1;
    }
    
    if (dirname == path_copy) {
        /* 根目录 */
        dir_path = "/";
        dirname++;
    } else {
        *dirname = '\0';
        dir_path = path_copy;
        dirname++;
    }
    
    /* 查找父目录 */
    ret = sysfs_path_resolve(dir_path, &parent);
    if (ret != 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查父目录是否是目录 */
    if (parent->type != SYSFS_NODE_TYPE_DIR) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查目录是否已存在 */
    if (sysfs_node_lookup(parent, dirname, &new_dir) == 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 创建目录节点 */
    new_dir = sysfs_node_create(dirname, parent, SYSFS_NODE_TYPE_DIR);
    if (!new_dir) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 设置目录模式 */
    new_dir->mode = mode;
    
    /* 获取当前时间 */
    // TODO: 实现获取当前时间的函数
    new_dir->atime = 0;
    new_dir->mtime = 0;
    new_dir->ctime = 0;
    
    shell_free(path_copy);
    return 0;
}

/**
 * @brief sysfs创建文件函数
 */
int sysfs_create_file(const char *path, mode_t mode, const char *data, size_t data_size)
{
    char *path_copy, *dir_path, *filename;
    struct sysfs_node *parent, *new_file;
    int ret = 0;
    
    if (!path) {
        return -1;
    }
    
    /* 复制路径字符串 */
    path_copy = shell_malloc(shell_strlen(path) + 1);
    if (!path_copy) {
        return -1;
    }
    shell_strncpy(path_copy, path, shell_strlen(path) + 1);
    
    /* 分离目录路径和文件名 */
    filename = path_copy + shell_strlen(path_copy) - 1;
    while (filename >= path_copy && *filename != '/') {
        filename--;
    }
    if (filename < path_copy) {
        shell_free(path_copy);
        return -1;
    }
    
    if (filename == path_copy) {
        /* 根目录 */
        dir_path = "/";
        filename++;
    } else {
        *filename = '\0';
        dir_path = path_copy;
        filename++;
    }
    
    /* 查找父目录 */
    ret = sysfs_path_resolve(dir_path, &parent);
    if (ret != 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查父目录是否是目录 */
    if (parent->type != SYSFS_NODE_TYPE_DIR) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查文件是否已存在 */
    if (sysfs_node_lookup(parent, filename, &new_file) == 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 创建文件节点 */
    new_file = sysfs_node_create(filename, parent, SYSFS_NODE_TYPE_FILE);
    if (!new_file) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 设置文件模式 */
    new_file->mode = mode;
    
    /* 设置文件数据 */
    if (data && data_size > 0) {
        new_file->data = shell_malloc(data_size);
        if (!new_file->data) {
            sysfs_node_destroy(new_file);
            shell_free(path_copy);
            return -1;
        }
        memcpy(new_file->data, data, data_size);
        new_file->data_size = data_size;
        new_file->size = data_size;
    }
    
    /* 获取当前时间 */
    // TODO: 实现获取当前时间的函数
    new_file->atime = 0;
    new_file->mtime = 0;
    new_file->ctime = 0;
    
    shell_free(path_copy);
    return 0;
}

/* VFS操作函数前向声明 */
static int sysfs_vfs_open(struct vfs_node *node, struct file *file);
static int sysfs_vfs_close(struct file *file);
static int sysfs_vfs_read(struct file *file, void *buf, size_t len);
static int sysfs_vfs_write(struct file *file, const void *buf, size_t len);
static int sysfs_vfs_lseek(struct file *file, vfs_off_t offset, int whence);
static int sysfs_vfs_ioctl(struct file *file, unsigned long cmd, unsigned long arg);
static int sysfs_vfs_mkdir(struct vfs_node *node, const char *name, mode_t mode);
static int sysfs_vfs_rmdir(struct vfs_node *node, const char *name);
static int sysfs_vfs_lookup(struct vfs_node *parent, const char *name, struct vfs_node **result);
static int sysfs_vfs_create(struct vfs_node *parent, const char *name, mode_t mode, struct vfs_node **result);
static int sysfs_vfs_unlink(struct vfs_node *parent, const char *name);
static int sysfs_vfs_rename(struct vfs_node *old_parent, const char *old_name,
                           struct vfs_node *new_parent, const char *new_name);
static int sysfs_vfs_readdir(struct file *file, struct dirent *dirent);
static int sysfs_vfs_getattr(struct vfs_node *node, struct stat *attr);
static int sysfs_vfs_setattr(struct vfs_node *node, const struct stat *attr);

/* VFS操作结构体 */
static const struct vfs_ops sysfs_vfs_ops = {
    .open = sysfs_vfs_open,
    .close = sysfs_vfs_close,
    .read = sysfs_vfs_read,
    .write = sysfs_vfs_write,
    .lseek = sysfs_vfs_lseek,
    .ioctl = sysfs_vfs_ioctl,
    .mkdir = sysfs_vfs_mkdir,
    .rmdir = sysfs_vfs_rmdir,
    .lookup = sysfs_vfs_lookup,
    .create = sysfs_vfs_create,
    .unlink = sysfs_vfs_unlink,
    .rename = sysfs_vfs_rename,
    .readdir = sysfs_vfs_readdir,
    .getattr = sysfs_vfs_getattr,
    .setattr = sysfs_vfs_setattr,
};

/**
 * @brief 将sysfs节点类型转换为VFS节点类型
 */
static enum vfs_node_type sysfs_to_vfs_node_type(enum sysfs_node_type type)
{
    switch (type) {
    case SYSFS_NODE_TYPE_DIR:
        return VFS_NODE_TYPE_DIR;
    case SYSFS_NODE_TYPE_FILE:
        return VFS_NODE_TYPE_FILE;
    case SYSFS_NODE_TYPE_LINK:
        return VFS_NODE_TYPE_LINK;
    default:
        return VFS_NODE_TYPE_UNKNOWN;
    }
}


/**
 * @brief 打开文件
 */
static int sysfs_vfs_open(struct vfs_node *node, struct file *file)
{
    struct sysfs_node *sysfs_node;
    
    if (!node || !file) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 检查节点类型 */
    if (sysfs_node->type != SYSFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    /* 更新访问时间 */
    // TODO: 实现获取当前时间的函数
    sysfs_node->atime = 0;
    
    return 0;
}

/**
 * @brief 关闭文件
 */
static int sysfs_vfs_close(struct file *file)
{
    if (!file) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 读取文件
 */
static int sysfs_vfs_read(struct file *file, void *buf, size_t len)
{
    struct sysfs_node *sysfs_node;
    size_t read_len;
    
    if (!file || !buf || len == 0) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)file->node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 检查节点类型 */
    if (sysfs_node->type != SYSFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    /* 检查是否有数据 */
    if (!sysfs_node->data || sysfs_node->data_size == 0) {
        return 0;
    }
    
    /* 计算可读取的长度 */
    if (file->pos >= sysfs_node->data_size) {
        return 0; /* 已经到达文件末尾 */
    }
    
    read_len = sysfs_node->data_size - file->pos;
    if (read_len > len) {
        read_len = len;
    }
    
    /* 复制数据 */
    memcpy(buf, sysfs_node->data + file->pos, read_len);
    
    /* 更新访问时间 */
    // TODO: 实现获取当前时间的函数
    sysfs_node->atime = 0;
    
    return read_len;
}

/**
 * @brief 写入文件
 */
static int sysfs_vfs_write(struct file *file, const void *buf, size_t len)
{
    struct sysfs_node *sysfs_node;
    char *new_data;
    
    if (!file || !buf || len == 0) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)file->node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 检查节点类型 */
    if (sysfs_node->type != SYSFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    /* 检查是否需要扩展数据缓冲区 */
    if (file->pos + len > sysfs_node->data_size) {
        new_data = shell_malloc(file->pos + len);
        if (!new_data) {
            return -1;
        }
        /* 复制旧数据到新缓冲区 */
        if (sysfs_node->data && sysfs_node->data_size > 0) {
            memcpy(new_data, sysfs_node->data, sysfs_node->data_size);
        }
        /* 释放旧缓冲区 */
        if (sysfs_node->data) {
            shell_free(sysfs_node->data);
        }
        sysfs_node->data = new_data;
        sysfs_node->data_size = file->pos + len;
        sysfs_node->size = file->pos + len;
    }
    
    /* 复制数据 */
    memcpy(sysfs_node->data + file->pos, buf, len);
    
    /* 更新修改时间 */
    // TODO: 实现获取当前时间的函数
    sysfs_node->mtime = 0;
    
    return len;
}

/**
 * @brief 定位文件指针
 */
static int sysfs_vfs_lseek(struct file *file, vfs_off_t offset, int whence)
{
    struct sysfs_node *sysfs_node;
    vfs_off_t new_pos;
    
    if (!file) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)file->node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 检查节点类型 */
    if (sysfs_node->type != SYSFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = file->pos + offset;
        break;
    case SEEK_END:
        new_pos = sysfs_node->data_size + offset;
        break;
    default:
        return -1;
    }
    
    /* 检查新位置是否有效 */
    if (new_pos < 0) {
        return -1;
    }
    
    /* 更新文件位置 */
    file->pos = new_pos;
    
    return 0;
}

/**
 * @brief 控制文件
 */
static int sysfs_vfs_ioctl(struct file *file, unsigned long cmd, unsigned long arg)
{
    if (!file) {
        return -1;
    }
    
    return -1; /* 不支持ioctl操作 */
}

/**
 * @brief 创建目录
 */
static int sysfs_vfs_mkdir(struct vfs_node *node, const char *name, mode_t mode)
{
    struct sysfs_node *sysfs_node, *new_dir;
    
    if (!node || !name) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 检查父节点是否是目录 */
    if (sysfs_node->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 检查目录是否已存在 */
    if (sysfs_node_lookup(sysfs_node, name, &new_dir) == 0) {
        return -1;
    }
    
    /* 创建目录节点 */
    new_dir = sysfs_node_create(name, sysfs_node, SYSFS_NODE_TYPE_DIR);
    if (!new_dir) {
        return -1;
    }
    
    /* 设置目录模式 */
    new_dir->mode = mode;
    
    /* 获取当前时间 */
    // TODO: 实现获取当前时间的函数
    new_dir->atime = 0;
    new_dir->mtime = 0;
    new_dir->ctime = 0;
    
    return 0;
}

/**
 * @brief 删除目录
 */
static int sysfs_vfs_rmdir(struct vfs_node *node, const char *name)
{
    struct sysfs_node *sysfs_node, *child;
    
    if (!node || !name) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 检查父节点是否是目录 */
    if (sysfs_node->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 查找子节点 */
    if (sysfs_node_lookup(sysfs_node, name, &child) != 0) {
        return -1;
    }
    
    /* 检查子节点是否是目录 */
    if (child->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 检查目录是否为空 */
    if (!list_empty(&child->children)) {
        return -1;
    }
    
    /* 销毁节点 */
    sysfs_node_destroy(child);
    
    return 0;
}

/**
 * @brief 查找节点
 */
static int sysfs_vfs_lookup(struct vfs_node *parent, const char *name, struct vfs_node **result)
{
    struct sysfs_node *sysfs_parent, *sysfs_child;
    
    if (!parent || !name || !result) {
        return -1;
    }
    
    sysfs_parent = (struct sysfs_node *)parent->priv;
    if (!sysfs_parent) {
        return -1;
    }
    
    /* 检查父节点是否是目录 */
    if (sysfs_parent->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 查找子节点 */
    if (sysfs_node_lookup(sysfs_parent, name, &sysfs_child) != 0) {
        return -1;
    }
    
    /* 创建VFS节点 */
    *result = shell_malloc(sizeof(struct vfs_node));
    if (!*result) {
        return -1;
    }
    
    /* 初始化VFS节点 */
    INIT_LIST_HEAD(&(*result)->list);
    INIT_LIST_HEAD(&(*result)->children);
    INIT_LIST_HEAD(&(*result)->sibling);
    (*result)->parent = parent;
    (*result)->vfs = parent->vfs;
    (*result)->name = sysfs_child->name;
    (*result)->type = sysfs_to_vfs_node_type(sysfs_child->type);
    (*result)->priv = sysfs_child;
    (*result)->mode = sysfs_child->mode;
    (*result)->uid = 0;
    (*result)->gid = 0;
    (*result)->size = sysfs_child->size;
    (*result)->atime = sysfs_child->atime;
    (*result)->mtime = sysfs_child->mtime;
    (*result)->ctime = sysfs_child->ctime;
    
    return 0;
}

/**
 * @brief 创建文件
 */
static int sysfs_vfs_create(struct vfs_node *parent, const char *name, mode_t mode, struct vfs_node **result)
{
    struct sysfs_node *sysfs_parent, *new_file;
    
    if (!parent || !name || !result) {
        return -1;
    }
    
    sysfs_parent = (struct sysfs_node *)parent->priv;
    if (!sysfs_parent) {
        return -1;
    }
    
    /* 检查父节点是否是目录 */
    if (sysfs_parent->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 检查文件是否已存在 */
    if (sysfs_node_lookup(sysfs_parent, name, &new_file) == 0) {
        return -1;
    }
    
    /* 创建文件节点 */
    new_file = sysfs_node_create(name, sysfs_parent, SYSFS_NODE_TYPE_FILE);
    if (!new_file) {
        return -1;
    }
    
    /* 设置文件模式 */
    new_file->mode = mode;
    
    /* 获取当前时间 */
    // TODO: 实现获取当前时间的函数
    new_file->atime = 0;
    new_file->mtime = 0;
    new_file->ctime = 0;
    
    /* 创建VFS节点 */
    *result = shell_malloc(sizeof(struct vfs_node));
    if (!*result) {
        sysfs_node_destroy(new_file);
        return -1;
    }
    
    /* 初始化VFS节点 */
    INIT_LIST_HEAD(&(*result)->list);
    INIT_LIST_HEAD(&(*result)->children);
    INIT_LIST_HEAD(&(*result)->sibling);
    (*result)->parent = parent;
    (*result)->vfs = parent->vfs;
    (*result)->name = new_file->name;
    (*result)->type = sysfs_to_vfs_node_type(new_file->type);
    (*result)->priv = new_file;
    (*result)->mode = new_file->mode;
    (*result)->uid = 0;
    (*result)->gid = 0;
    (*result)->size = new_file->size;
    (*result)->atime = new_file->atime;
    (*result)->mtime = new_file->mtime;
    (*result)->ctime = new_file->ctime;
    
    return 0;
}

/**
 * @brief 删除文件
 */
static int sysfs_vfs_unlink(struct vfs_node *parent, const char *name)
{
    struct sysfs_node *sysfs_parent, *child;
    
    if (!parent || !name) {
        return -1;
    }
    
    sysfs_parent = (struct sysfs_node *)parent->priv;
    if (!sysfs_parent) {
        return -1;
    }
    
    /* 检查父节点是否是目录 */
    if (sysfs_parent->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 查找子节点 */
    if (sysfs_node_lookup(sysfs_parent, name, &child) != 0) {
        return -1;
    }
    
    /* 检查子节点是否是文件 */
    if (child->type != SYSFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    /* 销毁节点 */
    sysfs_node_destroy(child);
    
    return 0;
}

/**
 * @brief 重命名文件或目录
 */
static int sysfs_vfs_rename(struct vfs_node *old_parent, const char *old_name,
                           struct vfs_node *new_parent, const char *new_name)
{
    struct sysfs_node *sysfs_old_parent, *sysfs_new_parent, *child;
    
    if (!old_parent || !old_name || !new_parent || !new_name) {
        return -1;
    }
    
    sysfs_old_parent = (struct sysfs_node *)old_parent->priv;
    sysfs_new_parent = (struct sysfs_node *)new_parent->priv;
    if (!sysfs_old_parent || !sysfs_new_parent) {
        return -1;
    }
    
    /* 检查父节点是否是目录 */
    if (sysfs_old_parent->type != SYSFS_NODE_TYPE_DIR ||
        sysfs_new_parent->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 查找子节点 */
    if (sysfs_node_lookup(sysfs_old_parent, old_name, &child) != 0) {
        return -1;
    }
    
    /* 检查新名称是否已存在 */
    if (sysfs_old_parent != sysfs_new_parent || shell_strcmp(old_name, new_name) != 0) {
        struct sysfs_node *existing;
        if (sysfs_node_lookup(sysfs_new_parent, new_name, &existing) == 0) {
            return -1;
        }
    }
    
    /* 从旧父节点的子节点链表中移除 */
    if (child->parent) {
        list_del(&child->sibling);
    }
    
    /* 添加到新父节点的子节点链表 */
    child->parent = sysfs_new_parent;
    list_add_tail(&child->sibling, &sysfs_new_parent->children);
    
    /* 更新节点名称 */
    // 释放旧名称
    if (child->name) {
        shell_free((void*)child->name);
    }
    // 分配新名称
    child->name = shell_malloc(shell_strlen(new_name) + 1);
    if (!child->name) {
        // 恢复到旧父节点
        child->parent = sysfs_old_parent;
        list_add_tail(&child->sibling, &sysfs_old_parent->children);
        return -1;
    }
    shell_strncpy((char*)child->name, new_name, shell_strlen(new_name) + 1);
    
    /* 更新修改时间 */
    // TODO: 实现获取当前时间的函数
    child->mtime = 0;
    
    return 0;
}

/**
 * @brief 读取目录
 */
static int sysfs_vfs_readdir(struct file *file, struct dirent *dirent)
{
    struct sysfs_node *sysfs_node, *child;
    int i;
    
    if (!file || !dirent) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)file->node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 检查节点是否是目录 */
    if (sysfs_node->type != SYSFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 查找第file->pos个子节点 */
    i = 0;
    list_for_each_entry(child, &sysfs_node->children, sibling) {
        if (i++ == file->pos) {
            /* 填充目录项 */
            dirent->d_ino = (ino_t)child;  /* 使用节点指针作为inode号 */
            dirent->d_off = file->pos;
            dirent->d_reclen = sizeof(struct dirent);
            
            switch (child->type) {
            case SYSFS_NODE_TYPE_DIR:
                dirent->d_type = DT_DIR;
                break;
            case SYSFS_NODE_TYPE_FILE:
                dirent->d_type = DT_REG;
                break;
            case SYSFS_NODE_TYPE_LINK:
                dirent->d_type = DT_LNK;
                break;
            default:
                dirent->d_type = DT_UNKNOWN;
                break;
            }
            
            shell_strncpy(dirent->d_name, child->name, MAX_PATH - 1);
            dirent->d_name[MAX_PATH - 1] = '\0';
            
            /* 更新文件位置 */
            file->pos++;
            
            /* 更新访问时间 */
            // TODO: 实现获取当前时间的函数
            sysfs_node->atime = 0;
            
            return 0;
        }
    }
    
    /* 已经到达目录末尾 */
    return -1;
}

/**
 * @brief 获取文件属性
 */
static int sysfs_vfs_getattr(struct vfs_node *node, struct stat *attr)
{
    struct sysfs_node *sysfs_node;
    
    if (!node || !attr) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 填充文件属性 */
    for (size_t i = 0; i < sizeof(struct stat); i++) {
        ((char *)attr)[i] = 0;
    }
    
    switch (sysfs_node->type) {
    case SYSFS_NODE_TYPE_FILE:
        attr->st_mode = S_IFREG | sysfs_node->mode;
        break;
    case SYSFS_NODE_TYPE_DIR:
        attr->st_mode = S_IFDIR | sysfs_node->mode;
        break;
    case SYSFS_NODE_TYPE_LINK:
        attr->st_mode = S_IFLNK | sysfs_node->mode;
        break;
    default:
        attr->st_mode = sysfs_node->mode;
        break;
    }
    
    attr->st_uid = 0;
    attr->st_gid = 0;
    attr->st_size = sysfs_node->size;
    attr->st_atime = sysfs_node->atime;
    attr->st_mtime = sysfs_node->mtime;
    attr->st_ctime = sysfs_node->ctime;
    
    return 0;
}

/**
 * @brief 设置文件属性
 */
static int sysfs_vfs_setattr(struct vfs_node *node, const struct stat *attr)
{
    struct sysfs_node *sysfs_node;
    
    if (!node || !attr) {
        return -1;
    }
    
    sysfs_node = (struct sysfs_node *)node->priv;
    if (!sysfs_node) {
        return -1;
    }
    
    /* 设置文件模式 */
    sysfs_node->mode = attr->st_mode;
    
    /* 更新修改时间 */
    // TODO: 实现获取当前时间的函数
    sysfs_node->mtime = 0;
    
    return 0;
}

/**
 * @brief sysfs模块初始化和注册函数
 */
int sysfs_register(void)
{
    int ret;
    
    shell_log_info(TAG, "Registering sysfs module");
    
    /* 初始化sysfs */
    ret = sysfs_init();
    if (ret != 0) {
        shell_log_error(TAG, "Failed to initialize sysfs");
        return ret;
    }
    
    /* 初始化VFS结构 */
    INIT_LIST_HEAD(&sysfs_instance->vfs.list);
    shell_strncpy(sysfs_instance->vfs.name, "sysfs", MAX_PATH - 1);
    sysfs_instance->vfs.name[MAX_PATH - 1] = '\0';
    sysfs_instance->vfs.ops = &sysfs_vfs_ops;
    sysfs_instance->vfs.priv = sysfs_instance;
    sysfs_instance->vfs.mount = NULL;
    
    /* 注册VFS */
    ret = vfs_register(&sysfs_instance->vfs);
    if (ret != 0) {
        shell_log_error(TAG, "Failed to register sysfs VFS");
        sysfs_cleanup();
        return ret;
    }
    
    shell_log_info(TAG, "sysfs module registered successfully");
    return 0;
}

/**
 * @brief sysfs模块注销和清理函数
 */
int sysfs_unregister(void)
{
    shell_log_info(TAG, "Unregistering sysfs module");
    
    if (!sysfs_instance) {
        shell_log_warn(TAG, "sysfs module not registered");
        return 0;
    }
    
    /* 注销VFS */
    vfs_unregister(&sysfs_instance->vfs);
    
    /* 清理sysfs */
    sysfs_cleanup();
    
    shell_log_info(TAG, "sysfs module unregistered successfully");
    return 0;
}

/**
 * @brief sysfs模块挂载函数
 */
int sysfs_mount(const char *path)
{
    int ret;
    
    if (!path) {
        return -1;
    }
    
    shell_log_info(TAG, "Mounting sysfs at '%s'", path);
    
    /* 挂载sysfs */
    ret = vfs_mount(NULL, path, "sysfs", 0, NULL);
    if (ret != 0) {
        shell_log_error(TAG, "Failed to mount sysfs at '%s'", path);
        return ret;
    }
    
    shell_log_info(TAG, "sysfs mounted successfully at '%s'", path);
    return 0;
}

/**
 * @brief sysfs模块卸载函数
 */
int sysfs_umount(const char *path)
{
    int ret;
    
    if (!path) {
        return -1;
    }
    
    shell_log_info(TAG, "Unmounting sysfs from '%s'", path);
    
    /* 卸载sysfs */
    ret = vfs_umount(path);
    if (ret != 0) {
        shell_log_error(TAG, "Failed to unmount sysfs from '%s'", path);
        return ret;
    }
    
    shell_log_info(TAG, "sysfs unmounted successfully from '%s'", path);
    return 0;
}

/**
 * @brief sysfs创建链接函数
 */
int sysfs_create_symlink(const char *path, const char *target)
{
    char *path_copy, *dir_path, *linkname;
    struct sysfs_node *parent, *new_link;
    int ret = 0;
    
    if (!path || !target) {
        return -1;
    }
    
    /* 复制路径字符串 */
    path_copy = shell_malloc(shell_strlen(path) + 1);
    if (!path_copy) {
        return -1;
    }
    shell_strncpy(path_copy, path, shell_strlen(path) + 1);
    
    /* 分离目录路径和链接名 */
    linkname = path_copy + shell_strlen(path_copy) - 1;
    while (linkname >= path_copy && *linkname != '/') {
        linkname--;
    }
    if (linkname < path_copy) {
        shell_free(path_copy);
        return -1;
    }
    
    if (linkname == path_copy) {
        /* 根目录 */
        dir_path = "/";
        linkname++;
    } else {
        *linkname = '\0';
        dir_path = path_copy;
        linkname++;
    }
    
    /* 查找父目录 */
    ret = sysfs_path_resolve(dir_path, &parent);
    if (ret != 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查父目录是否是目录 */
    if (parent->type != SYSFS_NODE_TYPE_DIR) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查链接是否已存在 */
    if (sysfs_node_lookup(parent, linkname, &new_link) == 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 创建链接节点 */
    new_link = sysfs_node_create(linkname, parent, SYSFS_NODE_TYPE_LINK);
    if (!new_link) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 设置链接目标 */
    new_link->target = shell_malloc(shell_strlen(target) + 1);
    if (!new_link->target) {
        sysfs_node_destroy(new_link);
        shell_free(path_copy);
        return -1;
    }
    shell_strncpy((char*)new_link->target, target, shell_strlen(target) + 1);
    
    /* 获取当前时间 */
    // TODO: 实现获取当前时间的函数
    new_link->atime = 0;
    new_link->mtime = 0;
    new_link->ctime = 0;
    
    shell_free(path_copy);
    return 0;
}

/**
 * @brief sysfs删除节点函数
 */
int sysfs_remove(const char *path)
{
    struct sysfs_node *node;
    int ret;
    
    if (!path) {
        return -1;
    }
    
    /* 查找节点 */
    ret = sysfs_path_resolve(path, &node);
    if (ret != 0) {
        return -1;
    }
    
    /* 不能删除根节点 */
    if (node == sysfs_instance->root) {
        return -1;
    }
    
    /* 如果是目录，检查是否为空 */
    if (node->type == SYSFS_NODE_TYPE_DIR && !list_empty(&node->children)) {
        return -1;
    }
    
    /* 销毁节点 */
    sysfs_node_destroy(node);
    
    return 0;
}

/**
 * @brief sysfs查找节点函数
 */
int sysfs_lookup(const char *path, struct sysfs_node **result)
{
    return sysfs_path_resolve(path, result);
}

/**
 * @brief sysfs获取节点数据函数
 */
int sysfs_get_data(struct sysfs_node *node, char **data, size_t *data_size)
{
    if (!node || !data || !data_size) {
        return -1;
    }
    
    /* 只有文件节点有数据 */
    if (node->type != SYSFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    *data = node->data;
    *data_size = node->data_size;
    
    return 0;
}

/**
 * @brief sysfs设置节点数据函数
 */
int sysfs_set_data(struct sysfs_node *node, const char *data, size_t data_size)
{
    if (!node || !data) {
        return -1;
    }
    
    /* 只有文件节点可以设置数据 */
    if (node->type != SYSFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    /* 释放旧数据 */
    if (node->data) {
        shell_free(node->data);
        node->data = NULL;
    }
    
    /* 分配新数据 */
    node->data = shell_malloc(data_size);
    if (!node->data) {
        return -1;
    }
    
    /* 复制数据 */
    memcpy(node->data, data, data_size);
    node->data_size = data_size;
    node->size = data_size;
    
    /* 更新修改时间 */
    // TODO: 实现获取当前时间的函数
    node->mtime = 0;
    
    return 0;
}
