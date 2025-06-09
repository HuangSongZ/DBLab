#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <vector>
#include <memory>
#include "invalidation_message.h"

// 模拟事务中的缓存失效消息列表
class InvalidationMessageList {
private:
    std::vector<InvalidationMessage> messages;
    
public:
    // 添加消息到列表
    void addMessage(const InvalidationMessage& msg) {
        messages.push_back(msg);
    }
    
    // 获取所有消息
    const std::vector<InvalidationMessage>& getMessages() const {
        return messages;
    }
    
    // 清空消息列表
    void clear() {
        messages.clear();
    }
    
    // 将一个列表的消息追加到另一个列表
    void appendFrom(const InvalidationMessageList& other) {
        messages.insert(messages.end(), other.messages.begin(), other.messages.end());
    }
    
    // 获取消息数量
    size_t size() const {
        return messages.size();
    }
};

// 模拟PostgreSQL的事务
class Transaction {
private:
    bool inProgress;
    InvalidationMessageList currentCmdInvalidMsgs;  // 当前命令的失效消息
    InvalidationMessageList priorCmdInvalidMsgs;    // 之前命令的失效消息
    
public:
    Transaction() : inProgress(false) {}
    
    // 开始事务
    void begin() {
        inProgress = true;
        currentCmdInvalidMsgs.clear();
        priorCmdInvalidMsgs.clear();
        std::cout << "【事务】开始事务" << std::endl;
    }
    
    // 提交事务
    std::vector<InvalidationMessage> commit() {
        if (!inProgress) {
            return {};
        }
        
        // 模拟AtEOXact_Inval(true)
        // 将当前命令的失效消息追加到之前命令的失效消息
        priorCmdInvalidMsgs.appendFrom(currentCmdInvalidMsgs);
        
        // 获取所有需要发送的失效消息
        std::vector<InvalidationMessage> messagesToSend = priorCmdInvalidMsgs.getMessages();
        
        // 结束事务
        inProgress = false;
        currentCmdInvalidMsgs.clear();
        priorCmdInvalidMsgs.clear();
        
        std::cout << "【事务】提交事务，生成 " << messagesToSend.size() << " 条失效消息" << std::endl;
        
        return messagesToSend;
    }
    
    // 回滚事务
    void rollback() {
        if (!inProgress) {
            return;
        }
        
        // 模拟AtEOXact_Inval(false)
        // 在回滚时，只在本地处理之前命令的失效消息，不发送到共享队列
        
        // 结束事务
        inProgress = false;
        currentCmdInvalidMsgs.clear();
        priorCmdInvalidMsgs.clear();
        
        std::cout << "【事务】回滚事务" << std::endl;
    }
    
    // 命令结束时处理失效消息
    void commandEnd() {
        if (!inProgress) {
            return;
        }
        
        // 模拟CommandEndInvalidationMessages
        // 将当前命令的失效消息追加到之前命令的失效消息
        priorCmdInvalidMsgs.appendFrom(currentCmdInvalidMsgs);
        currentCmdInvalidMsgs.clear();
        
        std::cout << "【事务】命令结束，累积 " << priorCmdInvalidMsgs.size() << " 条失效消息" << std::endl;
    }
    
    // 注册关系缓存失效
    void registerRelcacheInvalidation(uint32_t dbId, uint32_t relId) {
        if (!inProgress) {
            return;
        }
        
        InvalidationMessage msg = InvalidationMessage::createRelcacheInval(dbId, relId);
        currentCmdInvalidMsgs.addMessage(msg);
        
        std::cout << "【事务】注册关系缓存失效: dbId=" << dbId << ", relId=" << relId << std::endl;
    }
    
    // 注册系统缓存失效
    void registerSyscacheInvalidation(uint32_t dbId, uint32_t cacheId, uint32_t hashValue) {
        if (!inProgress) {
            return;
        }
        
        InvalidationMessage msg = InvalidationMessage::createSyscacheInval(dbId, cacheId, hashValue);
        currentCmdInvalidMsgs.addMessage(msg);
        
        std::cout << "【事务】注册系统缓存失效: dbId=" << dbId << ", cacheId=" << cacheId << ", hashValue=" << hashValue << std::endl;
    }
    
    // 检查事务是否在进行中
    bool isInProgress() const {
        return inProgress;
    }
};

#endif // TRANSACTION_H
