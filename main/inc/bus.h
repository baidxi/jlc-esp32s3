#pragma once

#include "device.h"
#include "driver.h"

#include "list.h"

struct bus_type {
    const char *name;
    const char *dev_name;

    struct device *dev_root;

    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    void (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);

    struct list_head list;
};

/* 总线管理函数 */
void bus_init(void);
int bus_register(struct bus_type *bus);
void bus_unregister(struct bus_type *bus);
struct bus_type *bus_find(const char *name);