/*
 * ESP32 I2C 驱动实现
 * 基于 ESP32 IDF V5.5 SDK
 *
 * 本文件实现了 ESP32 的 I2C 主机驱动功能，包括：
 * - I2C 总线初始化和配置
 * - I2C 数据传输（读/写）
 * - I2C 设备扫描
 * - 错误处理和调试功能
 */

#include "i2c/i2c_esp32.h"
#include "common.h"
#include "i2c/i2c.h"
#include "device.h"
#include <stdlib.h>
#include <string.h>

/* ESP32 IDF V5.5 SDK 头文件 */
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/* 定义日志标签 */
static const char *TAG = "i2c_esp32";

/* 错误处理函数 */
static const char *i2c_esp32_error_to_string(int error_code)
{
    switch (error_code) {
        case ESP_ERR_INVALID_ARG:
            return "Invalid argument";
        case ESP_ERR_INVALID_STATE:
            return "Invalid state";
        case ESP_ERR_TIMEOUT:
            return "Timeout";
        case ESP_ERR_NOT_FOUND:
            return "Device not found";
        case ESP_ERR_NO_MEM:
            return "Out of memory";
        default:
            return "Unknown error";
    }
}

/* 调试辅助函数 */
static void i2c_esp32_dump_message(struct i2c_msg *msg)
{
    if (!msg) {
        return;
    }
    
    ESP_LOGD(TAG, "  Address: 0x%02x", msg->addr);
    ESP_LOGD(TAG, "  Flags: 0x%02x", msg->flags);
    ESP_LOGD(TAG, "  Length: %d", msg->len);
    
    if (msg->len > 0 && msg->buf) {
        char hex_str[64] = {0};
        int i, max_len = (msg->len > 8) ? 8 : msg->len;
        
        for (i = 0; i < max_len; i++) {
            snprintf(hex_str + i * 3, sizeof(hex_str) - i * 3, "%02x ", msg->buf[i]);
        }
        
        if (msg->len > 8) {
            strcat(hex_str, "...");
        }
        
        ESP_LOGD(TAG, "  Data: %s", hex_str);
    }
}

/* ESP32 I2C master算法实现 */
static int i2c_esp32_master_xfer_impl(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
    struct i2c_esp32_adapter *esp32_adap = (struct i2c_esp32_adapter *)adap->algo_data;
    int i;
    esp_err_t ret;
    
    if (!adap || !msgs || num <= 0 || !esp32_adap || !esp32_adap->initialized) {
        ESP_LOGE(TAG, "Invalid parameters for I2C transfer");
        return -1;
    }
    
    ESP_LOGI(TAG, "Starting transfer of %d messages on I2C port %d", num, esp32_adap->port_num);
    
    for (i = 0; i < num; i++) {
        ESP_LOGD(TAG, "Processing message %d:", i);
        i2c_esp32_dump_message(&msgs[i]);
        
        /* 从消息中获取设备地址 */
        uint8_t addr = msgs[i].addr;
        
        if (msgs[i].flags & I2C_M_RD) {
            ESP_LOGD(TAG, "Reading %d bytes from device 0x%02x", msgs[i].len, addr);
            
            /* 使用ESP32 IDF V5.5新版I2C驱动API进行读取 */
            /* 首先需要创建设备句柄 */
            i2c_master_dev_handle_t dev_handle;
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = addr,
                .scl_speed_hz = esp32_adap->config.freq,
            };
            
            ret = i2c_master_bus_add_device(esp32_adap->bus_handle, &dev_cfg, &dev_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
                return -1;
            }
            
            /* 执行读取操作 */
            ret = i2c_master_receive(dev_handle, msgs[i].buf, msgs[i].len, esp32_adap->adapter.timeout / portTICK_PERIOD_MS);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(ret));
                ESP_LOGE(TAG, "Error details: %s", i2c_esp32_error_to_string(ret));
                i2c_master_bus_rm_device(dev_handle);
                return -1;
            }
            
            /* 移除设备句柄 */
            i2c_master_bus_rm_device(dev_handle);
            
            /* 显示读取的数据 */
            if (msgs[i].len > 0 && msgs[i].buf) {
                ESP_LOGD(TAG, "Read data:");
                i2c_esp32_dump_message(&msgs[i]);
            }
        } else {
            ESP_LOGD(TAG, "Writing %d bytes to device 0x%02x", msgs[i].len, addr);
            
            /* 使用ESP32 IDF V5.5新版I2C驱动API进行写入 */
            /* 首先需要创建设备句柄 */
            i2c_master_dev_handle_t dev_handle;
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = addr,
                .scl_speed_hz = esp32_adap->config.freq,
            };
            
            ret = i2c_master_bus_add_device(esp32_adap->bus_handle, &dev_cfg, &dev_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
                return -1;
            }
            
            /* 执行写入操作 */
            ret = i2c_master_transmit(dev_handle, msgs[i].buf, msgs[i].len, esp32_adap->adapter.timeout / portTICK_PERIOD_MS);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(ret));
                ESP_LOGE(TAG, "Error details: %s", i2c_esp32_error_to_string(ret));
                i2c_master_bus_rm_device(dev_handle);
                return -1;
            }
            
            /* 移除设备句柄 */
            i2c_master_bus_rm_device(dev_handle);
        }
    }
    
    ESP_LOGI(TAG, "Transfer completed successfully");
    return num;  /* 返回成功处理的消息数 */
}

