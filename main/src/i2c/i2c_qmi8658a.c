#include "common.h"
#include "i2c/i2c.h"

struct qmi8658a_i2c_device {
    struct i2c_device dev;
    struct i2c_driver *drv;
    struct i2c_adapter *adap;
};

struct qmi8658a_i2c_driver {
    struct i2c_driver drv;
    void *data;
};

#define to_qmi8658a(_d) (container_of(_d, struct qmi8658a_i2c_device, dev))

static struct qmi8658a_i2c_device qmi8658a_device = {
    .dev = {
        .dev = {
            .init_name = "qmi8658a_i2c_device"
        },
        .addr = 0x6a,
        .flags = 0,
    }
};

static int qmi8658a_i2c_probe(struct i2c_device *dev)
{
    struct i2c_adapter *adap = to_i2c_adapter(dev->dev.parent);
    struct i2c_msg msg;
    uint8_t buf;
    int ret;

    msg.addr = dev->addr;
    msg.flags = I2C_M_RD;
    msg.len = 1;
    msg.buf = &buf;

    ret = i2c_transfer(adap, &msg, 1);

    if (ret != 1) {
        printf("[qmi8658a] read failed:%d\n", ret);
        return -1;
    }

    printf("[qmi8658a] read reg %02x\n", buf);

    return 0;
}

static int qmi8658a_i2c_remove(struct i2c_device *dev)
{
    return 0;
}

static void qmi8658a_i2c_shutdown(struct i2c_device *dev)
{

}

static const struct i2c_device_table qmi8658a_ids[] = {
    {
        .compatible = "qmi8658a_i2c_device"
    },
    {}
};

static struct qmi8658a_i2c_driver qmi8658a_driver = {
    .drv = {
        .driver = {
            .name = "qmi8658a_i2c_driver",
        },
        .match_ptr = qmi8658a_ids,
        .probe = qmi8658a_i2c_probe,
        .remove = qmi8658a_i2c_remove,
        .shutdown = qmi8658a_i2c_shutdown, 
    }
};

int register_qmi8658a_device(struct i2c_adapter *adap)
{
    int ret;
    
    qmi8658a_device.dev.dev.parent = &adap->dev;

    ret = i2c_register_device(&qmi8658a_device.dev);

    if (ret) {
        printf("[qmi8658a] register err\n");
        return ret;
    }

    ret = i2c_register_driver(&qmi8658a_driver.drv);
    if (ret) {
        printf("[qmi8658a] register driver err\n");
        return ret;
    }

    return 0;
}