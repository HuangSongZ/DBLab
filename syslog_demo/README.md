# Syslog 实现演示

这个演示项目模拟了Syslog的日志系统实现，特别是syslogger进程和后端进程之间的通信机制。

## 项目结构

- `syslogger.c`: 模拟syslogger进程，负责收集和处理日志
- `backend.c`: 模拟Syslog的后端进程，生成并发送日志消息
- `common.h`: 共享的头文件，定义协议和常量
- `Makefile`: 编译脚本

## 核心概念演示

1. **管道通信**: 使用命名管道(FIFO)实现进程间通信
2. **分块传输**: 大消息的分块传输机制
3. **原子写入**: 利用PIPE_BUF保证小块写入的原子性
4. **并发处理**: 多个后端进程并发写入日志
5. **消息重组**: syslogger进程对分块消息的重组

## 使用方法

```bash
# 编译
make

# 启动syslogger进程
./syslogger

# 在另一个终端启动多个后端进程
./backend 1 &
./backend 2 &
./backend 3 &
```
