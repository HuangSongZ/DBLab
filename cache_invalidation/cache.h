#ifndef CACHE_H
#define CACHE_H

#include <map>
#include <string>
#include <iostream>
#include <functional>
#include "invalidation_message.h"

// 模拟缓存项
template <typename KeyType, typename ValueType>
class CacheEntry {
public:
    KeyType key;
    ValueType value;
    bool valid;
    
    CacheEntry() : valid(false) {}
    CacheEntry(const KeyType& k, const ValueType& v) : key(k), value(v), valid(true) {}
};

// 模拟缓存回调函数类型
using CacheCallback = std::function<void(uint32_t)>;

// 模拟PostgreSQL的缓存
template <typename KeyType, typename ValueType>
class Cache {
private:
    std::string cacheName;
    std::map<KeyType, CacheEntry<KeyType, ValueType>> entries;
    std::vector<CacheCallback> callbacks;
    
public:
    Cache(const std::string& name) : cacheName(name) {}
    
    // 向缓存中添加或更新项
    void put(const KeyType& key, const ValueType& value) {
        entries[key] = CacheEntry<KeyType, ValueType>(key, value);
    }
    
    // 从缓存中获取项
    ValueType* get(const KeyType& key) {
        auto it = entries.find(key);
        if (it != entries.end() && it->second.valid) {
            return &(it->second.value);
        }
        return nullptr;
    }
    
    // 使缓存项失效
    void invalidate(const KeyType& key) {
        auto it = entries.find(key);
        if (it != entries.end()) {
            it->second.valid = false;
            std::cout << "【缓存失效】" << cacheName << " 缓存项 " << key << " 已失效" << std::endl;
        }
    }
    
    // 使所有缓存项失效
    void invalidateAll() {
        for (auto& pair : entries) {
            pair.second.valid = false;
        }
        std::cout << "【缓存失效】" << cacheName << " 所有缓存项已失效" << std::endl;
    }
    
    // 处理失效消息
    void processInvalidationMessage(const InvalidationMessage& msg) {
        if (msg.id == CACHE_INVAL_RELCACHE) {
            // 关系缓存失效
            if (msg.relId == 0) {
                // relId为0表示使所有缓存项失效
                invalidateAll();
            } else {
                // 使特定关系的缓存项失效
                invalidate(msg.relId);
            }
            
            // 调用注册的回调函数
            for (const auto& callback : callbacks) {
                callback(msg.relId);
            }
        } else if (msg.id >= 0) {
            // 系统缓存失效
            invalidate(msg.hashValue);
        }
    }
    
    // 注册缓存失效回调函数
    void registerCallback(CacheCallback callback) {
        callbacks.push_back(callback);
    }
    
    // 获取缓存状态信息（用于调试）
    std::string getInfo() const {
        std::string info = cacheName + " 缓存: ";
        info += std::to_string(entries.size()) + " 项, ";
        
        int validCount = 0;
        for (const auto& pair : entries) {
            if (pair.second.valid) {
                validCount++;
            }
        }
        
        info += std::to_string(validCount) + " 有效项";
        return info;
    }
    
    // 打印缓存内容
    void printContents() const {
        std::cout << "【缓存内容】" << cacheName << " 缓存:" << std::endl;
        for (const auto& pair : entries) {
            std::cout << "  键: " << pair.first 
                      << ", 状态: " << (pair.second.valid ? "有效" : "无效")
                      << ", 值: " << pair.second.value << std::endl;
        }
    }
};

#endif // CACHE_H
