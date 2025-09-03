#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "core.h"
#include "shell_platform.h"
#include "shell.h"
#include "shell_cmds.h"
#include "fs/fs.h"
#include "device.h"
#include "bus.h"
#include "tty/tty.h"
#include "driver/uart.h"

/* 声明板级初始化函数 */
void board_init(void);

static const char *TAG = "esp32s3";


/* 声明平台抽象层初始化函数 */
int shell_platform_esp32_init(void);

void app_main(void)
{
    /* 使用early_printf输出早期日志 */
    shell_early_printf("Hello world!\n");
    shell_early_printf("This is esp32s3 chip with %d CPU core(s), WiFi%s%s, \n",
             2,
             ", BLE" ? "true" : "false",
             ", Embedded PSRAM 8MB (AP_3v3)" ? "true" : "false");

    shell_early_printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
    
    // 检查PSRAM是否可用
    ESP_LOGI(TAG, "Checking PSRAM...");
    // 使用heap_caps检查PSRAM是否可用
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram_size > 0) {
        ESP_LOGI(TAG, "PSRAM is available, size: %d KB", psram_size / 1024);
        
        // 测试PSRAM
        size_t test_size = 1024 * 1024; // 测试1MB
        uint8_t *psram_test_buffer = heap_caps_malloc(test_size, MALLOC_CAP_SPIRAM);
        if (psram_test_buffer) {
            ESP_LOGI(TAG, "Successfully allocated %d bytes from PSRAM", test_size);
            
            // 写入测试数据
            for (size_t i = 0; i < test_size; i++) {
                psram_test_buffer[i] = i % 256;
            }
            
            // 读取并验证测试数据
            bool test_passed = true;
            for (size_t i = 0; i < test_size; i++) {
                if (psram_test_buffer[i] != i % 256) {
                    test_passed = false;
                    break;
                }
            }
            
            if (test_passed) {
                ESP_LOGI(TAG, "PSRAM read/write test passed");
            } else {
                ESP_LOGE(TAG, "PSRAM read/write test failed");
            }
            
            heap_caps_free(psram_test_buffer);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory from PSRAM");
        }
        
        // 打印内存信息
        ESP_LOGI(TAG, "Total heap size: %d bytes", heap_caps_get_total_size(MALLOC_CAP_8BIT));
        ESP_LOGI(TAG, "Free heap size: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        ESP_LOGI(TAG, "Free PSRAM size: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    } else {
        ESP_LOGE(TAG, "PSRAM is not available");
    }

    /* 初始化Shell平台抽象层 */
    ESP_LOGI(TAG, "Initializing shell platform abstraction layer...");
    if (shell_platform_esp32_init() != 0) {
        ESP_LOGE(TAG, "Failed to initialize shell platform abstraction layer");
        return;
    }
    
    /* 初始化sysfs虚拟文件系统 */
    ESP_LOGI(TAG, "Initializing sysfs virtual file system...");
    if (sysfs_init() != 0) {
        ESP_LOGE(TAG, "Failed to initialize sysfs virtual file system");
        return;
    }
    
    /* 初始化设备驱动框架 */
    ESP_LOGI(TAG, "Initializing driver framework...");
    driver_init();
    
    /* 初始化设备和总线管理 */
    ESP_LOGI(TAG, "Initializing device and bus management...");
    device_init();
    bus_init();
    
    /* 初始化I2C示例 */
    // ESP_LOGI(TAG, "Initializing I2C example...");
    // i2c_example_init();
    
    // ESP_LOGI(TAG, "Driver framework, example module and I2C example initialized successfully!");
    
    /* 初始化板级设备 */
    ESP_LOGI(TAG, "Initializing board devices...");
    board_init();
    
    /* 初始化Shell终端 */
    ESP_LOGI(TAG, "Initializing shell terminal...");
    if (shell_init("ttyS0") != 0) {
        ESP_LOGE(TAG, "Failed to initialize shell terminal");
        return;
    }
    
    /* 注册Shell命令 */
    ESP_LOGI(TAG, "Registering shell commands...");
    shell_cmds_init();
    
    /* 启动Shell终端 */
    ESP_LOGI(TAG, "Starting shell terminal...");
    if (shell_start("ttyS0") != 0) {
        ESP_LOGE(TAG, "Failed to start shell terminal");
        return;
    }

    ESP_LOGI(TAG, "System initialized successfully. Shell terminal is now available.");
}
