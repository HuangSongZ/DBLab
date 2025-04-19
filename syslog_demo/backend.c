#include "common.h"

/* 
 * 写入管道块
 * 将长消息分割成多个块，确保每个块不超过PIPE_MAX_PAYLOAD
 */
static void write_pipe_chunks(int fd, char *data, int len) {
    PipeProtoChunk p;
    int rc;

    if (len <= 0)
        return;

    /* 初始化协议头 */
    p.proto.nuls[0] = p.proto.nuls[1] = '\0';
    p.proto.pid = getpid();

    /* 写入除最后一块外的所有块 */
    while (len > PIPE_MAX_PAYLOAD) {
        p.proto.is_last = 'f';  /* 不是最后一块 */
        p.proto.len = PIPE_MAX_PAYLOAD;
        memcpy(p.proto.data, data, PIPE_MAX_PAYLOAD);
        
        /* 写入管道 */
        rc = write(fd, &p, PIPE_HEADER_SIZE + PIPE_MAX_PAYLOAD);
        if (rc < 0) {
            fprintf(stderr, "后端进程 %d: 写入管道失败: %s\n", getpid(), strerror(errno));
            return;
        }
        
        /* 更新指针和剩余长度 */
        data += PIPE_MAX_PAYLOAD;
        len -= PIPE_MAX_PAYLOAD;
        
        /* 模拟处理延迟 */
        usleep(10000);  /* 10ms */
    }

    /* 写入最后一块 */
    p.proto.is_last = 't';  /* 最后一块 */
    p.proto.len = len;
    memcpy(p.proto.data, data, len);
    
    rc = write(fd, &p, PIPE_HEADER_SIZE + len);
    if (rc < 0) {
        fprintf(stderr, "后端进程 %d: 写入最后一块失败: %s\n", getpid(), strerror(errno));
    }
}

/* 
 * 发送日志消息
 * 格式化日志消息并发送到syslogger
 */
static void send_log_message(int fd, int level, const char *format, ...) {
    char buffer[4096];  /* 足够大的缓冲区 */
    LogMessage log_msg;
    va_list args;
    int len;

    /* 设置日志消息 */
    log_msg.level = level;
    log_msg.pid = getpid();
    get_current_timestamp(log_msg.timestamp, sizeof(log_msg.timestamp));

    /* 格式化消息内容 */
    va_start(args, format);
    vsnprintf(log_msg.message, sizeof(log_msg.message), format, args);
    va_end(args);

    /* 将日志消息序列化到缓冲区 */
    len = snprintf(buffer, sizeof(buffer), "%s [%d]: %s: %s\n",
                  log_msg.timestamp, log_msg.pid,
                  get_log_level_name(log_msg.level), log_msg.message);

    /* 发送到管道 */
    write_pipe_chunks(fd, buffer, len);
}

int main(int argc, char *argv[]) {
    int pipe_fd;
    int backend_id = 1;
    int i;
    
    /* 解析命令行参数 */
    if (argc > 1) {
        backend_id = atoi(argv[1]);
    }
    
    printf("后端进程 %d (PID: %d) 启动\n", backend_id, getpid());
    
    /* 打开命名管道 */
    pipe_fd = open(PIPE_NAME, O_WRONLY);
    if (pipe_fd < 0) {
        fprintf(stderr, "无法打开管道 %s: %s\n", PIPE_NAME, strerror(errno));
        fprintf(stderr, "请确保syslogger进程已经启动\n");
        return 1;
    }
    
    /* 发送一些日志消息 */
    send_log_message(pipe_fd, LOG_LEVEL_INFO, "后端进程 %d 初始化", backend_id);
    
    /* 模拟不同大小的日志消息 */
    for (i = 0; i < 5; i++) {
        /* 短消息 */
        send_log_message(pipe_fd, LOG_LEVEL_INFO, 
                        "后端 %d: 短消息 #%d", backend_id, i);
        
        /* 中等长度消息 */
        send_log_message(pipe_fd, LOG_LEVEL_NOTICE,
                        "后端 %d: 中等长度消息 #%d - 这是一个演示日志系统的示例，"
                        "展示了多进程写入和分块传输机制", backend_id, i);
        
        /* 长消息 - 需要分块传输 */
        if (i % 2 == 0) {
            char long_msg[PIPE_MAX_PAYLOAD * 3];
            
            snprintf(long_msg, sizeof(long_msg),
                    "后端 %d: 长消息 #%d - 这是一个非常长的消息，将被分成多个块传输。"
                    "日志系统使用分块传输机制来处理大型日志消息，"
                    "确保每个块不超过PIPE_BUF大小，以利用POSIX保证的原子写入特性。"
                    "这种机制确保了即使在高并发环境下，来自不同进程的日志消息也不会在字节级别交错。"
                    "在实际的实现中，syslogger进程会收集这些消息块，"
                    "并根据协议头中的信息重组完整的消息。这个演示程序简化了实际实现，"
                    "但保留了核心概念，帮助理解日志系统设计。"
                    "这个长消息是为了确保超过PIPE_MAX_PAYLOAD而设计的，用于演示分块传输机制。"
                    "重复的文本：ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
                    backend_id, i);
            
            send_log_message(pipe_fd, LOG_LEVEL_WARNING, "%s", long_msg);
        }
        
        /* 模拟处理延迟 */
        sleep(1);
    }
    
    /* 发送一些错误级别的消息 */
    if (backend_id % 3 == 0) {
        send_log_message(pipe_fd, LOG_LEVEL_ERROR, 
                        "后端 %d: 模拟错误消息", backend_id);
    }
    
    if (backend_id % 5 == 0) {
        send_log_message(pipe_fd, LOG_LEVEL_FATAL, 
                        "后端 %d: 模拟致命错误", backend_id);
    }
    
    /* 发送结束消息 */
    send_log_message(pipe_fd, LOG_LEVEL_INFO, "后端进程 %d 结束", backend_id);
    
    /* 关闭管道 */
    close(pipe_fd);
    
    printf("后端进程 %d (PID: %d) 结束\n", backend_id, getpid());
    
    return 0;
}
