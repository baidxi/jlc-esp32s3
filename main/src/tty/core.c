#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"

#include "tty/tty.h"

static const char *TAG = "tty_core";

/* TTY设备列表 */
static struct list_head tty_device_list = {0};

/* TTY总线定义 */
struct bus_type tty_bus = {
    .name = "tty",
    .dev_name = "tty",
};

/* TTY驱动定义 */
static struct device_driver tty_driver = {
    .name = "tty_driver",
    .bus = &tty_bus,
};

/**
 * @brief 从设备结构体获取TTY设备结构体
 */
static inline struct tty_device *to_tty_device(struct device *dev)
{
    return container_of(dev, struct tty_device, dev);
}

/**
 * @brief TTY设备驱动的probe函数
 */
static int tty_driver_probe(struct device *dev)
{
    struct tty_device *tty = to_tty_device(dev);
    
    if (!tty || !tty->ops) {
        return -EINVAL;
    }
    
    printf("[TTY] TTY device %s probed\n", tty->name);
    
    /* 如果有open函数，调用它 */
    if (tty->ops->open) {
        return tty->ops->open(dev);
    }
    
    return 0;
}

/**
 * @brief TTY设备驱动的remove函数
 */
static int tty_driver_remove(struct device *dev)
{
    struct tty_device *tty = to_tty_device(dev);
    
    if (!tty || !tty->ops) {
        return -EINVAL;
    }
    
    printf("[TTY] TTY device %s removed\n", tty->name);
    
    /* 如果有close函数，调用它 */
    if (tty->ops->close) {
        tty->ops->close(dev);
    }
    
    return 0;
}

/**
 * @brief 初始化TTY框架
 */
void tty_init(void)
{
    /* 初始化TTY设备列表 */
    INIT_LIST_HEAD(&tty_device_list);
    
    /* 注册TTY总线 */
    bus_register(&tty_bus);
    
    /* 设置TTY驱动的probe和remove函数 */
    tty_driver.probe = tty_driver_probe;
    tty_driver.remove = tty_driver_remove;
    
    /* 注册TTY驱动 */
    driver_register(&tty_driver);
    
    printf("[TTY] TTY framework initialized\n");
}

/**
 * @brief 注册TTY设备
 */
int tty_register_device(struct tty_device *tty)
{
    int ret;
    
    if (!tty || !tty->name || !tty->ops) {
        return -EINVAL;
    }
    
    /* 检查设备是否已经注册 */
    if (tty_find_device(tty->name)) {
        printf("[TTY] ERROR: TTY device %s already registered\n", tty->name);
        return -EEXIST;
    }
    
    /* 初始化设备结构 */
    tty->dev.init_name = tty->name;
    tty->dev.bus = &tty_bus;
    INIT_LIST_HEAD(&tty->dev.list);
    
    /* 注册设备 */
    ret = device_register(&tty->dev);
    if (ret) {
        printf("[TTY] ERROR: Failed to register TTY device %s\n", tty->name);
        return ret;
    }
    
    /* 添加到TTY设备列表 */
    list_add_tail(&tty->dev.list, &tty_device_list);
    
    printf("[TTY] TTY device %s registered\n", tty->name);
    
    return 0;
}

/**
 * @brief 注销TTY设备
 */
void tty_unregister_device(struct tty_device *tty)
{
    if (!tty) {
        return;
    }
    
    printf("[TTY] Unregistering TTY device %s\n", tty->name);
    
    /* 从TTY设备列表中移除 */
    list_del(&tty->dev.list);
    
    /* 注销设备 */
    device_unregister(&tty->dev);
}

/**
 * @brief 根据名称查找TTY设备
 */
struct tty_device *tty_find_device(const char *name)
{
    struct tty_device *tty;
    
    list_for_each_entry(tty, &tty_device_list, dev.list) {
        if (!strcmp(tty->name, name)) {
            return tty;
        }
    }
    
    return NULL;
}

/**
 * @brief 根据串口号查找TTY设备
 */
struct tty_device *tty_find_device_by_port(int port_num)
{
    struct tty_device *tty;
    
    list_for_each_entry(tty, &tty_device_list, dev.list) {
        if (tty->port_num == port_num) {
            return tty;
        }
    }
    
    return NULL;
}

/**
 * @brief 遍历所有TTY设备
 */
void tty_for_each_device(void (*fn)(struct tty_device *tty, void *data), void *data)
{
    struct tty_device *tty;
    
    if (!fn) {
        return;
    }
    
    list_for_each_entry(tty, &tty_device_list, dev.list) {
        fn(tty, data);
    }
}

