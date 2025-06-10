# 多进程缓存失效机制演示

这个改进版演示程序使用真正的多进程和共享内存来模拟PostgreSQL的缓存失效机制。

## 设计方案

### 1. 共享内存实现

使用POSIX共享内存（`shm_open`、`mmap`）创建一个共享内存区域，包含：

```c
// 共享内存结构
typedef struct {
    int minMsgNum;                   // 最小消息编号
    int maxMsgNum;                   // 最大消息编号
    pthread_mutex_t mutex;           // 互斥锁，保护共享数据
    pthread_cond_t cond;             // 条件变量，用于通知
    int lastBackendId;               // 最后分配的后端ID
    BackendState backends[MAX_BACKENDS]; // 后端状态数组
    InvalidationMessage messages[MAX_MESSAGES]; // 消息环形缓冲区
} SharedInvalBuffer;
```

### 2. 多进程架构

1. **主进程**：
   - 初始化共享内存
   - 创建后端进程
   - 协调演示流程

2. **后端进程**：
   - 模拟PostgreSQL后端进程
   - 维护本地缓存
   - 生成和处理缓存失效消息

### 3. 进程间通信

1. **共享内存**：用于存储缓存失效消息和后端状态
2. **信号**：用于通知后端处理新的失效消息
3. **命名管道**：用于主进程和后端进程之间的命令通信

## 实现细节

### 共享内存管理

```c
// 初始化共享内存
int initSharedMemory() {
    int fd = shm_open("/pg_cache_inval_demo", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        return -1;
    }
    
    // 设置共享内存大小
    if (ftruncate(fd, sizeof(SharedInvalBuffer)) == -1) {
        perror("ftruncate");
        return -1;
    }
    
    // 映射共享内存
    SharedInvalBuffer *buffer = mmap(NULL, sizeof(SharedInvalBuffer),
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    
    // 初始化共享内存
    memset(buffer, 0, sizeof(SharedInvalBuffer));
    
    // 初始化互斥锁和条件变量
    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&buffer->mutex, &mutexAttr);
    pthread_mutexattr_destroy(&mutexAttr);
    
    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&buffer->cond, &condAttr);
    pthread_condattr_destroy(&condAttr);
    
    return fd;
}
```

### 后端进程实现

```c
// 后端进程主函数
void backendProcess(int backendId, int databaseId) {
    // 映射共享内存
    int fd = shm_open("/pg_cache_inval_demo", O_RDWR, 0666);
    SharedInvalBuffer *buffer = mmap(NULL, sizeof(SharedInvalBuffer),
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    // 初始化本地缓存
    LocalCache relCache;
    LocalCache sysCache;
    
    // 注册信号处理函数，用于接收缓存失效通知
    signal(SIGUSR1, handleInvalidationSignal);
    
    // 主循环，处理命令
    while (1) {
        Command cmd = receiveCommand();
        
        switch (cmd.type) {
            case CMD_BEGIN_TRANSACTION:
                // 接收并处理失效消息
                acceptInvalidationMessages(buffer, backendId, &relCache, &sysCache);
                // 开始事务
                beginTransaction();
                break;
                
            case CMD_COMMIT_TRANSACTION:
                // 提交事务，发送失效消息
                commitTransaction(buffer, backendId, databaseId);
                break;
                
            // 其他命令处理...
            
            case CMD_EXIT:
                // 清理并退出
                munmap(buffer, sizeof(SharedInvalBuffer));
                close(fd);
                exit(0);
                break;
        }
    }
}
```

### 缓存失效消息处理

```c
// 发送缓存失效消息
void sendInvalidationMessage(SharedInvalBuffer *buffer, InvalidationMessage *msg) {
    pthread_mutex_lock(&buffer->mutex);
    
    // 检查是否需要清理队列
    if (buffer->maxMsgNum - buffer->minMsgNum >= MAX_MESSAGES) {
        cleanupQueue(buffer);
    }
    
    // 添加消息到环形缓冲区
    int index = buffer->maxMsgNum % MAX_MESSAGES;
    buffer->messages[index] = *msg;
    buffer->maxMsgNum++;
    
    // 标记所有后端有新消息，并发送信号
    for (int i = 0; i < MAX_BACKENDS; i++) {
        if (buffer->backends[i].pid > 0) {
            buffer->backends[i].hasMessages = true;
            kill(buffer->backends[i].pid, SIGUSR1);
        }
    }
    
    pthread_mutex_unlock(&buffer->mutex);
}

// 接收并处理缓存失效消息
void acceptInvalidationMessages(SharedInvalBuffer *buffer, int backendId,
                               LocalCache *relCache, LocalCache *sysCache) {
    pthread_mutex_lock(&buffer->mutex);
    
    BackendState *state = &buffer->backends[backendId];
    
    // 检查是否需要重置
    if (state->resetState) {
        state->nextMsgNum = buffer->maxMsgNum;
        state->resetState = false;
        pthread_mutex_unlock(&buffer->mutex);
        
        // 重置本地缓存
        resetLocalCaches(relCache, sysCache);
        return;
    }
    
    // 获取并处理消息
    while (state->nextMsgNum < buffer->maxMsgNum) {
        int index = state->nextMsgNum % MAX_MESSAGES;
        InvalidationMessage msg = buffer->messages[index];
        state->nextMsgNum++;
        
        // 在持有锁的情况下复制消息
        InvalidationMessage msgCopy = msg;
        
        // 释放锁，处理消息
        pthread_mutex_unlock(&buffer->mutex);
        processInvalidationMessage(&msgCopy, relCache, sysCache);
        pthread_mutex_lock(&buffer->mutex);
    }
    
    state->hasMessages = false;
    pthread_mutex_unlock(&buffer->mutex);
}
```

## 演示场景

1. **基本缓存失效流程**：
   - 启动多个后端进程
   - 一个后端修改数据并提交事务
   - 观察其他后端如何接收并处理失效消息

2. **事务回滚**：
   - 一个后端开始事务并修改数据
   - 回滚事务，观察失效消息不会发送

3. **缓存队列溢出**：
   - 生成大量失效消息，导致队列溢出
   - 观察系统如何处理队列清理和后端重置

## 编译和运行

```bash
# 编译
gcc -o pg_multi_demo pg_multi_demo.c -lrt -pthread

# 运行
./pg_multi_demo
```
