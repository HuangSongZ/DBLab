#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

// 共享内存名称
#define SHM_NAME "/pg_cache_inval_demo"
// 信号量名称
#define SEM_NAME "/pg_cache_inval_sem"

// 最大后端进程数
#define MAX_BACKENDS 10
// 最大消息数量
#define MAX_MESSAGES 100

// 消息类型常量
enum InvalidationMessageType {
    CACHE_INVAL_CATCACHE = 0,     // 系统缓存失效
    CACHE_INVAL_RELCACHE = -1,    // 关系缓存失效
    CACHE_INVAL_SYSCACHE = -2,    // 系统目录缓存失效
    CACHE_INVAL_SNAPSHOT = -3     // 快照失效
};

// 缓存失效消息
typedef struct {
    int8_t id;                    // 消息类型
    uint32_t dbId;                // 数据库ID
    uint32_t relId;               // 关系ID或目录ID
    uint32_t hashValue;           // 哈希值（用于系统缓存）
} InvalidationMessage;

// 后端进程状态
typedef struct {
    int pid;                      // 进程ID
    int nextMsgNum;               // 下一个要处理的消息编号
    int resetState;               // 是否需要重置
    int hasMessages;              // 是否有新消息
    int signaled;                 // 是否已发送信号
    uint32_t dbId;                // 数据库ID
} BackendState;

// 共享内存结构
typedef struct {
    int minMsgNum;                // 最小消息编号
    int maxMsgNum;                // 最大消息编号
    int nextThreshold;            // 清理阈值
    int lastBackendId;            // 最后一个后端ID
    
    // 后端状态数组
    BackendState backends[MAX_BACKENDS];
    
    // 消息环形缓冲区
    InvalidationMessage messages[MAX_MESSAGES];
} SharedInvalBuffer;

// 初始化共享内存
int init_shared_memory() {
    int fd;
    SharedInvalBuffer *buffer;
    
    // 创建共享内存
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        return -1;
    }
    
    // 设置共享内存大小
    if (ftruncate(fd, sizeof(SharedInvalBuffer)) == -1) {
        perror("ftruncate");
        close(fd);
        return -1;
    }
    
    // 映射共享内存
    buffer = (SharedInvalBuffer *)mmap(NULL, sizeof(SharedInvalBuffer), 
                                      PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }
    
    // 初始化共享内存
    memset(buffer, 0, sizeof(SharedInvalBuffer));
    buffer->minMsgNum = 0;
    buffer->maxMsgNum = 0;
    buffer->nextThreshold = 10;
    buffer->lastBackendId = 0;
    
    // 解除映射
    if (munmap(buffer, sizeof(SharedInvalBuffer)) == -1) {
        perror("munmap");
    }
    
    close(fd);
    return 0;
}

// 清理共享内存
void cleanup_shared_memory() {
    shm_unlink(SHM_NAME);
}

// 创建信号量
sem_t *create_semaphore() {
    sem_t *sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        return NULL;
    }
    return sem;
}

// 清理信号量
void cleanup_semaphore() {
    sem_unlink(SEM_NAME);
}

// 全局共享内存指针，每个进程只映射一次
static SharedInvalBuffer *g_shared_buffer = NULL;

// 获取共享内存映射 - 进程初始化时调用一次
SharedInvalBuffer *attach_shared_buffer() {
    if (g_shared_buffer != NULL) {
        return g_shared_buffer; // 已经映射过，直接返回
    }
    
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        return NULL;
    }
    
    g_shared_buffer = (SharedInvalBuffer *)mmap(NULL, sizeof(SharedInvalBuffer), 
                                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (g_shared_buffer == MAP_FAILED) {
        perror("mmap");
        close(fd);
        g_shared_buffer = NULL;
        return NULL;
    }
    
    close(fd);
    return g_shared_buffer;
}

// 获取已映射的共享内存 - 不需要重新映射
SharedInvalBuffer *get_shared_buffer() {
    return g_shared_buffer;
}

// 释放共享内存映射 - 进程退出时调用一次
void detach_shared_buffer() {
    if (g_shared_buffer != NULL) {
        munmap(g_shared_buffer, sizeof(SharedInvalBuffer));
        g_shared_buffer = NULL;
    }
}

#endif // SHARED_MEMORY_H