/**
 * @brief 打开TTY设备
 */
int tty_open(struct tty_device *tty)
{
    if (!tty || !tty->ops || !tty->ops->open) {
        return -EINVAL;
    }
    
    return tty->ops->open(&tty->dev);
}

/**
 * @brief 关闭TTY设备
 */
void tty_close(struct tty_device *tty)
{
    if (!tty || !tty->ops || !tty->ops->close) {
        return;
    }
    
    tty->ops->close(&tty->dev);
}

/**
 * @brief 从TTY设备读取数据
 */
ssize_t tty_read(struct tty_device *tty, void *buf, size_t count)
{
    if (!tty || !tty->ops || !tty->ops->read || !buf || count == 0) {
        return -EINVAL;
    }
    
    return tty->ops->read(&tty->dev, buf, count);
}

/**
 * @brief 向TTY设备写入数据
 */
ssize_t tty_write(struct tty_device *tty, const void *buf, size_t count)
{
    if (!tty || !tty->ops || !tty->ops->write || !buf || count == 0) {
        return -EINVAL;
    }
    
    return tty->ops->write(&tty->dev, buf, count);
}

/**
 * @brief TTY设备IO控制
 */
int tty_ioctl(struct tty_device *tty, unsigned int cmd, unsigned long arg)
{
    if (!tty || !tty->ops || !tty->ops->ioctl) {
        return -EINVAL;
    }
    
    return tty->ops->ioctl(&tty->dev, cmd, arg);
}

/**
 * @brief ESP32 UART设备操作函数
 */
static int esp32_uart_open(struct device *dev)
{
    struct tty_device *tty = to_tty_device(dev);
    
    if (!tty) {
        return -EINVAL;
    }
    
    printf("[TTY] Opening ESP32 UART device %s\n", tty->name);
    
    /* UART设备已在注册时初始化，这里不需要额外操作 */
    return 0;
}

static void esp32_uart_close(struct device *dev)
{
    struct tty_device *tty = to_tty_device(dev);
    
    if (!tty) {
        return;
    }
    
    printf("[TTY] Closing ESP32 UART device %s\n", tty->name);
    
    /* 这里可以添加关闭UART设备的代码 */
}

static ssize_t esp32_uart_read(struct device *dev, void *buf, size_t count)
{
    struct tty_device *tty = to_tty_device(dev);
    int uart_num;
    int len;
    
    if (!tty || !buf) {
        return -EINVAL;
    }
    
    uart_num = tty->port_num;
    
    /* 检查UART缓冲区中是否有数据 */
    size_t buffered_size = 0;
    uart_get_buffered_data_len(uart_num, &buffered_size);
    
    if (buffered_size == 0) {
        return 0;  /* 没有数据可读 */
    }
    
    /* 从UART读取数据，使用非阻塞方式 */
    len = uart_read_bytes(uart_num, buf, count, 0);
    
    if (len < 0) {
        printf("[TTY] ERROR: Failed to read from UART %d\n", uart_num);
        return len;
    }
 
    /* 调试日志：显示读取到的数据 */
       /*
    if (len > 0) {
        printf("[TTY] Read %d bytes from UART %d: ", len, uart_num);
        for (int i = 0; i < len; i++) {
            unsigned char ch = ((unsigned char *)buf)[i];
            if (ch >= 32 && ch < 127) {
                printf("'%c' (0x%02x) ", ch, ch);
            } else {
                printf("0x%02x ", ch);
            }
        }
        printf("\n");
    }
    */
    
    return len;
}

static ssize_t esp32_uart_write(struct device *dev, const void *buf, size_t count)
{
    struct tty_device *tty = to_tty_device(dev);
    int uart_num;
    int len;
    
    if (!tty || !buf) {
        return -EINVAL;
    }
    
    uart_num = tty->port_num;
    
    /* 向UART写入数据 */
    len = uart_write_bytes(uart_num, buf, count);
    
    if (len < 0) {
        printf("[TTY] ERROR: Failed to write to UART %d\n", uart_num);
        return len;
    }
    
    /* 等待数据发送完成 */
    uart_wait_tx_done(uart_num, pdMS_TO_TICKS(100));
    
    return len;
}

static int esp32_uart_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
    struct tty_device *tty = to_tty_device(dev);
    
    if (!tty) {
        return -EINVAL;
    }
    
    /* 这里可以添加IO控制代码 */
    return 0;
}

