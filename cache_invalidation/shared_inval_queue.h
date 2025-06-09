#ifndef SHARED_INVAL_QUEUE_H
#define SHARED_INVAL_QUEUE_H

#include <vector>
#include <mutex>
#include <map>
#include <algorithm>
#include "invalidation_message.h"

// 模拟每个后端进程的状态
struct BackendState {
    int nextMsgNum;       // 下一个要处理的消息编号
    bool resetState;      // 是否需要重置
    bool hasMessages;     // 是否有新消息
    bool signaled;        // 是否已发送信号
    int procPid;          // 进程ID
    
    BackendState() : nextMsgNum(0), resetState(false), 
                     hasMessages(false), signaled(false), procPid(0) {}
};

// 模拟PostgreSQL的共享失效队列
class SharedInvalQueue {
private:
    static constexpr int MAX_MESSAGES = 1024;  // 最大消息数量
    static constexpr int CLEANUP_THRESHOLD = 100;  // 清理阈值
    
    std::vector<InvalidationMessage> buffer;   // 消息缓冲区
    std::map<int, BackendState> backendStates; // 后端状态
    int minMsgNum;                            // 最小消息编号
    int maxMsgNum;                            // 最大消息编号
    std::mutex queueMutex;                    // 保护队列的互斥锁
    
public:
    SharedInvalQueue() : minMsgNum(0), maxMsgNum(0) {}
    
    // 向队列中插入消息
    void insertMessage(const InvalidationMessage& msg) {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        // 检查队列是否已满，如果满了则进行清理
        if (maxMsgNum - minMsgNum >= MAX_MESSAGES) {
            cleanupQueue();
        }
        
        // 将消息添加到缓冲区
        if (buffer.size() <= (size_t)(maxMsgNum % MAX_MESSAGES)) {
            buffer.push_back(msg);
        } else {
            buffer[maxMsgNum % MAX_MESSAGES] = msg;
        }
        
        // 更新最大消息编号
        maxMsgNum++;
        
        // 标记所有后端有新消息
        for (auto& pair : backendStates) {
            pair.second.hasMessages = true;
        }
    }
    
    // 注册后端进程
    int registerBackend(int pid) {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        // 分配后端ID
        int backendId = backendStates.size() + 1;
        
        // 初始化后端状态
        BackendState state;
        state.nextMsgNum = maxMsgNum;  // 从当前最大消息编号开始
        state.procPid = pid;
        
        backendStates[backendId] = state;
        
        return backendId;
    }
    
    // 获取后端的消息
    std::vector<InvalidationMessage> getMessages(int backendId) {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        std::vector<InvalidationMessage> messages;
        
        if (backendStates.find(backendId) == backendStates.end()) {
            return messages;  // 后端不存在
        }
        
        BackendState& state = backendStates[backendId];
        
        // 检查是否需要重置
        if (state.resetState) {
            state.nextMsgNum = maxMsgNum;
            state.resetState = false;
            state.signaled = false;
            return messages;  // 返回空消息列表，表示需要重置
        }
        
        // 获取后端需要处理的消息
        while (state.nextMsgNum < maxMsgNum) {
            int msgIndex = state.nextMsgNum % MAX_MESSAGES;
            if (msgIndex < buffer.size()) {
                messages.push_back(buffer[msgIndex]);
            }
            state.nextMsgNum++;
        }
        
        // 重置hasMessages标志
        state.hasMessages = false;
        
        return messages;
    }
    
    // 清理队列中已处理的消息
    void cleanupQueue() {
        // 找到所有后端中最小的nextMsgNum
        int newMinMsgNum = maxMsgNum;
        for (const auto& pair : backendStates) {
            newMinMsgNum = std::min(newMinMsgNum, pair.second.nextMsgNum);
        }
        
        // 更新最小消息编号
        minMsgNum = newMinMsgNum;
        
        // 如果某个后端落后太多，将其标记为需要重置
        for (auto& pair : backendStates) {
            if (maxMsgNum - pair.second.nextMsgNum > MAX_MESSAGES / 2) {
                pair.second.resetState = true;
                pair.second.signaled = true;
            }
        }
    }
    
    // 获取后端状态信息（用于调试）
    std::string getBackendStateInfo(int backendId) {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        if (backendStates.find(backendId) == backendStates.end()) {
            return "Backend " + std::to_string(backendId) + " not found";
        }
        
        const BackendState& state = backendStates[backendId];
        
        return "Backend " + std::to_string(backendId) + 
               " (PID " + std::to_string(state.procPid) + "): " +
               "nextMsgNum=" + std::to_string(state.nextMsgNum) + 
               ", resetState=" + (state.resetState ? "true" : "false") +
               ", hasMessages=" + (state.hasMessages ? "true" : "false") +
               ", signaled=" + (state.signaled ? "true" : "false");
    }
    
    // 获取队列状态信息（用于调试）
    std::string getQueueInfo() {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        return "Queue: minMsgNum=" + std::to_string(minMsgNum) + 
               ", maxMsgNum=" + std::to_string(maxMsgNum) + 
               ", messageCount=" + std::to_string(maxMsgNum - minMsgNum) +
               ", backendCount=" + std::to_string(backendStates.size());
    }
};

#endif // SHARED_INVAL_QUEUE_H
