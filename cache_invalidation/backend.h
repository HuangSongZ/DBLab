#ifndef BACKEND_H
#define BACKEND_H

#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include "shared_inval_queue.h"
#include "transaction.h"
#include "cache.h"

// 模拟PostgreSQL的后端进程
class Backend {
private:
    int backendId;
    int pid;
    uint32_t databaseId;
    std::shared_ptr<SharedInvalQueue> sharedQueue;
    std::unique_ptr<Transaction> currentTransaction;
    
    // 模拟关系缓存
    Cache<uint32_t, std::string> relCache;
    
    // 模拟系统缓存
    Cache<uint32_t, std::string> sysCache;
    
public:
    Backend(std::shared_ptr<SharedInvalQueue> queue, uint32_t dbId)
        : backendId(0), databaseId(dbId), sharedQueue(queue),
          currentTransaction(new Transaction()),
          relCache("关系"), sysCache("系统") {
        
        // 生成进程ID
        pid = static_cast<int>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        
        // 注册后端
        backendId = sharedQueue->registerBackend(pid);
        
        std::cout << "【后端】创建后端进程 " << backendId << " (PID " << pid << "), 数据库ID " << databaseId << std::endl;
    }
    
    // 开始事务
    void beginTransaction() {
        // 在事务开始时接收失效消息
        acceptInvalidationMessages();
        
        // 开始新事务
        currentTransaction->begin();
    }
    
    // 提交事务
    void commitTransaction() {
        if (!currentTransaction->isInProgress()) {
            std::cout << "【后端】没有活动事务可提交" << std::endl;
            return;
        }
        
        // 获取事务中生成的失效消息
        std::vector<InvalidationMessage> messages = currentTransaction->commit();
        
        // 将失效消息发送到共享队列
        for (const auto& msg : messages) {
            sharedQueue->insertMessage(msg);
            std::cout << "【后端】发送失效消息: " << msg.toString() << std::endl;
        }
    }
    
    // 回滚事务
    void rollbackTransaction() {
        currentTransaction->rollback();
    }
    
    // 执行命令
    void executeCommand() {
        if (!currentTransaction->isInProgress()) {
            std::cout << "【后端】没有活动事务，无法执行命令" << std::endl;
            return;
        }
        
        // 模拟命令执行结束时的处理
        currentTransaction->commandEnd();
    }
    
    // 接收并处理失效消息
    void acceptInvalidationMessages() {
        std::vector<InvalidationMessage> messages = sharedQueue->getMessages(backendId);
        
        if (messages.empty()) {
            std::cout << "【后端】没有新的失效消息" << std::endl;
            return;
        }
        
        std::cout << "【后端】接收到 " << messages.size() << " 条失效消息" << std::endl;
        
        // 处理失效消息
        for (const auto& msg : messages) {
            std::cout << "【后端】处理失效消息: " << msg.toString() << std::endl;
            
            // 根据消息类型处理
            if (msg.id == CACHE_INVAL_RELCACHE) {
                // 关系缓存失效
                relCache.processInvalidationMessage(msg);
            } else if (msg.id >= 0) {
                // 系统缓存失效
                sysCache.processInvalidationMessage(msg);
            }
        }
    }
    
    // 向关系缓存中添加项
    void addRelCacheEntry(uint32_t relId, const std::string& relName) {
        relCache.put(relId, relName);
        std::cout << "【后端】添加关系缓存项: relId=" << relId << ", name=" << relName << std::endl;
    }
    
    // 向系统缓存中添加项
    void addSysCacheEntry(uint32_t hashValue, const std::string& value) {
        sysCache.put(hashValue, value);
        std::cout << "【后端】添加系统缓存项: hashValue=" << hashValue << ", value=" << value << std::endl;
    }
    
    // 注册关系缓存失效
    void registerRelcacheInvalidation(uint32_t relId) {
        currentTransaction->registerRelcacheInvalidation(databaseId, relId);
    }
    
    // 注册系统缓存失效
    void registerSyscacheInvalidation(uint32_t cacheId, uint32_t hashValue) {
        currentTransaction->registerSyscacheInvalidation(databaseId, cacheId, hashValue);
    }
    
    // 获取关系缓存中的项
    std::string* getRelCacheEntry(uint32_t relId) {
        return relCache.get(relId);
    }
    
    // 获取系统缓存中的项
    std::string* getSysCacheEntry(uint32_t hashValue) {
        return sysCache.get(hashValue);
    }
    
    // 打印缓存状态
    void printCacheStatus() {
        std::cout << "【后端 " << backendId << "】缓存状态:" << std::endl;
        relCache.printContents();
        sysCache.printContents();
    }
    
    // 获取后端ID
    int getBackendId() const {
        return backendId;
    }
    
    // 获取数据库ID
    uint32_t getDatabaseId() const {
        return databaseId;
    }
};

#endif // BACKEND_H