/* ESP32 UART操作结构体 */
static const struct tty_operations esp32_uart_ops = {
    .open = esp32_uart_open,
    .close = esp32_uart_close,
    .read = esp32_uart_read,
    .write = esp32_uart_write,
    .ioctl = esp32_uart_ioctl,
};

/**
 * @brief 注册ESP32 UART设备
 */
int esp32_uart_register(struct esp32_uart_config *config, const char *name)
{
    struct tty_device *tty;
    uart_config_t uart_config = {0};
    uart_config.baud_rate = config->baudrate;
    uart_config.data_bits = config->data_bits;
    uart_config.parity = config->parity;
    uart_config.stop_bits = config->stop_bits;
    uart_config.flow_ctrl = config->flow_control;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    int ret;
    
    if (!config || !name) {
        return -EINVAL;
    }
    
    /* 检查设备是否已经注册 */
    if (tty_find_device(name)) {
        printf("[TTY] ERROR: TTY device %s already registered\n", name);
        return -EEXIST;
    }
    
    /* 分配TTY设备结构体 */
    tty = (struct tty_device *)malloc(sizeof(struct tty_device));
    if (!tty) {
        return -ENOMEM;
    }
    
    memset(tty, 0, sizeof(struct tty_device));
    
    /* 初始化UART */
    ret = uart_driver_install(config->uart_num, config->rx_buffer_size,
                              config->tx_buffer_size, 0, NULL, 0);
    if (ret != ESP_OK) {
        printf("[TTY] ERROR: Failed to install UART driver for %s\n", name);
        free(tty);
        return ret;
    }
    
    ret = uart_param_config(config->uart_num, &uart_config);
    if (ret != ESP_OK) {
        printf("[TTY] ERROR: Failed to config UART for %s\n", name);
        uart_driver_delete(config->uart_num);
        free(tty);
        return ret;
    }
    
    /* 设置UART引脚 */
    ret = uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin,
                       config->rts_pin, config->cts_pin);
    if (ret != ESP_OK) {
        printf("[TTY] ERROR: Failed to set UART pins for %s\n", name);
        uart_driver_delete(config->uart_num);
        free(tty);
        return ret;
    }
    
    /* 初始化TTY设备结构体 */
    tty->name = name;
    tty->port_num = config->uart_num;
    tty->baudrate = config->baudrate;
    tty->data_bits = config->data_bits;
    tty->parity = config->parity;
    tty->stop_bits = config->stop_bits;
    tty->flow_control = config->flow_control;
    tty->ops = &esp32_uart_ops;
    
    /* 注册TTY设备 */
    ret = tty_register_device(tty);
    if (ret) {
        printf("[TTY] ERROR: Failed to register TTY device %s\n", name);
        uart_driver_delete(config->uart_num);
        free(tty);
        return ret;
    }
    
    printf("[TTY] ESP32 UART device %s registered\n", name);
    
    return 0;
}

/**
 * @brief 注销ESP32 UART设备
 */
void esp32_uart_unregister(const char *name)
{
    struct tty_device *tty;
    
    if (!name) {
        return;
    }
    
    tty = tty_find_device(name);
    if (!tty) {
        printf("[TTY] ERROR: TTY device %s not found\n", name);
        return;
    }
    
    printf("[TTY] Unregistering ESP32 UART device %s\n", name);
    
    /* 删除UART驱动 */
    uart_driver_delete(tty->port_num);
    
    /* 注销TTY设备 */
    tty_unregister_device(tty);
    
    /* 释放TTY设备结构体 */
    free(tty);
}

/**
 * @brief 设置ESP32 UART配置
 */
int esp32_uart_set_config(struct tty_device *tty, struct esp32_uart_config *config)
{
    uart_config_t uart_config = {0};
    uart_config.baud_rate = config->baudrate;
    uart_config.data_bits = config->data_bits;
    uart_config.parity = config->parity;
    uart_config.stop_bits = config->stop_bits;
    uart_config.flow_ctrl = config->flow_control;
    int ret;
    
    if (!tty || !config) {
        return -EINVAL;
    }
    
    /* 更新UART配置 */
    ret = uart_param_config(tty->port_num, &uart_config);
    if (ret != ESP_OK) {
        printf("[TTY] ERROR: Failed to config UART for %s\n", tty->name);
        return ret;
    }
    
    /* 更新TTY设备配置 */
    tty->baudrate = config->baudrate;
    tty->data_bits = config->data_bits;
    tty->parity = config->parity;
    tty->stop_bits = config->stop_bits;
    tty->flow_control = config->flow_control;
    
    printf("[TTY] UART config updated for %s\n", tty->name);
    
    return 0;
}

