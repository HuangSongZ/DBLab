#ifndef RESOURCE_OWNER_H
#define RESOURCE_OWNER_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cassert>

// 资源类型枚举
enum class ResourceType {
    BUFFER,
    RELATION,
    FILE,
    SNAPSHOT,
    LOCK
};

// 资源基类
class Resource {
public:
    virtual ~Resource() = default;
    virtual ResourceType getType() const = 0;
    virtual std::string toString() const = 0;
};

// 缓冲区资源
class BufferResource : public Resource {
private:
    int bufferId;
public:
    BufferResource(int id) : bufferId(id) {}
    ResourceType getType() const override { return ResourceType::BUFFER; }
    int getId() const { return bufferId; }
    std::string toString() const override { 
        return "Buffer #" + std::to_string(bufferId); 
    }
};

// 关系资源
class RelationResource : public Resource {
private:
    std::string relationName;
public:
    RelationResource(const std::string& name) : relationName(name) {}
    ResourceType getType() const override { return ResourceType::RELATION; }
    std::string getName() const { return relationName; }
    std::string toString() const override { 
        return "Relation '" + relationName + "'"; 
    }
};

// 文件资源
class FileResource : public Resource {
private:
    int fileDescriptor;
public:
    FileResource(int fd) : fileDescriptor(fd) {}
    ResourceType getType() const override { return ResourceType::FILE; }
    int getFd() const { return fileDescriptor; }
    std::string toString() const override { 
        return "File descriptor " + std::to_string(fileDescriptor); 
    }
};

// 快照资源
class SnapshotResource : public Resource {
private:
    int snapshotId;
public:
    SnapshotResource(int id) : snapshotId(id) {}
    ResourceType getType() const override { return ResourceType::SNAPSHOT; }
    int getId() const { return snapshotId; }
    std::string toString() const override { 
        return "Snapshot #" + std::to_string(snapshotId); 
    }
};

// 锁资源
class LockResource : public Resource {
private:
    std::string lockName;
public:
    LockResource(const std::string& name) : lockName(name) {}
    ResourceType getType() const override { return ResourceType::LOCK; }
    std::string getName() const { return lockName; }
    std::string toString() const override { 
        return "Lock '" + lockName + "'"; 
    }
};

// 资源数组类，用于存储特定类型的资源
class ResourceArray {
private:
    std::vector<std::shared_ptr<Resource>> items;
    ResourceType type;

public:
    ResourceArray(ResourceType t) : type(t) {}

    // 添加资源
    void add(std::shared_ptr<Resource> resource) {
        assert(resource->getType() == type);
        items.push_back(resource);
    }

    // 移除资源
    bool remove(std::shared_ptr<Resource> resource) {
        for (auto it = items.begin(); it != items.end(); ++it) {
            if (*it == resource) {
                items.erase(it);
                return true;
            }
        }
        return false;
    }

    // 获取资源数量
    size_t size() const {
        return items.size();
    }

    // 获取所有资源
    const std::vector<std::shared_ptr<Resource>>& getItems() const {
        return items;
    }

    // 清空资源
    void clear() {
        items.clear();
    }
};

// 资源所有者类
class ResourceOwner {
private:
    std::string name;
    ResourceOwner* parent;
    std::vector<ResourceOwner*> children;
    
    // 各类资源数组
    ResourceArray bufferArray;
    ResourceArray relationArray;
    ResourceArray fileArray;
    ResourceArray snapshotArray;
    ResourceArray lockArray;

public:
    ResourceOwner(const std::string& ownerName, ResourceOwner* parentOwner = nullptr);
    ~ResourceOwner();

    // 获取名称
    const std::string& getName() const { return name; }

    // 获取父所有者
    ResourceOwner* getParent() const { return parent; }

    // 添加子所有者
    void addChild(ResourceOwner* child);

    // 移除子所有者
    void removeChild(ResourceOwner* child);

    // 获取子所有者列表
    const std::vector<ResourceOwner*>& getChildren() const { return children; }

    // 记住资源
    void rememberBuffer(std::shared_ptr<BufferResource> buffer);
    void rememberRelation(std::shared_ptr<RelationResource> relation);
    void rememberFile(std::shared_ptr<FileResource> file);
    void rememberSnapshot(std::shared_ptr<SnapshotResource> snapshot);
    void rememberLock(std::shared_ptr<LockResource> lock);

    // 忘记资源
    bool forgetBuffer(std::shared_ptr<BufferResource> buffer);
    bool forgetRelation(std::shared_ptr<RelationResource> relation);
    bool forgetFile(std::shared_ptr<FileResource> file);
    bool forgetSnapshot(std::shared_ptr<SnapshotResource> snapshot);
    bool forgetLock(std::shared_ptr<LockResource> lock);

    // 释放所有资源
    void releaseAllResources(bool isCommit);

    // 打印资源信息
    void printResources() const;
};

// 全局资源所有者
extern ResourceOwner* CurrentResourceOwner;
extern ResourceOwner* TopTransactionResourceOwner;

// 初始化资源所有者系统
void initResourceOwnerSystem();

// 清理资源所有者系统
void cleanupResourceOwnerSystem();

// 开始事务
void startTransaction();

// 提交事务
void commitTransaction();

// 回滚事务
void abortTransaction();

// 创建保存点
ResourceOwner* createSavepoint(const std::string& name);

// 回滚到保存点
void rollbackToSavepoint(ResourceOwner* savepoint);

// 释放保存点
void releaseSavepoint(ResourceOwner* savepoint);

#endif // RESOURCE_OWNER_H
