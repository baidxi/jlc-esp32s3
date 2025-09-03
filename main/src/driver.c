#include <errno.h>
#include <string.h>
#include <stddef.h>

#include "driver.h"
#include "device.h"

static struct list_head driver_list;

void driver_init(void)
{
    INIT_LIST_HEAD(&driver_list);
}

int driver_register(struct device_driver *drv)
{
    struct device_driver *_drv;

    if (!drv || !drv->name || !drv->bus) {
        return -EINVAL;
    }

    /* 检查驱动是否已经注册 */
    list_for_each_entry(_drv, &driver_list, list) {
        if (!strcmp(_drv->name, drv->name) && _drv->bus == drv->bus) {
            return -EEXIST;
        }
    }

    /* 初始化驱动链表节点 */
    INIT_LIST_HEAD(&drv->list);

    /* 将驱动添加到驱动列表 */
    list_add_tail(&drv->list, &driver_list);

    /* 尝试将驱动与现有设备匹配 */
    driver_attach(drv);

    return 0;
}

void driver_unregister(struct device_driver *drv)
{
    if (!drv) {
        return;
    }

    /* 从驱动列表中移除 */
    list_del(&drv->list);
}

struct driver_attach_data {
    struct device_driver *drv;
};

static void driver_attach_helper(struct device *dev, void *data)
{
    struct driver_attach_data *d = data;
    struct device_driver *drv = d->drv;

    /* 跳过不属于该总线的设备 */
    if (dev->bus != drv->bus)
        return;

    /* 跳过已经有驱动的设备 */
    if (dev->driver)
        return;

    /* 如果总线有match函数，使用它来匹配设备和驱动 */
    if (drv->bus->match) {
        if (drv->bus->match(dev, drv) == 0) {
            /* 匹配成功，尝试绑定设备和驱动 */
            if (driver_probe_device(drv, dev) == 0) {
                /* 绑定成功 */
                return;
            }
        }
    } else {
        /* 如果没有match函数，直接尝试探测 */
        if (driver_probe_device(drv, dev) == 0) {
            /* 绑定成功 */
            return;
        }
    }
}

int driver_attach(struct device_driver *drv)
{
    struct driver_attach_data data;

    if (!drv || !drv->bus) {
        return -EINVAL;
    }

    data.drv = drv;
    device_for_each(driver_attach_helper, &data);

    return 0;
}

int driver_probe_device(struct device_driver *drv, struct device *dev)
{
    int ret = 0;

    if (!drv || !dev) {
        return -EINVAL;
    }

    /* 将驱动绑定到设备 */
    dev->driver = drv;

    /* 调用驱动的probe函数 */
    if (drv->probe) {
        ret = drv->probe(dev);
        if (ret) {
            /* probe失败，解除绑定 */
            dev->driver = NULL;
            return ret;
        }
    }

    return 0;
}

void driver_detach(struct device_driver *drv, struct device *dev)
{
    if (!drv || !dev) {
        return;
    }

    /* 如果设备绑定了这个驱动 */
    if (dev->driver == drv) {
        /* 调用驱动的remove函数 */
        if (drv->remove) {
            drv->remove(dev);
        }

        /* 解除绑定 */
        dev->driver = NULL;
        dev->driver_data = NULL;
    }
}