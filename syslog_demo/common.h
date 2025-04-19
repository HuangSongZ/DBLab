#ifndef PG_SYSLOG_COMMON_H
#define PG_SYSLOG_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <stdarg.h>

/* 日志级别定义 */
#define LOG_LEVEL_DEBUG    1
#define LOG_LEVEL_INFO     2
#define LOG_LEVEL_NOTICE   3
#define LOG_LEVEL_WARNING  4
#define LOG_LEVEL_ERROR    5
#define LOG_LEVEL_FATAL    6
#define LOG_LEVEL_PANIC    7

/* 管道相关常量 */
#define PIPE_NAME "/tmp/pg_syslog_demo.pipe"

/* 
 * 根据POSIX标准，PIPE_BUF大小的写入是原子的
 * 在Linux上通常是4096字节
 */
#ifndef PIPE_BUF
#define PIPE_BUF 512  /* POSIX保证的最小值 */
#endif

/* 定义最大载荷大小 */
#define PIPE_MAX_PAYLOAD (PIPE_BUF - 32)

/* 协议头大小 */
#define PIPE_HEADER_SIZE (sizeof(PipeProtoHeader))

/* 日志协议头 */
typedef struct {
    char        nuls[2];        /* 始终为 \0\0，用于协议识别 */
    unsigned short len;         /* 数据块大小 */
    int         pid;            /* 发送进程的PID */
    char        is_last;        /* 是否为消息的最后一块 */
    char        data[0];        /* 柔性数组成员，存储实际数据 */
} PipeProtoHeader;

/* 完整的协议块 */
typedef struct {
    PipeProtoHeader proto;
    char        data[PIPE_MAX_PAYLOAD];  /* 预分配的数据缓冲区 */
} PipeProtoChunk;

/* 日志消息结构 */
typedef struct {
    int         level;          /* 日志级别 */
    char        timestamp[32];  /* 时间戳 */
    int         pid;            /* 进程ID */
    char        message[1024];  /* 日志消息 */
} LogMessage;

/* 获取日志级别名称 */
const char *get_log_level_name(int level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "DEBUG";
        case LOG_LEVEL_INFO:    return "INFO";
        case LOG_LEVEL_NOTICE:  return "NOTICE";
        case LOG_LEVEL_WARNING: return "WARNING";
        case LOG_LEVEL_ERROR:   return "ERROR";
        case LOG_LEVEL_FATAL:   return "FATAL";
        case LOG_LEVEL_PANIC:   return "PANIC";
        default:                return "UNKNOWN";
    }
}

/* 获取当前时间戳 */
static void get_current_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S.000 CST", tm_info);
}

#endif /* PG_SYSLOG_COMMON_H */
