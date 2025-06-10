#ifndef BACKEND_PROCESS_H
#define BACKEND_PROCESS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <semaphore.h>
#include <stdint.h>
#include "shared_memory.h"

// 缓存项状态
typedef enum {
    CACHE_VALID = 1,
    CACHE_INVALID = 0
} CacheItemStatus;

// 缓存项
typedef struct {
    uint32_t key;
    char value[64];
    CacheItemStatus status;
} CacheItem;

// 本地缓存
typedef struct {
    CacheItem items[100];
    int count;
} LocalCache;

// 事务状态
typedef enum {
    TRANS_IDLE = 0,
    TRANS_ACTIVE = 1
} TransactionState;

// 事务中的失效消息
typedef struct {
    InvalidationMessage messages[100];
    int count;
} InvalidationMessageList;

// 事务上下文
typedef struct {
    TransactionState state;
    InvalidationMessageList currentCmdInvalMsgs;
    InvalidationMessageList priorCmdInvalMsgs;
} TransactionContext;

// 全局变量
static int g_backendId = -1;
static uint32_t g_dbId = 0;
static LocalCache g_relCache;
static LocalCache g_sysCache;
static TransactionContext g_transaction;
static volatile sig_atomic_t g_hasNewMessages = 0;
static sem_t *g_sem = NULL;

// 信号处理函数，处理SIGUSR1信号（缓存失效通知）
void handle_invalidation_signal(int signo) {
    if (signo == SIGUSR1) {
        g_hasNewMessages = 1;
    }
}

// 初始化本地缓存
void init_local_cache(LocalCache *cache) {
    memset(cache, 0, sizeof(LocalCache));
}

// 向本地缓存添加项
void add_cache_item(LocalCache *cache, uint32_t key, const char *value) {
    for (int i = 0; i < cache->count; i++) {
        if (cache->items[i].key == key) {
            strncpy(cache->items[i].value, value, sizeof(cache->items[i].value) - 1);
            cache->items[i].status = CACHE_VALID;
            return;
        }
    }
    
    if (cache->count < 100) {
        cache->items[cache->count].key = key;
        strncpy(cache->items[cache->count].value, value, sizeof(cache->items[cache->count].value) - 1);
        cache->items[cache->count].status = CACHE_VALID;
        cache->count++;
    }
}

// 使缓存项失效
void invalidate_cache_item(LocalCache *cache, uint32_t key) {
    for (int i = 0; i < cache->count; i++) {
        if (cache->items[i].key == key) {
            cache->items[i].status = CACHE_INVALID;
            printf("【缓存失效】缓存项 %u 已失效\n", key);
            return;
        }
    }
}

// 使所有缓存项失效
void invalidate_all_cache_items(LocalCache *cache) {
    for (int i = 0; i < cache->count; i++) {
        cache->items[i].status = CACHE_INVALID;
    }
    printf("【缓存失效】所有缓存项已失效\n");
}

// 处理单条失效消息
void process_invalidation_message(InvalidationMessage *msg) {
    printf("【处理失效消息】类型=%d, dbId=%u, relId=%u, hashValue=%u\n", 
           msg->id, msg->dbId, msg->relId, msg->hashValue);
    
    // 只处理与本数据库相关的消息
    if (msg->dbId != 0 && msg->dbId != g_dbId) {
        return;
    }
    
    if (msg->id == CACHE_INVAL_RELCACHE) {
        // 关系缓存失效
        if (msg->relId == 0) {
            invalidate_all_cache_items(&g_relCache);
        } else {
            invalidate_cache_item(&g_relCache, msg->relId);
        }
    } else if (msg->id >= 0) {
        // 系统缓存失效
        invalidate_cache_item(&g_sysCache, msg->hashValue);
    }
}

