#pragma once

#include <stdint.h>
#include <stddef.h>
#include "device.h"
#include "driver.h"

/* TTY设备操作结构体 */
struct tty_operations {
    int (*open)(struct device *dev);
    void (*close)(struct device *dev);
    ssize_t (*read)(struct device *dev, void *buf, size_t count);
    ssize_t (*write)(struct device *dev, const void *buf, size_t count);
    int (*ioctl)(struct device *dev, unsigned int cmd, unsigned long arg);
    void (*set_termios)(struct device *dev, void *termios);
};

/* TTY设备结构体 */
struct tty_device {
    struct device dev;            /* 基础设备结构 */
    const char *name;            /* TTY设备名称 */
    int port_num;                /* 串口号 */
    uint32_t baudrate;           /* 波特率 */
    uint8_t data_bits;           /* 数据位 */
    uint8_t parity;              /* 校验位 */
    uint8_t stop_bits;           /* 停止位 */
    uint8_t flow_control;        /* 流控制 */
    void *private_data;          /* 私有数据 */
    const struct tty_operations *ops; /* 操作函数 */
};

/* ESP32 UART配置结构体 */
struct esp32_uart_config {
    int uart_num;                /* UART端口号 */
    int tx_pin;                  /* 发送引脚 */
    int rx_pin;                  /* 接收引脚 */
    int rts_pin;                 /* RTS引脚 */
    int cts_pin;                 /* CTS引脚 */
    uint32_t baudrate;           /* 波特率 */
    uint8_t data_bits;           /* 数据位 */
    uint8_t parity;              /* 校验位 */
    uint8_t stop_bits;           /* 停止位 */
    uint8_t flow_control;        /* 流控制 */
    uint32_t rx_buffer_size;     /* 接收缓冲区大小 */
    uint32_t tx_buffer_size;     /* 发送缓冲区大小 */
};

/* TTY设备管理函数 */
void tty_init(void);
int tty_register_device(struct tty_device *tty);
void tty_unregister_device(struct tty_device *tty);
struct tty_device *tty_find_device(const char *name);
struct tty_device *tty_find_device_by_port(int port_num);

/* ESP32 UART设备管理函数 */
int esp32_uart_register(struct esp32_uart_config *config, const char *name);
void esp32_uart_unregister(const char *name);
int esp32_uart_set_config(struct tty_device *tty, struct esp32_uart_config *config);

/* TTY设备操作函数 */
int tty_open(struct tty_device *tty);
void tty_close(struct tty_device *tty);
ssize_t tty_read(struct tty_device *tty, void *buf, size_t count);
ssize_t tty_write(struct tty_device *tty, const void *buf, size_t count);
int tty_ioctl(struct tty_device *tty, unsigned int cmd, unsigned long arg);

/* TTY设备遍历函数 */
void tty_for_each_device(void (*fn)(struct tty_device *tty, void *data), void *data);

/* TTY总线定义 */
extern struct bus_type tty_bus;