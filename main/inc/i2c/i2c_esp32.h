/*
 * ESP32 I2C 驱动头文件
 * 基于 ESP32 IDF V5.5 SDK
 *
 * 本文件定义了 ESP32 I2C 驱动的接口和数据结构
 */

#pragma once

#include "i2c.h"

/* ESP32 IDF V5.5 SDK 头文件 */
#include "driver/i2c_master.h"

/* ESP32 I2C master配置结构体 */
struct i2c_esp32_config {
    int sda_io_num;     /* SDA引脚号 */
    int scl_io_num;     /* SCL引脚号 */
    unsigned int freq;   /* I2C频率(Hz) */
    int sda_pullup_en;  /* SDA上拉使能 */
    int scl_pullup_en;  /* SCL上拉使能 */
};

/* ESP32 I2C master适配器结构体 */
struct i2c_esp32_adapter {
    struct i2c_adapter adapter;  /* 基础I2C适配器 */
    struct i2c_esp32_config config;  /* 配置 */
    int port_num;             /* I2C端口号 */
    int initialized;          /* 初始化标志 */
    
    /* ESP32 IDF 特定字段 */
    i2c_master_bus_handle_t bus_handle;  /* I2C总线句柄 */
    i2c_master_dev_handle_t dev_handle; /* I2C设备句柄 */
};

/* ESP32 I2C master算法结构体 */
extern struct i2c_algorithm i2c_esp32_algorithm;

/* ESP32 I2C master函数 */
int i2c_esp32_init(struct i2c_esp32_adapter *adap, struct i2c_esp32_config *config);
void i2c_esp32_deinit(struct i2c_esp32_adapter *adap);
int i2c_esp32_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
unsigned int i2c_esp32_functionality(struct i2c_adapter *adap);

/* 创建ESP32 I2C master适配器 */
struct i2c_esp32_adapter *i2c_esp32_create_adapter(int port_num, int sda_io_num, int scl_io_num, unsigned int freq);

/* 销毁ESP32 I2C master适配器 */
void i2c_esp32_destroy_adapter(struct i2c_esp32_adapter *adap);

/* ESP32 特定的 I2C 功能支持 */

/* 扫描I2C总线上的设备 */
int i2c_esp32_scan_devices(struct i2c_esp32_adapter *adap, uint8_t *found_devices, int max_devices);

/* 获取I2C总线状态 */
int i2c_esp32_get_bus_status(struct i2c_esp32_adapter *adap);

/* 重置I2C总线 */
int i2c_esp32_reset_bus(struct i2c_esp32_adapter *adap);

/* 设置I2C总线频率 */
int i2c_esp32_set_frequency(struct i2c_esp32_adapter *adap, uint32_t freq);