static unsigned int i2c_esp32_functionality_impl(struct i2c_adapter *adap)
{
    return I2C_FUNC_I2C | I2C_FUNC_10BIT_ADDR | I2C_FUNC_PROTOCOL_MANGLING;
}

/* ESP32 I2C master算法结构体 */
struct i2c_algorithm i2c_esp32_algorithm = {
    .master_xfer = i2c_esp32_master_xfer_impl,
    .functionality = i2c_esp32_functionality_impl,
};

/* 初始化ESP32 I2C master适配器 */
int i2c_esp32_init(struct i2c_esp32_adapter *adap, struct i2c_esp32_config *config)
{
    i2c_master_bus_config_t i2c_bus_config = {0};
    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret;
    
    if (!adap || !config) {
        ESP_LOGE(TAG, "Invalid parameters for I2C initialization");
        return -1;
    }
    
    /* 保存配置 */
    memcpy(&adap->config, config, sizeof(struct i2c_esp32_config));
    
    /* 初始化I2C适配器 */
    adap->adapter.dev.init_name = "ESP32 I2C Master";
    adap->adapter.algo = &i2c_esp32_algorithm;
    adap->adapter.algo_data = adap;
    adap->adapter.timeout = 1000;  /* 1秒超时 */
    adap->adapter.retries = 3;     /* 重试3次 */
    adap->adapter.adapter_class = 0;
    
    ESP_LOGI(TAG, "Initializing ESP32 I2C master on port %d", adap->port_num);
    ESP_LOGI(TAG, "SDA: GPIO%d, SCL: GPIO%d, Frequency: %dHz",
           config->sda_io_num, config->scl_io_num, config->freq);
    
    /* 配置I2C总线 */
    i2c_bus_config.i2c_port = adap->port_num;
    i2c_bus_config.sda_io_num = config->sda_io_num;
    i2c_bus_config.scl_io_num = config->scl_io_num;
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.glitch_ignore_cnt = 7;
    
    /* 配置总线频率 */
    i2c_bus_config.trans_queue_depth = 0;  /* 使用默认值 */
    
    /* 配置上拉电阻 */
    if (config->sda_pullup_en) {
        i2c_bus_config.flags.enable_internal_pullup = true;
    } else {
        i2c_bus_config.flags.enable_internal_pullup = false;
    }
    
    /* 安装I2C主机驱动 */
    ret = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C master driver: %s", esp_err_to_name(ret));
        return -1;
    }
    
    /* 保存总线句柄 */
    adap->bus_handle = bus_handle;
    
    /* 不再在这里创建设备句柄，而是在设备注册时创建 */
    
    adap->initialized = 1;
    
    ESP_LOGI(TAG, "I2C master initialized successfully");
    return 0;
}

/* 反初始化ESP32 I2C master适配器 */
void i2c_esp32_deinit(struct i2c_esp32_adapter *adap)
{
    if (!adap || !adap->initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing ESP32 I2C master on port %d", adap->port_num);
    
    /* 设备句柄现在由每个I2C设备自己管理，不需要在这里删除 */
    
    /* 删除I2C主机驱动 */
    if (adap->bus_handle) {
        i2c_del_master_bus(adap->bus_handle);
        adap->bus_handle = NULL;
    }
    
    adap->initialized = 0;
    
    ESP_LOGI(TAG, "I2C master deinitialized successfully");
}

/* ESP32 I2C master传输函数 */
int i2c_esp32_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
    struct i2c_esp32_adapter *esp32_adap = (struct i2c_esp32_adapter *)adap->algo_data;
    
    if (!esp32_adap || !esp32_adap->initialized) {
        return -1;
    }
    
    return i2c_esp32_master_xfer_impl(adap, msgs, num);
}

/* ESP32 I2C master功能函数 */
unsigned int i2c_esp32_functionality(struct i2c_adapter *adap)
{
    return i2c_esp32_functionality_impl(adap);
}

/* 创建ESP32 I2C master适配器 */
struct i2c_esp32_adapter *i2c_esp32_create_adapter(int port_num, int sda_io_num, int scl_io_num, unsigned int freq)
{
    struct i2c_esp32_adapter *adap;
    struct i2c_esp32_config config;
    
    /* 分配内存 */
    adap = (struct i2c_esp32_adapter *)malloc(sizeof(struct i2c_esp32_adapter));
    if (!adap) {
        return NULL;
    }
    
