#include "common.h"
#include <sys/select.h>
#include <sys/time.h>
#include <stdarg.h>

/* 日志文件 */
#define LOG_FILE "./syslog_demo.log"

/* 用于存储未完成的消息块 */
typedef struct {
    int         pid;            /* 进程ID */
    char        *buffer;        /* 消息缓冲区 */
    int         used;           /* 已使用的缓冲区大小 */
    int         allocated;      /* 已分配的缓冲区大小 */
} PendingChunk;

/* 最多支持的并发后端进程数 */
#define MAX_PENDING_CHUNKS 100

/* 全局变量 */
static PendingChunk pending_chunks[MAX_PENDING_CHUNKS];
static int num_pending_chunks = 0;
static FILE *log_file = NULL;
static int running = 1;

/* 信号处理 */
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        fprintf(stderr, "收到信号 %d，准备退出...\n", sig);
        running = 0;
    }
}

/* 初始化日志文件 */
static int init_log_file(void) {
    log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        fprintf(stderr, "无法打开日志文件 %s: %s\n", LOG_FILE, strerror(errno));
        return -1;
    }
    
    /* 设置行缓冲模式 */
    setvbuf(log_file, NULL, _IOLBF, 0);
    
    return 0;
}

/* 写入日志文件 */
static void write_to_log(const char *format, ...) {
    va_list args;
    char timestamp[32];
    
    if (!log_file)
        return;
    
    /* 获取当前时间戳 */
    get_current_timestamp(timestamp, sizeof(timestamp));
    
    /* 写入时间戳和进程ID */
    fprintf(log_file, "%s [%d]: ", timestamp, getpid());
    
    /* 写入消息 */
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    /* 确保写入磁盘 */
    fflush(log_file);
}

/* 查找或创建待处理块 */
static PendingChunk *find_or_create_pending_chunk(int pid) {
    int i;
    
    /* 查找现有块 */
    for (i = 0; i < num_pending_chunks; i++) {
        if (pending_chunks[i].pid == pid)
            return &pending_chunks[i];
    }
    
    /* 如果没有找到，创建新块 */
    if (num_pending_chunks < MAX_PENDING_CHUNKS) {
        PendingChunk *chunk = &pending_chunks[num_pending_chunks++];
        chunk->pid = pid;
        chunk->buffer = malloc(1024);  /* 初始分配 */
        chunk->allocated = 1024;
        chunk->used = 0;
        return chunk;
    }
    
    /* 超出最大数量 */
    write_to_log("SYSLOGGER: 超出最大待处理块数量，丢弃来自PID %d的消息\n", pid);
    return NULL;
}

/* 确保缓冲区足够大 */
static void ensure_buffer_space(PendingChunk *chunk, int needed) {
    if (chunk->used + needed > chunk->allocated) {
        int new_size = chunk->allocated * 2;
        while (new_size < chunk->used + needed)
            new_size *= 2;
        
        chunk->buffer = realloc(chunk->buffer, new_size);
        chunk->allocated = new_size;
    }
}

/* 处理完整的日志消息 */
static void process_complete_message(PendingChunk *chunk) {
    /* 直接写入日志文件 */
    fprintf(log_file, "%s", chunk->buffer);
    fflush(log_file);
    
    /* 重置缓冲区 */
    chunk->used = 0;
}

/* 处理收到的消息块 */
static void process_pipe_input(int pipe_fd) {
    PipeProtoChunk p;
    int bytes_read;
    PendingChunk *chunk;
    
    /* 读取消息块 */
    bytes_read = read(pipe_fd, &p, sizeof(p));
    if (bytes_read <= 0) {
        if (bytes_read < 0 && errno != EAGAIN)
            write_to_log("SYSLOGGER: 从管道读取失败: %s\n", strerror(errno));
        return;
    }
    
    /* 验证协议头 */
    if (p.proto.nuls[0] != '\0' || p.proto.nuls[1] != '\0') {
        write_to_log("SYSLOGGER: 收到无效的协议头\n");
        return;
    }
    
    /* 查找或创建待处理块 */
    chunk = find_or_create_pending_chunk(p.proto.pid);
    if (!chunk)
        return;
    
    /* 确保缓冲区足够大 */
    ensure_buffer_space(chunk, p.proto.len);
    
    /* 追加数据 */
    memcpy(chunk->buffer + chunk->used, p.proto.data, p.proto.len);
    chunk->used += p.proto.len;
    
    /* 如果是最后一块，处理完整消息 */
    if (p.proto.is_last == 't') {
        /* 确保字符串以null结尾 */
        ensure_buffer_space(chunk, 1);
        chunk->buffer[chunk->used] = '\0';
        
        /* 处理完整消息 */
        process_complete_message(chunk);
    }
}

/* 清理资源 */
static void cleanup(void) {
    int i;
    
    /* 关闭日志文件 */
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    
    /* 释放待处理块 */
    for (i = 0; i < num_pending_chunks; i++) {
        free(pending_chunks[i].buffer);
        pending_chunks[i].buffer = NULL;
    }
    num_pending_chunks = 0;
}

int main(void) {
    int pipe_fd;
    struct sigaction sa;
    
    /* 设置信号处理 */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    /* 初始化日志文件 */
    if (init_log_file() < 0) {
        return 1;
    }
    
    /* 创建命名管道 */
    unlink(PIPE_NAME);  /* 移除可能存在的旧管道 */
    if (mkfifo(PIPE_NAME, 0666) < 0) {
        fprintf(stderr, "无法创建命名管道 %s: %s\n", PIPE_NAME, strerror(errno));
        cleanup();
        return 1;
    }
    
    /* 打开管道读取端 */
    pipe_fd = open(PIPE_NAME, O_RDONLY | O_NONBLOCK);
    if (pipe_fd < 0) {
        fprintf(stderr, "无法打开管道读取端 %s: %s\n", PIPE_NAME, strerror(errno));
        unlink(PIPE_NAME);
        cleanup();
        return 1;
    }
    
    printf("Syslogger进程启动 (PID: %d)\n", getpid());
    printf("日志将写入 %s\n", LOG_FILE);
    printf("按Ctrl+C退出\n");
    
    write_to_log("SYSLOGGER: 进程启动\n");
    
    /* 主循环 */
    while (running) {
        fd_set rfds;
        struct timeval tv;
        int ret;
        
        /* 设置select参数 */
        FD_ZERO(&rfds);
        FD_SET(pipe_fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        /* 等待管道可读 */
        ret = select(pipe_fd + 1, &rfds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno != EINTR) {
                write_to_log("SYSLOGGER: select失败: %s\n", strerror(errno));
                break;
            }
        } else if (ret > 0) {
            /* 处理管道输入 */
            if (FD_ISSET(pipe_fd, &rfds)) {
                process_pipe_input(pipe_fd);
            }
        }
        
        /* 周期性刷新日志文件 */
        if (log_file)
            fflush(log_file);
    }
    
    /* 清理资源 */
    write_to_log("SYSLOGGER: 进程关闭\n");
    close(pipe_fd);
    unlink(PIPE_NAME);
    cleanup();
    
    printf("Syslogger进程已关闭\n");
    
    return 0;
}
