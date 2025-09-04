#pragma once

#include "../list.h"
#include "../common.h"
#include "types.h"

/* 前向声明 */
struct vfs_node;
struct vfs;
struct mount_point;
struct file;
struct dirent;
struct stat;

/* VFS 操作函数结构体 */
struct vfs_ops {
    int (*open)(struct vfs_node *node, struct file *file);
    int (*close)(struct file *file);
    int (*read)(struct file *file, void *buf, size_t len);
    int (*write)(struct file *file, const void *buf, size_t len);
    int (*lseek)(struct file *file, vfs_off_t offset, int whence);
    int (*ioctl)(struct file *file, unsigned long cmd, unsigned long arg);
    int (*mkdir)(struct vfs_node *node, const char *name, mode_t mode);
    int (*rmdir)(struct vfs_node *node, const char *name);
    int (*lookup)(struct vfs_node *parent, const char *name, struct vfs_node **result);
    int (*create)(struct vfs_node *parent, const char *name, mode_t mode, struct vfs_node **result);
    int (*unlink)(struct vfs_node *parent, const char *name);
    int (*rename)(struct vfs_node *old_parent, const char *old_name,
                  struct vfs_node *new_parent, const char *new_name);
    int (*readdir)(struct file *file, struct dirent *dirent);
    int (*getattr)(struct vfs_node *node, struct stat *attr);
    int (*setattr)(struct vfs_node *node, const struct stat *attr);
};

/* VFS 节点类型 */
enum vfs_node_type {
    VFS_NODE_TYPE_UNKNOWN,
    VFS_NODE_TYPE_FILE,
    VFS_NODE_TYPE_DIR,
    VFS_NODE_TYPE_LINK,
    VFS_NODE_TYPE_CHAR,
    VFS_NODE_TYPE_BLOCK,
    VFS_NODE_TYPE_FIFO,
    VFS_NODE_TYPE_SOCK
};

/* VFS 节点结构体 */
struct vfs_node {
    struct list_head list;          /* 链表节点 */
    struct list_head children;       /* 子节点链表（如果是目录） */
    struct list_head sibling;       /* 兄弟节点链表 */
    struct vfs_node *parent;        /* 父节点 */
    struct vfs *vfs;                /* 所属的VFS */
    const char *name;               /* 节点名称 */
    enum vfs_node_type type;         /* 节点类型 */
    mode_t mode;                    /* 访问权限 */
    uid_t uid;                      /* 用户ID */
    gid_t gid;                      /* 组ID */
    vfs_off_t size;                     /* 文件大小 */
    time_t atime;                   /* 访问时间 */
    time_t mtime;                   /* 修改时间 */
    time_t ctime;                   /* 创建时间 */
    void *priv;                     /* 私有数据 */
};

/* VFS 挂载点结构体 */
struct mount_point {
    struct list_head list;          /* 链表节点 */
    char path[MAX_PATH];            /* 挂载路径 */
    struct vfs *vfs;                /* 挂载的VFS */
    struct vfs_node *root;          /* 根节点 */
    struct mount_point *parent;     /* 父挂载点 */
    struct list_head children;       /* 子挂载点链表 */
};

/* VFS 结构体 */
struct vfs {
    struct list_head list;          /* 链表节点 */
    char name[MAX_PATH];            /* VFS名称 */
    const struct vfs_ops *ops;      /* 操作函数 */
    void *priv;                     /* 私有数据 */
    struct mount_point *mount;      /* 挂载点 */
};

/* seek 操作的 whence 参数 */
#define SEEK_SET    0               /* 相对于文件开头 */
#define SEEK_CUR    1               /* 相对于当前位置 */
#define SEEK_END    2               /* 相对于文件结尾 */

/* 文件结构体 */
struct file {
    struct vfs_node *node;         /* 关联的VFS节点 */
    int flags;                      /* 文件标志 */
    vfs_off_t pos;                  /* 文件位置 */
    void *priv;                     /* 私有数据 */
};

/* 目录项结构体 */
struct dirent {
    ino_t d_ino;                    /* inode号 */
    vfs_off_t d_off;                /* 偏移量 */
    unsigned short d_reclen;        /* 记录长度 */
    unsigned char d_type;           /* 文件类型 */
    char d_name[MAX_PATH];          /* 文件名 */
};

/* 文件状态结构体 */
struct stat {
    dev_t st_dev;                   /* 设备ID */
    ino_t st_ino;                   /* inode号 */
    mode_t st_mode;                 /* 文件模式 */
    nlink_t st_nlink;               /* 硬链接数 */
    uid_t st_uid;                   /* 用户ID */
    gid_t st_gid;                   /* 组ID */
    dev_t st_rdev;                  /* 设备ID（如果是特殊文件） */
    vfs_off_t st_size;              /* 文件大小 */
    time_t st_atime;                /* 最后访问时间 */
    time_t st_mtime;                /* 最后修改时间 */
    time_t st_ctime;                /* 最后状态改变时间 */
};

/* VFS 初始化函数 */
int vfs_init(void);

/* VFS 挂载函数 */
int vfs_mount(const char *source, const char *target,
              const char *filesystemtype, unsigned long mountflags,
              const void *data);

/* VFS 卸载函数 */
int vfs_umount(const char *target);

/* VFS 查找节点函数 */
int vfs_lookup(const char *path, struct vfs_node **result);

/* VFS 创建文件函数 */
int vfs_create(const char *path, mode_t mode, struct vfs_node **result);

/* VFS 创建目录函数 */
int vfs_mkdir(const char *path, mode_t mode);

/* VFS 删除文件函数 */
int vfs_unlink(const char *path);

/* VFS 删除目录函数 */
int vfs_rmdir(const char *path);

/* VFS 重命名函数 */
int vfs_rename(const char *oldpath, const char *newpath);

/* VFS 获取文件属性函数 */
int vfs_stat(const char *path, struct stat *buf);

/* VFS 设置文件属性函数 */
int vfs_chmod(const char *path, mode_t mode);

/* VFS 打开文件函数 */
int vfs_open(const char *path, int flags, mode_t mode, struct file **file);

/* VFS 关闭文件函数 */
int vfs_close(struct file *file);

/* VFS 读取文件函数 */
int vfs_read(struct file *file, void *buf, size_t len);

/* VFS 写入文件函数 */
int vfs_write(struct file *file, const void *buf, size_t len);

/* VFS 定位文件指针函数 */
int vfs_lseek(struct file *file, vfs_off_t offset, int whence);

/* VFS 控制文件函数 */
int vfs_ioctl(struct file *file, unsigned long cmd, unsigned long arg);

/* VFS 读取目录函数 */
int vfs_readdir(struct file *file, struct dirent *dirent);

/* 注册VFS */
int vfs_register(struct vfs *vfs);

/* 注销VFS */
int vfs_unregister(struct vfs *vfs);