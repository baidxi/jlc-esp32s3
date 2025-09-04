#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "fs/vfs.h"
#include "shell_platform.h"

static const char *TAG = "vfs";

/* 全局VFS链表 */
static struct list_head vfs_list;
static struct list_head mount_list;

/* VFS互斥锁 */
static shell_mutex_t vfs_mutex = NULL;

/* 根挂载点 */
static struct mount_point *root_mount = NULL;

/**
 * @brief 初始化VFS节点
 */
static void vfs_node_init(struct vfs_node *node, const char *name,
                         struct vfs_node *parent, enum vfs_node_type type,
                         struct vfs *vfs)
{
    if (!node) {
        return;
    }
    
    node->name = name;
    node->type = type;
    node->parent = parent;
    node->vfs = vfs;
    node->mode = (type == VFS_NODE_TYPE_DIR) ? S_DEFAULT_DIR_MODE : S_DEFAULT_FILE_MODE;
    node->uid = 0;
    node->gid = 0;
    node->size = 0;
    node->atime = 0;
    node->mtime = 0;
    node->ctime = 0;
    node->priv = NULL;
    
    INIT_LIST_HEAD(&node->children);
    INIT_LIST_HEAD(&node->sibling);
    
    if (parent) {
        list_add_tail(&node->sibling, &parent->children);
    }
}

/**
 * @brief 创建V节点
 */
static struct vfs_node *vfs_node_create(const char *name, struct vfs_node *parent,
                                      enum vfs_node_type type, struct vfs *vfs)
{
    struct vfs_node *node;
    
    node = shell_malloc(sizeof(struct vfs_node));
    if (!node) {
        return NULL;
    }
    
    vfs_node_init(node, name, parent, type, vfs);
    
    return node;
}

/**
 * @brief 销毁V节点
 */
static void vfs_node_destroy(struct vfs_node *node)
{
    if (!node) {
        return;
    }
    
    /* 递归销毁子节点 */
    if (node->type == VFS_NODE_TYPE_DIR) {
        struct vfs_node *child, *tmp;
        list_for_each_entry_safe(child, tmp, &node->children, sibling) {
            vfs_node_destroy(child);
        }
    }
    
    /* 从父节点的子节点链表中移除 */
    if (node->parent) {
        list_del(&node->sibling);
    }
    
    shell_free(node);
}

/**
 * @brief 查找V节点
 */