// 接收并处理失效消息
void accept_invalidation_messages() {
    SharedInvalBuffer *buffer = get_shared_buffer();
    if (buffer == NULL) {
        return;
    }
    
    sem_wait(g_sem);
    
    // 获取后端状态
    BackendState *state = &buffer->backends[g_backendId];
    
    // 检查是否需要重置
    if (state->resetState) {
        state->nextMsgNum = buffer->maxMsgNum;
        state->resetState = 0;
        state->signaled = 0;
        
        sem_post(g_sem);
        
        // 重置本地缓存
        invalidate_all_cache_items(&g_relCache);
        invalidate_all_cache_items(&g_sysCache);
        
        printf("【后端】缓存已重置\n");
        return;
    }
    
    // 获取新消息
    int count = 0;
    InvalidationMessage messages[100];
    
    while (state->nextMsgNum < buffer->maxMsgNum && count < 100) {
        int msgIndex = state->nextMsgNum % MAX_MESSAGES;
        messages[count] = buffer->messages[msgIndex];
        state->nextMsgNum++;
        count++;
    }
    
    // 重置标志
    state->hasMessages = 0;
    g_hasNewMessages = 0;
    
    sem_post(g_sem);
    
    // 处理消息
    if (count > 0) {
        printf("【后端】接收到 %d 条失效消息\n", count);
        for (int i = 0; i < count; i++) {
            process_invalidation_message(&messages[i]);
        }
    } else {
        printf("【后端】没有新的失效消息\n");
    }
}

// 注册后端进程
int register_backend(uint32_t dbId) {
    SharedInvalBuffer *buffer = get_shared_buffer();
    if (buffer == NULL) {
        return -1;
    }
    
    sem_wait(g_sem);
    
    // 分配后端ID
    int backendId = buffer->lastBackendId;
    buffer->lastBackendId++;
    
    // 初始化后端状态
    buffer->backends[backendId].pid = getpid();
    buffer->backends[backendId].nextMsgNum = buffer->maxMsgNum;
    buffer->backends[backendId].resetState = 0;
    buffer->backends[backendId].hasMessages = 0;
    buffer->backends[backendId].signaled = 0;
    buffer->backends[backendId].dbId = dbId;
    
    sem_post(g_sem);
    
    return backendId;
}

// 开始事务
void begin_transaction() {
    // 接收失效消息
    accept_invalidation_messages();
    
    // 初始化事务上下文
    g_transaction.state = TRANS_ACTIVE;
    g_transaction.currentCmdInvalMsgs.count = 0;
    g_transaction.priorCmdInvalMsgs.count = 0;
    
    printf("【事务】开始事务\n");
}

// 添加失效消息到当前命令的消息列表
void add_invalidation_message(InvalidationMessage *msg) {
    if (g_transaction.state != TRANS_ACTIVE) {
        return;
    }
    
    if (g_transaction.currentCmdInvalMsgs.count < 100) {
        g_transaction.currentCmdInvalMsgs.messages[g_transaction.currentCmdInvalMsgs.count] = *msg;
        g_transaction.currentCmdInvalMsgs.count++;
    }
}

// 命令结束时处理失效消息
void command_end_invalidation_messages() {
    if (g_transaction.state != TRANS_ACTIVE) {
        return;
    }
    
    // 处理当前命令的失效消息
    for (int i = 0; i < g_transaction.currentCmdInvalMsgs.count; i++) {
        process_invalidation_message(&g_transaction.currentCmdInvalMsgs.messages[i]);
    }
    
    // 将当前命令的失效消息追加到之前命令的失效消息
    for (int i = 0; i < g_transaction.currentCmdInvalMsgs.count; i++) {
        if (g_transaction.priorCmdInvalMsgs.count < 100) {
            g_transaction.priorCmdInvalMsgs.messages[g_transaction.priorCmdInvalMsgs.count] = 
                g_transaction.currentCmdInvalMsgs.messages[i];
            g_transaction.priorCmdInvalMsgs.count++;
        }
    }
    
    // 清空当前命令的失效消息
    g_transaction.currentCmdInvalMsgs.count = 0;
    
    printf("【事务】命令结束，累积 %d 条失效消息\n", g_transaction.priorCmdInvalMsgs.count);
}

