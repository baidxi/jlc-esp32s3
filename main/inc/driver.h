#pragma once

#include "list.h"

struct device;
struct bus_type;

struct device_driver {
    const char *name;
    struct bus_type *bus;

    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);

    struct list_head list;
};

/* 驱动管理函数 */
void driver_init(void);
int driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);
int driver_attach(struct device_driver *drv);
int driver_probe_device(struct device_driver *drv, struct device *dev);
void driver_detach(struct device_driver *drv, struct device *dev);