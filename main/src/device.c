#include <errno.h>
#include <string.h>
#include <stddef.h>

#include "device.h"
#include "driver.h"

static struct list_head device_list;

void device_init(void)
{
    INIT_LIST_HEAD(&device_list);
}

int device_register(struct device *dev)
{
    struct device *_dev;
    int ret;

    if (!dev || !dev->init_name) {
        return -EINVAL;
    }

    /* 检查设备是否已经注册 */
    list_for_each_entry(_dev, &device_list, list) {
        if (!strcmp(_dev->init_name, dev->init_name)) {
            return -EEXIST;
        }
    }

    /* 初始化设备链表节点 */
    INIT_LIST_HEAD(&dev->list);

    /* 将设备添加到设备列表 */
    list_add_tail(&dev->list, &device_list);

    /* 如果设备属于某个总线，也将其添加到总线设备列表 */
    /* 注意：这里不能重复添加同一个链表节点到不同的链表 */
    /* 总线设备列表应该由总线管理，这里暂时不处理 */

    /* 设备注册成功 */

    return 0;
}

void device_unregister(struct device *dev)
{
    if (!dev) {
        return;
    }

    /* 如果设备有驱动，先调用驱动的remove函数 */
    if (dev->driver && dev->driver->remove) {
        dev->driver->remove(dev);
    }

    /* 设备注销成功 */

    /* 从设备列表中移除 */
    list_del(&dev->list);
}

struct device *device_find(const char *name)
{
    struct device *dev;

    list_for_each_entry(dev, &device_list, list) {
        if (!strcmp(dev->init_name, name)) {
            return dev;
        }
    }

    return NULL;
}

void device_for_each(void (*fn)(struct device *dev, void *data), void *data)
{
    struct device *dev;

    if (!fn) {
        return;
    }

    list_for_each_entry(dev, &device_list, list) {
        fn(dev, data);
    }
}