// 发送失效消息到共享队列
void send_shared_invalidation_messages() {
    SharedInvalBuffer *buffer = get_shared_buffer();
    if (buffer == NULL) {
        return;
    }
    
    sem_wait(g_sem);
    
    // 检查是否需要清理队列
    if (buffer->maxMsgNum - buffer->minMsgNum >= buffer->nextThreshold) {
        // 找到所有后端中最小的nextMsgNum
        int newMinMsgNum = buffer->maxMsgNum;
        for (int i = 0; i < buffer->lastBackendId; i++) {
            if (buffer->backends[i].pid > 0 && buffer->backends[i].nextMsgNum < newMinMsgNum) {
                newMinMsgNum = buffer->backends[i].nextMsgNum;
            }
        }
        
        // 更新最小消息编号
        buffer->minMsgNum = newMinMsgNum;
        
        // 如果某个后端落后太多，将其标记为需要重置
        for (int i = 0; i < buffer->lastBackendId; i++) {
            if (buffer->backends[i].pid > 0 && 
                buffer->maxMsgNum - buffer->backends[i].nextMsgNum > MAX_MESSAGES / 2) {
                buffer->backends[i].resetState = 1;
                
                // 发送信号通知后端
                if (!buffer->backends[i].signaled) {
                    buffer->backends[i].signaled = 1;
                    kill(buffer->backends[i].pid, SIGUSR1);
                }
            }
        }
        
        // 更新下一个清理阈值
        buffer->nextThreshold = buffer->maxMsgNum - buffer->minMsgNum + 10;
    }
    
    // 发送失效消息
    for (int i = 0; i < g_transaction.priorCmdInvalMsgs.count; i++) {
        // 将消息添加到环形缓冲区
        int msgIndex = buffer->maxMsgNum % MAX_MESSAGES;
        buffer->messages[msgIndex] = g_transaction.priorCmdInvalMsgs.messages[i];
        buffer->maxMsgNum++;
        
        // 标记所有后端有新消息
        for (int j = 0; j < buffer->lastBackendId; j++) {
            if (buffer->backends[j].pid > 0 && j != g_backendId) {
                buffer->backends[j].hasMessages = 1;
                
                // 发送信号通知后端
                if (!buffer->backends[j].signaled) {
                    buffer->backends[j].signaled = 1;
                    kill(buffer->backends[j].pid, SIGUSR1);
                }
            }
        }
    }
    
    // 释放锁，将信号量值加1，允许其他等待的进程获取锁。
    sem_post(g_sem);
}

// 提交事务
void commit_transaction() {
    if (g_transaction.state != TRANS_ACTIVE) {
        printf("【事务】没有活动事务可提交\n");
        return;
    }
    
    // 将当前命令的失效消息追加到之前命令的失效消息
    for (int i = 0; i < g_transaction.currentCmdInvalMsgs.count; i++) {
        if (g_transaction.priorCmdInvalMsgs.count < 100) {
            g_transaction.priorCmdInvalMsgs.messages[g_transaction.priorCmdInvalMsgs.count] = 
                g_transaction.currentCmdInvalMsgs.messages[i];
            g_transaction.priorCmdInvalMsgs.count++;
        }
    }
    
    // 发送失效消息到共享队列
    send_shared_invalidation_messages();
    
    // 结束事务
    g_transaction.state = TRANS_IDLE;
    g_transaction.currentCmdInvalMsgs.count = 0;
    g_transaction.priorCmdInvalMsgs.count = 0;
    
    printf("【事务】提交事务\n");
}

