#pragma once

#include "bus.h"
#include "list.h"

struct device {
    struct device *parent;
    const char *init_name;
    struct bus_type *bus;
    struct device_driver *driver;
    void *driver_data;
    struct list_head list;
};

/* 设备管理函数 */
void device_init(void);
int device_register(struct device *dev);
void device_unregister(struct device *dev);
struct device *device_find(const char *name);
void device_for_each(void (*fn)(struct device *dev, void *data), void *data);