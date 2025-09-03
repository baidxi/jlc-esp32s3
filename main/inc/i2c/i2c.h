#pragma once

#include "device.h"
#include "driver.h"

/* I2C设备结构体 */
struct i2c_device {
    struct device dev;          /* 基础设备结构 */
    unsigned int addr;          /* I2C设备地址 */
    unsigned int flags;         /* 设备标志 */
};

/* I2C消息结构体 */
struct i2c_msg {
    unsigned int addr;          /* 从设备地址 */
    unsigned short flags;       /* 消息标志 */
    unsigned short len;         /* 消息长度 */
    unsigned char *buf;         /* 数据缓冲区 */
};

/* I2C适配器结构体 */
struct i2c_adapter {
    struct device dev;          /* 基础设备结构 */
    unsigned int adapter_class; /* 适配器类 */
    struct i2c_algorithm *algo; /* 算法指针 */
    void *algo_data;            /* 算法数据 */
    int timeout;                /* 超时时间(ms) */
    int retries;                /* 重试次数 */
};

/* I2C算法结构体 */
struct i2c_algorithm {
    int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
    int (*smbus_xfer)(struct i2c_adapter *adap, unsigned short addr,
                     unsigned short flags, char read_write,
                     unsigned char command, int size, void *data);
    unsigned int (*functionality)(struct i2c_adapter *adap);
};

/* I2C SMBus数据联合体 */
union i2c_smbus_data {
    unsigned char byte;
    unsigned short word;
    unsigned char block[34];
};

/* I2C消息标志 */
#define I2C_M_RD              0x0001  /* 读消息 */
#define I2C_M_TEN             0x0010  /* 10位地址 */
#define I2C_M_RECV_LEN        0x0400  /* 长度将首先被接收 */
#define I2C_M_NO_RD_ACK       0x0800  /* 如果是读命令，忽略ACK */
#define I2C_M_IGNORE_NAK      0x1000  /* 忽略NAK */
#define I2C_M_REV_DIR_ADDR    0x2000  /* 反转方向位 */
#define I2C_M_NOSTART         0x4000  /* 不发送起始位 */

/* I2C功能标志 */
#define I2C_FUNC_I2C                  0x00000001
#define I2C_FUNC_10BIT_ADDR           0x00000002
#define I2C_FUNC_PROTOCOL_MANGLING    0x00000004
#define I2C_FUNC_SMBUS_PEC            0x00000008
#define I2C_FUNC_NOSTART              0x00000010
#define I2C_FUNC_SLAVE                0x00000020

struct i2c_device_table {
    const char *compatible;
    uint32_t data;
};

/* I2C驱动结构体 */
struct i2c_driver {
    struct device_driver driver;   /* 基础驱动结构 */
    const struct i2c_device_table *match_ptr;
    int (*probe)(struct i2c_device *dev);
    int (*remove)(struct i2c_device *dev);
    void (*shutdown)(struct i2c_device *dev);
};

/* I2C总线类型 */
// extern struct bus_type i2c_bus_type;

/* I2C适配器函数 */
int i2c_register_adapter(struct i2c_adapter *adapter);
void i2c_unregister_adapter(struct i2c_adapter *adapter);

/* I2C设备函数 */
int i2c_register_device(struct i2c_device *dev);
void i2c_unregister_device(struct i2c_device *dev);

/* I2C驱动函数 */
int i2c_register_driver(struct i2c_driver *driver);
void i2c_unregister_driver(struct i2c_driver *driver);

/* I2C通信函数 */
int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);

/* I2C辅助函数 */
struct i2c_device *i2c_verify_device(struct device *dev);
struct i2c_adapter *i2c_verify_adapter(struct device *dev);
struct i2c_driver *i2c_verify_driver(struct device_driver *drv);

/* 初始化I2C子系统 */
int i2c_init_subsystem(void);

/* 从i2c_device获取device结构体 */
#define to_i2c_device(d) container_of(d, struct i2c_device, dev)

/* 从i2c_adapter获取device结构体 */
#define to_i2c_adapter(d) container_of(d, struct i2c_adapter, dev)

/* 从i2c_driver获取device_driver结构体 */
#define to_i2c_driver(d) container_of(d, struct i2c_driver, driver)