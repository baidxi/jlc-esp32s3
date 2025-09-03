#include "core.h"
#include "bus.h"
#include "device.h"
#include "driver.h"

/**
 * @brief 初始化设备驱动框架
 * 
 * 该函数初始化设备驱动框架的核心组件，包括总线、设备和驱动的管理。
 * 在使用框架的任何功能之前，必须先调用此函数进行初始化。
 */
void driver_framework_init(void)
{
    /* 初始化总线管理 */
    bus_init();
    
    /* 初始化设备管理 */
    device_init();
    
    /* 初始化驱动管理 */
    driver_init();
}