// 回滚事务
void rollback_transaction() {
    if (g_transaction.state != TRANS_ACTIVE) {
        printf("【事务】没有活动事务可回滚\n");
        return;
    }
    
    // 在回滚时，只在本地处理之前命令的失效消息，不发送到共享队列
    for (int i = 0; i < g_transaction.priorCmdInvalMsgs.count; i++) {
        process_invalidation_message(&g_transaction.priorCmdInvalMsgs.messages[i]);
    }
    
    // 结束事务
    g_transaction.state = TRANS_IDLE;
    g_transaction.currentCmdInvalMsgs.count = 0;
    g_transaction.priorCmdInvalMsgs.count = 0;
    
    printf("【事务】回滚事务\n");
}

// 注册关系缓存失效
void register_relcache_invalidation(uint32_t relId) {
    if (g_transaction.state != TRANS_ACTIVE) {
        return;
    }
    
    InvalidationMessage msg;
    msg.id = CACHE_INVAL_RELCACHE;
    msg.dbId = g_dbId;
    msg.relId = relId;
    msg.hashValue = 0;
    
    add_invalidation_message(&msg);
    
    printf("【事务】注册关系缓存失效: relId=%u\n", relId);
}

// 注册系统缓存失效
void register_syscache_invalidation(uint32_t cacheId, uint32_t hashValue) {
    if (g_transaction.state != TRANS_ACTIVE) {
        return;
    }
    
    InvalidationMessage msg;
    msg.id = cacheId;
    msg.dbId = g_dbId;
    msg.relId = 0;
    msg.hashValue = hashValue;
    
    add_invalidation_message(&msg);
    
    printf("【事务】注册系统缓存失效: cacheId=%u, hashValue=%u\n", cacheId, hashValue);
}

// 打印缓存状态
void print_cache_status() {
    printf("【后端 %d】关系缓存状态:\n", g_backendId);
    for (int i = 0; i < g_relCache.count; i++) {
        printf("  键: %u, 状态: %s, 值: %s\n", 
               g_relCache.items[i].key, 
               g_relCache.items[i].status == CACHE_VALID ? "有效" : "无效",
               g_relCache.items[i].value);
    }
    
    printf("【后端 %d】系统缓存状态:\n", g_backendId);
    for (int i = 0; i < g_sysCache.count; i++) {
        printf("  键: %u, 状态: %s, 值: %s\n", 
               g_sysCache.items[i].key, 
               g_sysCache.items[i].status == CACHE_VALID ? "有效" : "无效",
               g_sysCache.items[i].value);
    }
}

// 后端进程清理
void cleanup_backend() {
    // 释放共享内存映射
    detach_shared_buffer();
    
    // 关闭信号量
    if (g_sem != NULL) {
        sem_close(g_sem);
        g_sem = NULL;
    }
    
    printf("【后端】清理后端进程 %d (PID %d)\n", 
           g_backendId, getpid());
}

// 初始化后端进程
void init_backend(uint32_t dbId) {
    // 注册信号处理函数
    struct sigaction sa;
    sa.sa_handler = handle_invalidation_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    // 初始化本地缓存
    init_local_cache(&g_relCache);
    init_local_cache(&g_sysCache);
    
    // 初始化事务上下文
    g_transaction.state = TRANS_IDLE;
    g_transaction.currentCmdInvalMsgs.count = 0;
    g_transaction.priorCmdInvalMsgs.count = 0;
    
    // 设置数据库ID
    g_dbId = dbId;
    
    // 打开信号量
    g_sem = sem_open(SEM_NAME, 0);
    if (g_sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    
    // 映射共享内存
    if (attach_shared_buffer() == NULL) {
        fprintf(stderr, "【错误】无法映射共享内存\n");
        sem_close(g_sem);
        exit(1);
    }
    
    // 注册后端
    g_backendId = register_backend(dbId);
    
    printf("【后端】初始化后端进程 %d (PID %d), 数据库ID %u\n", 
           g_backendId, getpid(), g_dbId);
    
    // 注册退出处理函数
    atexit(cleanup_backend);
}

#endif // BACKEND_PROCESS_H
