#include <errno.h>
#include <string.h>
#include <stddef.h>

#include "bus.h"
#include "device.h"
#include "driver.h"
#include "fs/fs.h"

static struct list_head bus_list;

void bus_init(void)
{
    INIT_LIST_HEAD(&bus_list);
}

int bus_register(struct bus_type *bus)
{
    struct bus_type *_;
    int ret;

    if (!bus || !bus->name) {
        return -EINVAL;
    }

    list_for_each_entry(_, &bus_list, list) {
        if (bus == _)
            return -EEXIST;
    }

    /* 初始化总线设备列表 */
    INIT_LIST_HEAD(&bus->list);

    list_add_tail(&bus->list, &bus_list);

    /* 注册总线到sysfs */
    ret = sysfs_register_bus(bus);
    if (ret != 0) {
        /* 如果sysfs注册失败，从总线列表中移除 */
        list_del(&bus->list);
        return ret;
    }

    return 0;
}

void bus_unregister(struct bus_type *bus)
{
    if (!bus) {
        return;
    }

    /* 从sysfs注销总线 */
    sysfs_unregister_bus(bus);

    /* 从总线列表中移除 */
    list_del(&bus->list);
}

struct bus_type *bus_find(const char *name)
{
    struct bus_type *bus;

    list_for_each_entry(bus, &bus_list, list) {
        if (!strcmp(bus->name, name)) {
            return bus;
        }
    }

    return NULL;
}
