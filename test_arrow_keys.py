#!/usr/bin/env python3
import serial
import time

# 配置串口
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# 等待串口稳定
time.sleep(2)

# 发送一些测试命令
test_commands = ['help', 'ls', 'clear']

# 发送测试命令
for cmd in test_commands:
    print(f"Sending command: {cmd}")
    ser.write(cmd.encode())
    ser.write(b'\r\n')
    time.sleep(1)

# 发送上方向键 (ESC[A)
print("Sending up arrow key")
ser.write(b'\x1b[A')
time.sleep(1)

# 发送下方向键 (ESC[B)
print("Sending down arrow key")
ser.write(b'\x1b[B')
time.sleep(1)

# 再次发送上方向键
print("Sending up arrow key again")
ser.write(b'\x1b[A')
time.sleep(1)

# 关闭串口
ser.close()
print("Test completed")