static int vfs_node_lookup(struct vfs_node *parent, const char *name, struct vfs_node **result)
{
    struct vfs_node *child;
    
    if (!parent || !name || !result) {
        return -1;
    }
    
    if (parent->type != VFS_NODE_TYPE_DIR) {
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
static int path_resolve(const char *path, struct vfs_node **result)
{
    char *path_copy, *token, *saveptr;
    const char *delim = "/";
    struct vfs_node *node;
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
    
    /* 检查互斥锁是否已初始化 */
    if (!vfs_mutex) {
        shell_free(path_copy);
        return -1;
    }
    
    shell_mutex_take(vfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 从根节点开始查找 */
    if (root_mount) {
        node = root_mount->root;
    } else {
        /* 如果没有根挂载点，但是路径是根目录，则创建一个临时根节点 */
        if (shell_strcmp(path, "/") == 0) {
            /* 创建临时根节点 */
            node = shell_malloc(sizeof(struct vfs_node));
            if (!node) {
                ret = -1;
                goto out;
            }
            
            /* 初始化临时根节点 */
            node->name = "";
            node->type = VFS_NODE_TYPE_DIR;
            node->parent = NULL;
            node->vfs = NULL;
            node->mode = S_DEFAULT_DIR_MODE;
            node->uid = 0;
            node->gid = 0;
            node->size = 0;
            node->atime = 0;
            node->mtime = 0;
            node->ctime = 0;
            node->priv = NULL;
            
            INIT_LIST_HEAD(&node->children);
            INIT_LIST_HEAD(&node->sibling);
        } else {
            node = NULL;
            ret = -1;
            goto out;
        }
    }
    
    /* 如果路径是根目录，直接返回根节点 */
    if (shell_strcmp(path, "/") == 0) {
        *result = node;
        goto out;
    }
    
    /* 解析路径 */
    token = strtok_r(path_copy, delim, &saveptr);
    while (token) {
        struct vfs_node *child;
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
    shell_mutex_give(vfs_mutex);
    shell_free(path_copy);
    return ret;
}

/**
 * @brief 查找挂载点
 */
static struct mount_point *find_mount_point(const char *path)
{
    struct mount_point *mount, *best_match = NULL;
    size_t path_len, best_len = 0;
    
    if (!path) {
        return NULL;
    }
    
    path_len = shell_strlen(path);
    
    /* 检查互斥锁是否已初始化 */
    if (!vfs_mutex) {
        return NULL;
    }
    
    shell_mutex_take(vfs_mutex, SHELL_WAIT_FOREVER);
    
    list_for_each_entry(mount, &mount_list, list) {
        size_t mount_len = shell_strlen(mount->path);
        
        /* 检查路径是否以挂载点路径开头 */
        if (path_len >= mount_len &&
            shell_strncmp(path, mount->path, mount_len) == 0 &&
            (mount_len == 1 || path_len == mount_len || path[mount_len] == '/')) {
            
            /* 选择最长的匹配 */
            if (mount_len > best_len) {
                best_len = mount_len;
                best_match = mount;
            }
        }
    }
    
    shell_mutex_give(vfs_mutex);
    return best_match;
}

/**
 * @brief 初始化VFS
 */
int vfs_init(void)
{
    shell_log_info(TAG, "Initializing VFS");
    
    /* 初始化互斥锁 */
    if (!vfs_mutex) {
        vfs_mutex = shell_mutex_create();
        if (!vfs_mutex) {
            shell_log_error(TAG, "Failed to create VFS mutex");
            return -1;
        }
    }
    
    /* 初始化链表 */
    INIT_LIST_HEAD(&vfs_list);
    INIT_LIST_HEAD(&mount_list);
    
    shell_log_info(TAG, "VFS initialized");
    return 0;
}

/**
 * @brief 注册VFS
 */
int vfs_register(struct vfs *vfs)
{
    if (!vfs) {
        return -1;
    }
    
    /* 检查互斥锁是否已初始化 */
    if (!vfs_mutex) {
        return -1;
    }
    
    shell_mutex_take(vfs_mutex, SHELL_WAIT_FOREVER);
    list_add_tail(&vfs->list, &vfs_list);
    shell_mutex_give(vfs_mutex);
    
    shell_log_info(TAG, "Registered VFS '%s'", vfs->name);
    return 0;
}

/**
 * @brief 注销VFS
 */
int vfs_unregister(struct vfs *vfs)
{
    if (!vfs) {
        return -1;
    }
    
    /* 检查互斥锁是否已初始化 */
    if (!vfs_mutex) {
        return -1;
    }
    
    shell_mutex_take(vfs_mutex, SHELL_WAIT_FOREVER);
    list_del(&vfs->list);
    shell_mutex_give(vfs_mutex);
    
    shell_log_info(TAG, "Unregistered VFS '%s'", vfs->name);
    return 0;
}

/**
 * @brief 挂载VFS
 */
int vfs_mount(const char *source, const char *target,
              const char *filesystemtype, unsigned long mountflags,
              const void *data)
{
    struct vfs *vfs;
    struct mount_point *mount;
    struct vfs_node *target_node = NULL;
    int ret = 0;
    
    if (!target || !filesystemtype) {
        return -1;
    }
    
    /* 检查互斥锁是否已初始化 */
    if (!vfs_mutex) {
        return -1;
    }
    
    shell_mutex_take(vfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 查找文件系统类型 */
    list_for_each_entry(vfs, &vfs_list, list) {
        if (shell_strcmp(vfs->name, filesystemtype) == 0) {
            break;
        }
    }
    
    if (&vfs->list == &vfs_list) {
        shell_mutex_give(vfs_mutex);
        shell_log_error(TAG, "Filesystem type '%s' not found", filesystemtype);
        return -1;
    }
    
    shell_mutex_give(vfs_mutex);
    
    /* 分配挂载点结构 */
    mount = shell_malloc(sizeof(struct mount_point));
    if (!mount) {
        return -1;
    }
    
    /* 初始化挂载点 */
    shell_strncpy(mount->path, target, MAX_PATH - 1);
    mount->path[MAX_PATH - 1] = '\0';
    mount->vfs = vfs;
    mount->parent = NULL;
    INIT_LIST_HEAD(&mount->children);
    
    /* 创建根节点 */
    mount->root = vfs_node_create("", NULL, VFS_NODE_TYPE_DIR, vfs);
    if (!mount->root) {
        shell_free(mount);
        return -1;
    }
    
    /* 如果不是根挂载点，先查找目标节点和父挂载点 */
    if (shell_strcmp(target, "/") != 0) {
        /* 查找目标节点（不持有互斥锁） */
        ret = path_resolve(target, &target_node);
        if (ret != 0) {
            vfs_node_destroy(mount->root);
            shell_free(mount);
            shell_log_error(TAG, "Target path '%s' not found", target);
            return -1;
        }
        
        /* 查找父挂载点（不持有互斥锁） */
        mount->parent = find_mount_point(target);
    }
    
    /* 检查互斥锁是否已初始化 */
    if (!vfs_mutex) {
        vfs_node_destroy(mount->root);
        shell_free(mount);
        return -1;
    }
    
    shell_mutex_take(vfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 如果是根挂载点 */
    if (shell_strcmp(target, "/") == 0) {
        if (root_mount) {
            shell_mutex_give(vfs_mutex);
            vfs_node_destroy(mount->root);
            shell_free(mount);
            shell_log_error(TAG, "Root filesystem already mounted");
            return -1;
        }
        root_mount = mount;
    } else {
        /* 使用之前查找的目标节点和父挂载点 */
        if (mount->parent) {
            list_add_tail(&mount->list, &mount->parent->children);
        }
    }
    
    /* 添加到挂载点链表 */
    list_add_tail(&mount->list, &mount_list);
    
    shell_mutex_give(vfs_mutex);
    
    shell_log_info(TAG, "Mounted '%s' at '%s'", filesystemtype, target);
    return 0;
}

/**
 * @brief 卸载VFS
 */
int vfs_umount(const char *target)
{
    struct mount_point *mount;
    
    if (!target) {
        return -1;
    }
    
    /* 检查互斥锁是否已初始化 */
    if (!vfs_mutex) {
        return -1;
    }
    
    shell_mutex_take(vfs_mutex, SHELL_WAIT_FOREVER);
    
    /* 查找挂载点 */
    list_for_each_entry(mount, &mount_list, list) {
        if (shell_strcmp(mount->path, target) == 0) {
            break;
        }
    }
    
    if (&mount->list == &mount_list) {
        shell_mutex_give(vfs_mutex);
        shell_log_error(TAG, "Mount point '%s' not found", target);
        return -1;
    }
    
    /* 检查是否有子挂载点 */
    if (!list_empty(&mount->children)) {
        shell_mutex_give(vfs_mutex);
        shell_log_error(TAG, "Cannot unmount '%s', submounts exist", target);
        return -1;
    }
    
    /* 从链表中移除 */
    list_del(&mount->list);
    if (mount->parent) {
        list_del(&mount->list);
    }
    
    /* 如果是根挂载点 */
    if (mount == root_mount) {
        root_mount = NULL;
    }
    
    shell_mutex_give(vfs_mutex);
    
    /* 销毁根节点 */
    vfs_node_destroy(mount->root);
    
    /* 释放挂载点结构 */
    shell_free(mount);
    
    shell_log_info(TAG, "Unmounted '%s'", target);
    return 0;
}

/**
 * @brief 查找V节点
 */
int vfs_lookup(const char *path, struct vfs_node **result)
{
    return path_resolve(path, result);
}

/**
 * @brief 创建文件
 */
int vfs_create(const char *path, mode_t mode, struct vfs_node **result)
{
    char *path_copy, *dir_path, *filename;
    struct vfs_node *parent;
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
    ret = path_resolve(dir_path, &parent);
    if (ret != 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查父目录是否是目录 */
    if (parent->type != VFS_NODE_TYPE_DIR) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查文件是否已存在 */
    if (vfs_node_lookup(parent, filename, result) == 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 创建文件节点 */
    *result = vfs_node_create(filename, parent, VFS_NODE_TYPE_FILE, parent->vfs);
    if (!*result) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 设置文件模式 */
    (*result)->mode = mode;
    
    /* 获取当前时间 */
    // TODO: 实现获取当前时间的函数
    (*result)->atime = 0;
    (*result)->mtime = 0;
    (*result)->ctime = 0;
    
    shell_free(path_copy);
    return 0;
}

/**
 * @brief 创建目录
 */
int vfs_mkdir(const char *path, mode_t mode)
{
    char *path_copy, *dir_path, *dirname;
    struct vfs_node *parent, *new_dir;
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
    ret = path_resolve(dir_path, &parent);
    if (ret != 0) {
        /* 如果是创建根目录下的子目录，且没有根挂载点，则创建一个根挂载点 */
        if (shell_strcmp(dir_path, "/") == 0) {
            /* 检查互斥锁是否已初始化 */
            if (!vfs_mutex) {
                shell_free(path_copy);
                return -1;
            }
            
            shell_mutex_take(vfs_mutex, SHELL_WAIT_FOREVER);
            
            /* 创建根挂载点 */
            struct mount_point *mount = shell_malloc(sizeof(struct mount_point));
            if (!mount) {
                shell_mutex_give(vfs_mutex);
                shell_free(path_copy);
                return -1;
            }
            
            /* 初始化挂载点 */
            shell_strncpy(mount->path, "/", MAX_PATH - 1);
            mount->path[MAX_PATH - 1] = '\0';
            mount->vfs = NULL;
            mount->parent = NULL;
            INIT_LIST_HEAD(&mount->children);
            
            /* 创建根节点 */
            mount->root = vfs_node_create("", NULL, VFS_NODE_TYPE_DIR, NULL);
            if (!mount->root) {
                shell_free(mount);
                shell_mutex_give(vfs_mutex);
                shell_free(path_copy);
                return -1;
            }
            
            /* 设置为根挂载点 */
            root_mount = mount;
            
            /* 添加到挂载点链表 */
            list_add_tail(&mount->list, &mount_list);
            
            shell_mutex_give(vfs_mutex);
            
            /* 使用新创建的根节点作为父目录 */
            parent = mount->root;
        } else {
            shell_free(path_copy);
            return -1;
        }
    }
    
    /* 检查父目录是否是目录 */
    if (parent->type != VFS_NODE_TYPE_DIR) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 检查目录是否已存在 */
    if (vfs_node_lookup(parent, dirname, &new_dir) == 0) {
        shell_free(path_copy);
        return -1;
    }
    
    /* 创建目录节点 */
    new_dir = vfs_node_create(dirname, parent, VFS_NODE_TYPE_DIR, parent->vfs);
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
 * @brief 删除文件
 */
int vfs_unlink(const char *path)
{
    struct vfs_node *node;
    int ret;
    
    if (!path) {
        return -1;
    }
    
    /* 查找节点 */
    ret = path_resolve(path, &node);
    if (ret != 0) {
        return -1;
    }
    
    /* 检查节点类型 */
    if (node->type != VFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    /* 销毁节点 */
    vfs_node_destroy(node);
    
    return 0;
}

/**
 * @brief 删除目录
 */
int vfs_rmdir(const char *path)
{
    struct vfs_node *node;
    int ret;
    
    if (!path) {
        return -1;
    }
    
    /* 查找节点 */
    ret = path_resolve(path, &node);
    if (ret != 0) {
        return -1;
    }
    
    /* 检查节点类型 */
    if (node->type != VFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 检查目录是否为空 */
    if (!list_empty(&node->children)) {
        return -1;
    }
    
    /* 销毁节点 */
    vfs_node_destroy(node);
    
    return 0;
}

/**
 * @brief 重命名文件或目录
 */
int vfs_rename(const char *oldpath, const char *newpath)
{
    struct vfs_node *old_node, *new_parent, *new_node;
    char *newpath_copy, *new_dir_path, *new_name;
    int ret;
    
    if (!oldpath || !newpath) {
        return -1;
    }
    
    /* 查找旧节点 */
    ret = path_resolve(oldpath, &old_node);
    if (ret != 0) {
        return -1;
    }
    
    /* 复制新路径字符串 */
    newpath_copy = shell_malloc(shell_strlen(newpath) + 1);
    if (!newpath_copy) {
        return -1;
    }
    shell_strncpy(newpath_copy, newpath, shell_strlen(newpath) + 1);
    
    /* 分离新目录路径和新名称 */
    new_name = newpath_copy + shell_strlen(newpath_copy) - 1;
    while (new_name >= newpath_copy && *new_name != '/') {
        new_name--;
    }
    if (new_name < newpath_copy) {
        shell_free(newpath_copy);
        return -1;
    }
    
    if (new_name == newpath_copy) {
        /* 根目录 */
        new_dir_path = "/";
        new_name++;
    } else {
        *new_name = '\0';
        new_dir_path = newpath_copy;
        new_name++;
    }
    
    /* 查找新父目录 */
    ret = path_resolve(new_dir_path, &new_parent);
    if (ret != 0) {
        shell_free(newpath_copy);
        return -1;
    }
    
    /* 检查新父目录是否是目录 */
    if (new_parent->type != VFS_NODE_TYPE_DIR) {
        shell_free(newpath_copy);
        return -1;
    }
    
    /* 检查新名称是否已存在 */
    if (vfs_node_lookup(new_parent, new_name, &new_node) == 0) {
        shell_free(newpath_copy);
        return -1;
    }
    
    /* 从旧父节点的子节点链表中移除 */
    if (old_node->parent) {
        list_del(&old_node->sibling);
    }
    
    /* 添加到新父节点的子节点链表 */
    old_node->parent = new_parent;
    list_add_tail(&old_node->sibling, &new_parent->children);
    
    /* 更新节点名称 */
    // 注意：这里假设节点名称是字符串常量，实际可能需要动态分配
    // 由于new_name指向newpath_copy的内部，我们需要复制它
    char *name_copy = shell_malloc(shell_strlen(new_name) + 1);
    if (!name_copy) {
        // 恢复到旧父节点
        old_node->parent = old_node->parent;
        list_add_tail(&old_node->sibling, &old_node->parent->children);
        shell_free(newpath_copy);
        return -1;
    }
    shell_strncpy(name_copy, new_name, shell_strlen(new_name) + 1);
    old_node->name = name_copy;
    
    /* 更新修改时间 */
    // TODO: 实现获取当前时间的函数
    old_node->mtime = 0;
    
    shell_free(newpath_copy);
    return 0;
}

/**
 * @brief 获取文件属性
 */
int vfs_stat(const char *path, struct stat *buf)
{
    struct vfs_node *node;
    int ret;
    
    if (!path || !buf) {
        return -1;
    }
    
    /* 查找节点 */
    ret = path_resolve(path, &node);
    if (ret != 0) {
        return -1;
    }
    
    /* 填充文件属性 */
    for (size_t i = 0; i < sizeof(struct stat); i++) {
        ((char *)buf)[i] = 0;
    }
    
    switch (node->type) {
    case VFS_NODE_TYPE_FILE:
        buf->st_mode = S_IFREG | node->mode;
        break;
    case VFS_NODE_TYPE_DIR:
        buf->st_mode = S_IFDIR | node->mode;
        break;
    case VFS_NODE_TYPE_LINK:
        buf->st_mode = S_IFLNK | node->mode;
        break;
    case VFS_NODE_TYPE_CHAR:
        buf->st_mode = S_IFCHR | node->mode;
        break;
    case VFS_NODE_TYPE_BLOCK:
        buf->st_mode = S_IFBLK | node->mode;
        break;
    case VFS_NODE_TYPE_FIFO:
        buf->st_mode = S_IFIFO | node->mode;
        break;
    case VFS_NODE_TYPE_SOCK:
        buf->st_mode = S_IFSOCK | node->mode;
        break;
    default:
        buf->st_mode = node->mode;
        break;
    }
    
    buf->st_uid = node->uid;
    buf->st_gid = node->gid;
    buf->st_size = node->size;
    buf->st_atime = node->atime;
    buf->st_mtime = node->mtime;
    buf->st_ctime = node->ctime;
    
    return 0;
}

/**
 * @brief 设置文件属性
 */
int vfs_chmod(const char *path, mode_t mode)
{
    struct vfs_node *node;
    int ret;
    
    if (!path) {
        return -1;
    }
    
    /* 查找节点 */
    ret = path_resolve(path, &node);
    if (ret != 0) {
        return -1;
    }
    
    /* 设置文件模式 */
    node->mode = mode;
    
    /* 更新修改时间 */
    // TODO: 实现获取当前时间的函数
    node->mtime = 0;
    
    return 0;
}

/**
 * @brief 打开文件
 */
int vfs_open(const char *path, int flags, mode_t mode, struct file **file)
{
    struct vfs_node *node;
    int ret;
    
    if (!path || !file) {
        return -1;
    }
    
    /* 查找节点 */
    ret = path_resolve(path, &node);
    if (ret != 0) {
        /* 如果文件不存在且设置了创建标志，则创建文件 */
        if (flags & O_CREAT) {
            ret = vfs_create(path, mode, &node);
            if (ret != 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    /* 检查节点类型 */
    if (node->type != VFS_NODE_TYPE_FILE) {
        return -1;
    }
    
    /* 分配文件结构 */
    *file = shell_malloc(sizeof(struct file));
    if (!*file) {
        return -1;
    }
    
    /* 初始化文件结构 */
    (*file)->node = node;
    (*file)->flags = flags;
    (*file)->pos = 0;
    (*file)->priv = NULL;
    
    /* 如果文件系统有打开操作，则调用 */
    if (node->vfs && node->vfs->ops && node->vfs->ops->open) {
        ret = node->vfs->ops->open(node, *file);
        if (ret != 0) {
            shell_free(*file);
            return ret;
        }
    }
    
    /* 更新访问时间 */
    // TODO: 实现获取当前时间的函数
    node->atime = 0;
    
    return 0;
}

/**
 * @brief 关闭文件
 */
int vfs_close(struct file *file)
{
    int ret = 0;
    
    if (!file) {
        return -1;
    }
    
    /* 如果文件系统有关闭操作，则调用 */
    if (file->node && file->node->vfs &&
        file->node->vfs->ops && file->node->vfs->ops->close) {
        ret = file->node->vfs->ops->close(file);
    }
    
    /* 释放文件结构 */
    shell_free(file);
    
    return ret;
}

/**
 * @brief 读取文件
 */
int vfs_read(struct file *file, void *buf, size_t len)
{
    int ret;
    
    if (!file || !buf || len == 0) {
        return -1;
    }
    
    /* 检查文件是否以读方式打开 */
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        return -1;
    }
    
    /* 如果文件系统有读取操作，则调用 */
    if (file->node && file->node->vfs &&
        file->node->vfs->ops && file->node->vfs->ops->read) {
        ret = file->node->vfs->ops->read(file, buf, len);
        if (ret > 0) {
            /* 更新文件位置 */
            file->pos += ret;
            
            /* 更新访问时间 */
            // TODO: 实现获取当前时间的函数
            file->node->atime = 0;
        }
        return ret;
    }
    
    return -1;
}

/**
 * @brief 写入文件
 */
int vfs_write(struct file *file, const void *buf, size_t len)
{
    int ret;
    
    if (!file || !buf || len == 0) {
        return -1;
    }
    
    /* 检查文件是否以写方式打开 */
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        return -1;
    }
    
    /* 如果文件系统有写入操作，则调用 */
    if (file->node && file->node->vfs &&
        file->node->vfs->ops && file->node->vfs->ops->write) {
        ret = file->node->vfs->ops->write(file, buf, len);
        if (ret > 0) {
            /* 更新文件位置 */
            file->pos += ret;
            
            /* 更新文件大小 */
            if (file->pos > file->node->size) {
                file->node->size = file->pos;
            }
            
            /* 更新修改时间 */
            // TODO: 实现获取当前时间的函数
            file->node->mtime = 0;
        }
        return ret;
    }
    
    return -1;
}

/**
 * @brief 定位文件指针
 */
int vfs_lseek(struct file *file, vfs_off_t offset, int whence)
{
    vfs_off_t new_pos;
    
    if (!file) {
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
        new_pos = file->node->size + offset;
        break;
    default:
        return -1;
    }
    
    /* 检查新位置是否有效 */
    if (new_pos < 0) {
        return -1;
    }
    
    /* 如果文件系统有定位操作，则调用 */
    if (file->node && file->node->vfs &&
        file->node->vfs->ops && file->node->vfs->ops->lseek) {
        int ret = file->node->vfs->ops->lseek(file, offset, whence);
        if (ret != 0) {
            return ret;
        }
    }
    
    /* 更新文件位置 */
    file->pos = new_pos;
    
    return 0;
}

/**
 * @brief 控制文件
 */
int vfs_ioctl(struct file *file, unsigned long cmd, unsigned long arg)
{
    if (!file) {
        return -1;
    }
    
    /* 如果文件系统有控制操作，则调用 */
    if (file->node && file->node->vfs &&
        file->node->vfs->ops && file->node->vfs->ops->ioctl) {
        return file->node->vfs->ops->ioctl(file, cmd, arg);
    }
    
    return -1;
}

/**
 * @brief 读取目录
 */
int vfs_readdir(struct file *file, struct dirent *dirent)
{
    struct vfs_node *child;
    int ret = 0;
    
    if (!file || !dirent) {
        return -1;
    }
    
    /* 检查文件是否是目录 */
    if (file->node->type != VFS_NODE_TYPE_DIR) {
        return -1;
    }
    
    /* 如果文件系统有读取目录操作，则调用 */
    if (file->node && file->node->vfs &&
        file->node->vfs->ops && file->node->vfs->ops->readdir) {
        ret = file->node->vfs->ops->readdir(file, dirent);
        if (ret == 0) {
            /* 更新文件位置 */
            file->pos++;
            
            /* 更新访问时间 */
            // TODO: 实现获取当前时间的函数
            file->node->atime = 0;
        }
        return ret;
    }
    
    /* 默认实现：遍历子节点 */
    child = NULL;
    list_for_each_entry(child, &file->node->children, sibling) {
        if (file->pos-- == 0) {
            break;
        }
    }
    
    if (&child->sibling == &file->node->children) {
        /* 已经到达目录末尾 */
        return -1;
    }
    
    /* 填充目录项 */
    dirent->d_ino = (ino_t)child;  /* 使用节点指针作为inode号 */
    dirent->d_off = file->pos;
    dirent->d_reclen = sizeof(struct dirent);
    
    switch (child->type) {
    case VFS_NODE_TYPE_FILE:
        dirent->d_type = DT_REG;
        break;
    case VFS_NODE_TYPE_DIR:
        dirent->d_type = DT_DIR;
        break;
    case VFS_NODE_TYPE_LINK:
        dirent->d_type = DT_LNK;
        break;
    case VFS_NODE_TYPE_CHAR:
        dirent->d_type = DT_CHR;
        break;
    case VFS_NODE_TYPE_BLOCK:
        dirent->d_type = DT_BLK;
        break;
    case VFS_NODE_TYPE_FIFO:
        dirent->d_type = DT_FIFO;
        break;
    case VFS_NODE_TYPE_SOCK:
        dirent->d_type = DT_SOCK;
        break;
    default:
        dirent->d_type = DT_UNKNOWN;
        break;
    }
    
    shell_strncpy(dirent->d_name, child->name, MAX_PATH - 1);
    dirent->d_name[MAX_PATH - 1] = '\0';
    
    /* 更新访问时间 */
    // TODO: 实现获取当前时间的函数
    file->node->atime = 0;
    
    return 0;
}
