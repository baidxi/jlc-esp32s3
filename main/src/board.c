#include "core.h"
#include "i2c/i2c.h"
#include "i2c/i2c_esp32.h"
#include "i2c/qmi8658a.h"
#include "tty/tty.h"
#include "driver/uart.h"
#include "shell.h"

/* Shell命令初始化函数声明 */
void shell_cmds_init(void);

struct board_t {
    struct {
        struct i2c_esp32_adapter esp32;
        struct i2c_esp32_config config;
    }i2c;

    struct {
        struct esp32_uart_config config;
    }uart;
};

 struct board_t board = {
    .i2c = {
        .config = {
            .sda_io_num = 1,
            .scl_io_num = 2,
            .freq = 100000,
            .sda_pullup_en = 1,
            .scl_pullup_en = 1,
        },
        .esp32 = {
            .port_num = 0,
        }
    },
    .uart = {
        .config = {
            .uart_num = UART_NUM_0,  // 使用UART0
            .tx_pin = 43,            // UART0 TX引脚
            .rx_pin = 44,            // UART0 RX引脚
            .rts_pin = UART_PIN_NO_CHANGE,
            .cts_pin = UART_PIN_NO_CHANGE,
            .baudrate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_control = UART_HW_FLOWCTRL_DISABLE,
            .rx_buffer_size = 256,
            .tx_buffer_size = 256,
        }
    }
};

void board_init()
{
    int ret;

    /* 注意：driver_framework_init()已在main.c中调用，这里不再重复调用 */
    // driver_framework_init();
    
    i2c_init_subsystem();

    ret = i2c_esp32_init(&board.i2c.esp32, &board.i2c.config);
    if (ret) {
        printf("[esp32] i2c adapter init error\n");
        return;
    }

    register_qmi8658a_device(&board.i2c.esp32.adapter);

    /* 初始化TTY框架 */
    tty_init();
    
    /* 注册UART设备 */
    ret = esp32_uart_register(&board.uart.config, "ttyS0");
    if (ret) {
        printf("[esp32] tty device init error\n");
        return;
    }
    
    printf("[esp32] tty device initialized\n");
}
