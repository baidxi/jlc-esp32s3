#!/bin/bash

# 测试脚本：验证ESP32-S3调试配置

echo "启动OpenOCD..."
openocd -f openocd_bridge_custom.cfg -c "gdb_port 3333; telnet_port 4444" &
OPENOCD_PID=$!

# 等待OpenOCD启动
echo "等待OpenOCD启动..."
sleep 3

# 检查OpenOCD是否正在运行
if ps -p $OPENOCD_PID > /dev/null; then
    echo "OpenOCD已启动，PID: $OPENOCD_PID"
    
    # 尝试连接GDB
    echo "尝试连接GDB..."
    echo "target remote localhost:3333" | /home/juno/.espressif/tools/xtensa-esp-elf-gdb/16.2_20250324/xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb build/jlc-esp32s3.elf
    
    # 清理
    kill $OPENOCD_PID
    echo "测试完成"
else
    echo "OpenOCD启动失败"
fi