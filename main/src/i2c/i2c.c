#include "i2c/i2c.h"
#include "common.h"
#include <string.h>

/* I2C设备匹配函数 */
static int i2c_device_match(struct device *dev, struct device_driver *drv)
{
    struct i2c_device *i2c_dev = to_i2c_device(dev);
    struct i2c_driver *i2c_drv = to_i2c_driver(drv);
    const struct i2c_device_table *ptr = i2c_drv->match_ptr;

    if (!i2c_dev || !i2c_drv || !ptr) {
        return 0;
    }

    for (; ptr->compatible; ptr++) {
        if (strcmp(ptr->compatible, i2c_dev->dev.init_name) == 0) {
            return 1;  /* 匹配成功 */
        }
    }

    return 0;  /* 匹配失败 */
}

/* I2C设备探测函数 */
static int i2c_device_probe(struct device *dev)
{
    struct i2c_device *i2c_dev = to_i2c_device(dev);
    struct i2c_driver *i2c_drv = to_i2c_driver(dev->driver);
    
    if (i2c_drv && i2c_drv->probe) {
        return i2c_drv->probe(i2c_dev);
    }
    
    return 0;
}

/* I2C设备移除函数 */
static void i2c_device_remove(struct device *dev)
{
    struct i2c_device *i2c_dev = to_i2c_device(dev);
    struct i2c_driver *i2c_drv = to_i2c_driver(dev->driver);
    
    if (i2c_drv && i2c_drv->remove) {
        i2c_drv->remove(i2c_dev);
    }
}

/* I2C设备移除函数包装器，用于device_driver结构体 */
static int i2c_device_remove_wrapper(struct device *dev)
{
    i2c_device_remove(dev);
    return 0;
}

/* I2C设备关闭函数 */
static void i2c_device_shutdown(struct device *dev)
{
    struct i2c_device *i2c_dev = to_i2c_device(dev);
    struct i2c_driver *i2c_drv = to_i2c_driver(dev->driver);
    
    if (i2c_drv && i2c_drv->shutdown) {
        i2c_drv->shutdown(i2c_dev);
    }
}

/* I2C总线类型定义 */
static struct bus_type i2c_bus_type = {
    .name = "i2c",
    .match = i2c_device_match,
    .probe = i2c_device_probe,
    .remove = i2c_device_remove,
    .shutdown = i2c_device_shutdown,
};

/* 初始化I2C总线类型 */
static int i2c_init(void)
{
    /* 初始化总线链表头 */
    INIT_LIST_HEAD(&i2c_bus_type.list);
    
    return bus_register(&i2c_bus_type);
}

/* 注册I2C适配器 */
int i2c_register_adapter(struct i2c_adapter *adapter)
{
    if (!adapter || !adapter->algo) {
        return -1;
    }
    
    /* 初始化设备结构 */
    adapter->dev.bus = &i2c_bus_type;
    
    /* 注册设备 */
    return device_register(&adapter->dev);
}

/* 注销I2C适配器 */
void i2c_unregister_adapter(struct i2c_adapter *adapter)
{
    if (adapter) {
        device_unregister(&adapter->dev);
    }
}

/* 注册I2C设备 */
int i2c_register_device(struct i2c_device *dev)
{
    if (!dev) {
        return -1;
    }
    
    /* 初始化设备结构 */
    dev->dev.bus = &i2c_bus_type;
    
    /* 注册设备 */
    return device_register(&dev->dev);
}

/* 注销I2C设备 */
void i2c_unregister_device(struct i2c_device *dev)
{
    if (dev) {
        /* 注销设备 */
        device_unregister(&dev->dev);
    }
}

/* 注册I2C驱动 */
int i2c_register_driver(struct i2c_driver *driver)
{
    if (!driver) {
        return -1;
    }
    
    /* 初始化驱动结构 */
    driver->driver.bus = &i2c_bus_type;
    driver->driver.probe = i2c_device_probe;
    driver->driver.remove = i2c_device_remove_wrapper;
    driver->driver.shutdown = i2c_device_shutdown;
    
    /* 注册驱动 */
    return driver_register(&driver->driver);
}

/* 注销I2C驱动 */
void i2c_unregister_driver(struct i2c_driver *driver)
{
    if (driver) {
        driver_unregister(&driver->driver);
    }
}

/* I2C传输函数 */
int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
    if (!adap || !adap->algo || !adap->algo->master_xfer) {
        return -1;
    }
    
    return adap->algo->master_xfer(adap, msgs, num);
}

/* 验证I2C设备 */
struct i2c_device *i2c_verify_device(struct device *dev)
{
    if (!dev || dev->bus != &i2c_bus_type) {
        return NULL;
    }
    
    return to_i2c_device(dev);
}

/* 验证I2C适配器 */
struct i2c_adapter *i2c_verify_adapter(struct device *dev)
{
    if (!dev || dev->bus != &i2c_bus_type) {
        return NULL;
    }
    
    return to_i2c_adapter(dev);
}

/* 验证I2C驱动 */
struct i2c_driver *i2c_verify_driver(struct device_driver *drv)
{
    if (!drv || drv->bus != &i2c_bus_type) {
        return NULL;
    }
    
    return to_i2c_driver(drv);
}

/* 初始化I2C子系统 */
int i2c_init_subsystem(void)
{
    return i2c_init();
}