    /* 清零结构体 */
    memset(adap, 0, sizeof(struct i2c_esp32_adapter));
    
    /* 设置配置 */
    adap->port_num = port_num;
    config.sda_io_num = sda_io_num;
    config.scl_io_num = scl_io_num;
    config.freq = freq;
    config.sda_pullup_en = 1;  /* 默认使能上拉 */
    config.scl_pullup_en = 1;  /* 默认使能上拉 */
    
    /* 初始化适配器 */
    if (i2c_esp32_init(adap, &config) != 0) {
        free(adap);
        return NULL;
    }
    
    return adap;
}

/* 销毁ESP32 I2C master适配器 */
void i2c_esp32_destroy_adapter(struct i2c_esp32_adapter *adap)
{
    if (!adap) {
        return;
    }
    
    /* 反初始化适配器 */
    i2c_esp32_deinit(adap);
    
    /* 释放内存 */
    free(adap);
}

/* ESP32 特定的 I2C 功能支持 */

/* 扫描I2C总线上的设备 */
int i2c_esp32_scan_devices(struct i2c_esp32_adapter *adap, uint8_t *found_devices, int max_devices)
{
    uint8_t addr;
    int found_count = 0;
    esp_err_t ret;
    
    if (!adap || !adap->initialized || !found_devices || max_devices <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for I2C scan");
        return -1;
    }
    
    ESP_LOGI(TAG, "Scanning I2C bus on port %d", adap->port_num);
    
    for (addr = 0x08; addr <= 0x77; addr++) {
        /* 尝试检测设备 */
        /* 使用新版I2C驱动API进行设备探测 */
        i2c_master_dev_handle_t dev_handle;
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = adap->config.freq,
        };
        
        ret = i2c_master_bus_add_device(adap->bus_handle, &dev_cfg, &dev_handle);
        if (ret == ESP_OK) {
            /* 设备添加成功，说明设备存在 */
            ESP_LOGI(TAG, "Found I2C device at address 0x%02x", addr);
            
            if (found_count < max_devices) {
                found_devices[found_count++] = addr;
            } else {
                ESP_LOGW(TAG, "Reached maximum device count, stopping scan");
                i2c_master_bus_rm_device(dev_handle);
                break;
            }
            
            /* 移除设备句柄 */
            i2c_master_bus_rm_device(dev_handle);
        }
    }
    
    ESP_LOGI(TAG, "Scan complete, found %d devices", found_count);
    return found_count;
}

/* 获取I2C总线状态 */
int i2c_esp32_get_bus_status(struct i2c_esp32_adapter *adap)
{
    if (!adap || !adap->initialized) {
        ESP_LOGE(TAG, "Invalid adapter or adapter not initialized");
        return -1;
    }
    
    /* 在ESP32 IDF V5.5中，可以通过检查总线句柄是否有效来判断状态 */
    if (adap->bus_handle) {
        return 1;  /* 总线正常 */
    } else {
        return 0;  /* 总线异常 */
    }
}

/* 重置I2C总线 */
int i2c_esp32_reset_bus(struct i2c_esp32_adapter *adap)
{
    esp_err_t ret;
    
    if (!adap || !adap->initialized) {
        ESP_LOGE(TAG, "Invalid adapter or adapter not initialized");
        return -1;
    }
    
    ESP_LOGI(TAG, "Resetting I2C bus on port %d", adap->port_num);
    
    /* 重置I2C总线 */
    /* 使用ESP-IDF V5.5的i2c_master_bus_reset API */
    ret = i2c_master_bus_reset(adap->bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset I2C bus: %s", esp_err_to_name(ret));
        return -1;
    }
    
    ESP_LOGI(TAG, "I2C bus reset successfully");
    return 0;
}

/* 设置I2C总线频率 */
int i2c_esp32_set_frequency(struct i2c_esp32_adapter *adap, uint32_t freq)
{
    esp_err_t ret;
    
    if (!adap || !adap->initialized) {
        ESP_LOGE(TAG, "Invalid adapter or adapter not initialized");
        return -1;
    }
    
    ESP_LOGI(TAG, "Setting I2C bus frequency to %dHz", freq);
    
    /* 设置总线频率 */
    /* 在ESP32 IDF V5.5中，频率在总线配置时设置，需要重新初始化总线来改变频率 */
    ESP_LOGD(TAG, "Changing I2C bus frequency to %dHz by reinitializing", freq);
    
    /* 更新配置中的频率 */
    adap->config.freq = freq;
    
    /* 重新初始化总线以应用新频率 */
    ret = i2c_del_master_bus(adap->bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete I2C bus for frequency change: %s", esp_err_to_name(ret));
        return -1;
    }
    
    /* 重新初始化总线 */
    ret = i2c_esp32_init(adap, &adap->config);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to reinitialize I2C bus for frequency change");
        return -1;
    }
    
    ESP_LOGI(TAG, "I2C bus frequency set successfully");
    return 0;
}