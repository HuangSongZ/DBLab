#ifndef INVALIDATION_MESSAGE_H
#define INVALIDATION_MESSAGE_H

#include <cstdint>
#include <string>

// 消息类型常量，类似于PostgreSQL中的定义
enum InvalidationMessageType {
    CACHE_INVAL_CATCACHE = 0,     // 系统缓存失效
    CACHE_INVAL_RELCACHE = -1,    // 关系缓存失效
    CACHE_INVAL_SYSCACHE = -2,    // 系统目录缓存失效
    CACHE_INVAL_SNAPSHOT = -3     // 快照失效
};

// 模拟PostgreSQL的SharedInvalidationMessage
struct InvalidationMessage {
    int8_t id;                    // 消息类型
    uint32_t dbId;                // 数据库ID
    uint32_t relId;               // 关系ID或目录ID
    uint32_t hashValue;           // 哈希值（用于系统缓存）
    
    // 构造函数
    InvalidationMessage() : id(0), dbId(0), relId(0), hashValue(0) {}
    
    // 创建关系缓存失效消息
    static InvalidationMessage createRelcacheInval(uint32_t dbId, uint32_t relId) {
        InvalidationMessage msg;
        msg.id = CACHE_INVAL_RELCACHE;
        msg.dbId = dbId;
        msg.relId = relId;
        return msg;
    }
    
    // 创建系统缓存失效消息
    static InvalidationMessage createSyscacheInval(uint32_t dbId, uint32_t cacheId, uint32_t hashValue) {
        InvalidationMessage msg;
        msg.id = cacheId; // 在PostgreSQL中，正值表示系统缓存ID
        msg.dbId = dbId;
        msg.hashValue = hashValue;
        return msg;
    }
    
    // 消息的字符串表示，用于调试
    std::string toString() const {
        std::string typeStr;
        switch (id) {
            case CACHE_INVAL_RELCACHE:
                typeStr = "RELCACHE";
                break;
            case CACHE_INVAL_SYSCACHE:
                typeStr = "SYSCACHE";
                break;
            case CACHE_INVAL_SNAPSHOT:
                typeStr = "SNAPSHOT";
                break;
            default:
                typeStr = "CATCACHE(" + std::to_string(id) + ")";
        }
        
        return "InvalidationMessage{type=" + typeStr + 
               ", dbId=" + std::to_string(dbId) + 
               ", relId=" + std::to_string(relId) + 
               ", hashValue=" + std::to_string(hashValue) + "}";
    }
};

#endif // INVALIDATION_MESSAGE_H
