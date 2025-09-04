#pragma once

#include <stddef.h>
#include <stdint.h>

/* 基本类型定义 */
typedef int32_t mode_t;     /* 文件模式类型 */
typedef uint32_t uid_t;     /* 用户ID类型 */
typedef uint32_t gid_t;     /* 组ID类型 */

/* 使用自定义的 vfs_off_t 避免与系统 off_t 冲突 */
typedef int64_t vfs_off_t;  /* 文件偏移量类型 */

typedef int64_t time_t;     /* 时间类型 */
typedef uint32_t ino_t;     /* inode号类型 */
typedef uint32_t dev_t;     /* 设备ID类型 */
typedef uint32_t nlink_t;   /* 硬链接数类型 */

/* 文件权限标志 */
#define S_IRUSR  0000400    /* 用户读权限 */
#define S_IWUSR  0000200    /* 用户写权限 */
#define S_IXUSR  0000100    /* 用户执行权限 */
#define S_IRGRP  0000040    /* 组读权限 */
#define S_IWGRP  0000020    /* 组写权限 */
#define S_IXGRP  0000010    /* 组执行权限 */
#define S_IROTH  0000004    /* 其他读权限 */
#define S_IWOTH  0000002    /* 其他写权限 */
#define S_IXOTH  0000001    /* 其他执行权限 */

#define S_ISUID  0004000    /* 设置用户ID */
#define S_ISGID  0002000    /* 设置组ID */
#define S_ISVTX  0001000    /* 粘着位 */

/* 文件类型标志 */
#define S_IFMT   0170000    /* 文件类型掩码 */
#define S_IFDIR  0040000    /* 目录 */
#define S_IFCHR  0020000    /* 字符设备 */
#define S_IFBLK  0060000    /* 块设备 */
#define S_IFREG  0100000    /* 常规文件 */
#define S_IFLNK  0120000    /* 符号链接 */
#define S_IFSOCK 0140000    /* 套接字 */
#define S_IFIFO  0010000    /* FIFO */

/* 文件类型检查宏 */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)

/* 默认权限 */
#define S_IRWXU  (S_IRUSR | S_IWUSR | S_IXUSR)  /* 用户读写执行 */
#define S_IRWXG  (S_IRGRP | S_IWGRP | S_IXGRP)  /* 组读写执行 */
#define S_IRWXO  (S_IROTH | S_IWOTH | S_IXOTH)  /* 其他读写执行 */

#define S_IRWXA  (S_IRWXU | S_IRWXG | S_IRWXO)   /* 所有用户读写执行 */

#define S_DEFAULT_FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)  /* 默认文件权限 */
#define S_DEFAULT_DIR_MODE  (S_DEFAULT_FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)  /* 默认目录权限 */

/* 文件访问模式 */
#define O_ACCMODE   00000003  /* 文件访问模式掩码 */
#define O_RDONLY    00000000  /* 只读打开 */
#define O_WRONLY    00000001  /* 只写打开 */
#define O_RDWR      00000002  /* 读写打开 */
#define O_CREAT     00000100  /* 若文件不存在则创建 */
#define O_EXCL      00000200  /* 独占方式创建 */
#define O_TRUNC     00001000  /* 文件长度截断为0 */
#define O_APPEND    00002000  /* 追加方式 */

/* 目录项类型 */
#define DT_UNKNOWN   0
#define DT_FIFO      1
#define DT_CHR       2
#define DT_DIR       4
#define DT_BLK       6
#define DT_REG       8
#define DT_LNK      10
#define DT_SOCK     12