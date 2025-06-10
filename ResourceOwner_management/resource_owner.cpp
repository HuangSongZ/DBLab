#include "resource_owner.h"

// 全局资源所有者
ResourceOwner* CurrentResourceOwner = nullptr;
ResourceOwner* TopTransactionResourceOwner = nullptr;

// 资源所有者构造函数
ResourceOwner::ResourceOwner(const std::string& ownerName, ResourceOwner* parentOwner)
    : name(ownerName), 
      parent(parentOwner),
      bufferArray(ResourceType::BUFFER),
      relationArray(ResourceType::RELATION),
      fileArray(ResourceType::FILE),
      snapshotArray(ResourceType::SNAPSHOT),
      lockArray(ResourceType::LOCK) {
    
    // 如果有父所有者，将自己添加为其子所有者
    if (parent) {
        parent->addChild(this);
    }
}

// 资源所有者析构函数
ResourceOwner::~ResourceOwner() {
    // 释放所有资源
    releaseAllResources(false);
    
    // 从父所有者中移除自己
    if (parent) {
        parent->removeChild(this);
    }
    
    // 删除所有子所有者
    while (!children.empty()) {
        ResourceOwner* child = children.back();
        delete child; // 这会递归调用析构函数
    }
}

// 添加子所有者
void ResourceOwner::addChild(ResourceOwner* child) {
    children.push_back(child);
}

// 移除子所有者
void ResourceOwner::removeChild(ResourceOwner* child) {
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (*it == child) {
            children.erase(it);
            return;
        }
    }
}

// 记住缓冲区资源
void ResourceOwner::rememberBuffer(std::shared_ptr<BufferResource> buffer) {
    bufferArray.add(buffer);
    std::cout << "资源所有者 '" << name << "' 获取了资源: " << buffer->toString() << std::endl;
}

// 记住关系资源
void ResourceOwner::rememberRelation(std::shared_ptr<RelationResource> relation) {
    relationArray.add(relation);
    std::cout << "资源所有者 '" << name << "' 获取了资源: " << relation->toString() << std::endl;
}

// 记住文件资源
void ResourceOwner::rememberFile(std::shared_ptr<FileResource> file) {
    fileArray.add(file);
    std::cout << "资源所有者 '" << name << "' 获取了资源: " << file->toString() << std::endl;
}

// 记住快照资源
void ResourceOwner::rememberSnapshot(std::shared_ptr<SnapshotResource> snapshot) {
    snapshotArray.add(snapshot);
    std::cout << "资源所有者 '" << name << "' 获取了资源: " << snapshot->toString() << std::endl;
}

// 记住锁资源
void ResourceOwner::rememberLock(std::shared_ptr<LockResource> lock) {
    lockArray.add(lock);
    std::cout << "资源所有者 '" << name << "' 获取了资源: " << lock->toString() << std::endl;
}

// 忘记缓冲区资源
bool ResourceOwner::forgetBuffer(std::shared_ptr<BufferResource> buffer) {
    bool result = bufferArray.remove(buffer);
    if (result) {
        std::cout << "资源所有者 '" << name << "' 释放了资源: " << buffer->toString() << std::endl;
    }
    return result;
}

// 忘记关系资源
bool ResourceOwner::forgetRelation(std::shared_ptr<RelationResource> relation) {
    bool result = relationArray.remove(relation);
    if (result) {
        std::cout << "资源所有者 '" << name << "' 释放了资源: " << relation->toString() << std::endl;
    }
    return result;
}

// 忘记文件资源
bool ResourceOwner::forgetFile(std::shared_ptr<FileResource> file) {
    bool result = fileArray.remove(file);
    if (result) {
        std::cout << "资源所有者 '" << name << "' 释放了资源: " << file->toString() << std::endl;
    }
    return result;
}

// 忘记快照资源
bool ResourceOwner::forgetSnapshot(std::shared_ptr<SnapshotResource> snapshot) {
    bool result = snapshotArray.remove(snapshot);
    if (result) {
        std::cout << "资源所有者 '" << name << "' 释放了资源: " << snapshot->toString() << std::endl;
    }
    return result;
}

// 忘记锁资源
bool ResourceOwner::forgetLock(std::shared_ptr<LockResource> lock) {
    bool result = lockArray.remove(lock);
    if (result) {
        std::cout << "资源所有者 '" << name << "' 释放了资源: " << lock->toString() << std::endl;
    }
    return result;
}

// 释放所有资源
void ResourceOwner::releaseAllResources(bool isCommit) {
    // 首先释放子所有者的资源
    for (auto child : children) {
        child->releaseAllResources(isCommit);
    }
    
    // 释放自己的资源
    // 按照特定顺序释放资源（模拟PostgreSQL的三阶段释放）
    
    // 第一阶段：释放非锁资源
    auto buffers = bufferArray.getItems();
    for (auto& buffer : buffers) {
        std::cout << "资源所有者 '" << name << "' 释放资源: " << buffer->toString() << std::endl;
    }
    bufferArray.clear();
    
    auto relations = relationArray.getItems();
    for (auto& relation : relations) {
        std::cout << "资源所有者 '" << name << "' 释放资源: " << relation->toString() << std::endl;
    }
    relationArray.clear();
    
    auto files = fileArray.getItems();
    for (auto& file : files) {
        std::cout << "资源所有者 '" << name << "' 释放资源: " << file->toString() << std::endl;
    }
    fileArray.clear();
    
    auto snapshots = snapshotArray.getItems();
    for (auto& snapshot : snapshots) {
        std::cout << "资源所有者 '" << name << "' 释放资源: " << snapshot->toString() << std::endl;
    }
    snapshotArray.clear();
    
    // 第二阶段：释放锁资源
    auto locks = lockArray.getItems();
    for (auto& lock : locks) {
        std::cout << "资源所有者 '" << name << "' 释放资源: " << lock->toString() << std::endl;
    }
    lockArray.clear();
    
    // 如果是提交操作，检查是否有资源泄漏
    if (isCommit) {
        if (bufferArray.size() > 0 || relationArray.size() > 0 || 
            fileArray.size() > 0 || snapshotArray.size() > 0 || 
            lockArray.size() > 0) {
            std::cout << "警告：资源所有者 '" << name << "' 在提交时存在资源泄漏！" << std::endl;
        }
    }
}

// 打印资源信息
void ResourceOwner::printResources() const {
    std::cout << "资源所有者 '" << name << "' 的资源情况：" << std::endl;
    std::cout << "  缓冲区: " << bufferArray.size() << " 个" << std::endl;
    std::cout << "  关系: " << relationArray.size() << " 个" << std::endl;
    std::cout << "  文件: " << fileArray.size() << " 个" << std::endl;
    std::cout << "  快照: " << snapshotArray.size() << " 个" << std::endl;
    std::cout << "  锁: " << lockArray.size() << " 个" << std::endl;
    
    // 打印子所有者的资源
    for (auto child : children) {
        child->printResources();
    }
}

// 初始化资源所有者系统
void initResourceOwnerSystem() {
    // 确保系统干净
    cleanupResourceOwnerSystem();
    
    // 创建顶级资源所有者
    TopTransactionResourceOwner = new ResourceOwner("TopTransaction");
    CurrentResourceOwner = TopTransactionResourceOwner;
    
    std::cout << "资源所有者系统已初始化" << std::endl;
}

// 清理资源所有者系统
void cleanupResourceOwnerSystem() {
    if (TopTransactionResourceOwner) {
        delete TopTransactionResourceOwner;
        TopTransactionResourceOwner = nullptr;
    }
    CurrentResourceOwner = nullptr;
    
    std::cout << "资源所有者系统已清理" << std::endl;
}

// 开始事务
void startTransaction() {
    initResourceOwnerSystem();
    std::cout << "事务已开始" << std::endl;
}

// 提交事务
void commitTransaction() {
    if (TopTransactionResourceOwner) {
        std::cout << "提交事务..." << std::endl;
        TopTransactionResourceOwner->releaseAllResources(true);
        cleanupResourceOwnerSystem();
        std::cout << "事务已提交" << std::endl;
    }
}

// 回滚事务
void abortTransaction() {
    if (TopTransactionResourceOwner) {
        std::cout << "回滚事务..." << std::endl;
        TopTransactionResourceOwner->releaseAllResources(false);
        cleanupResourceOwnerSystem();
        std::cout << "事务已回滚" << std::endl;
    }
}

// 创建保存点
ResourceOwner* createSavepoint(const std::string& name) {
    if (!CurrentResourceOwner) {
        std::cerr << "错误：没有活动的资源所有者" << std::endl;
        return nullptr;
    }
    
    std::string savepointName = "Savepoint_" + name;
    ResourceOwner* savepoint = new ResourceOwner(savepointName, CurrentResourceOwner);
    CurrentResourceOwner = savepoint;
    
    std::cout << "创建了保存点 '" << name << "'" << std::endl;
    return savepoint;
}

// 回滚到保存点
void rollbackToSavepoint(ResourceOwner* savepoint) {
    if (!savepoint) {
        std::cerr << "错误：无效的保存点" << std::endl;
        return;
    }
    
    std::cout << "回滚到保存点 '" << savepoint->getName() << "'..." << std::endl;
    
    // 找到保存点的所有子保存点并释放它们的资源
    auto parent = savepoint->getParent();
    if (parent) {
        for (auto child : parent->getChildren()) {
            if (child != savepoint) {
                child->releaseAllResources(false);
            }
        }
    }
    
    // 设置当前资源所有者为保存点
    CurrentResourceOwner = savepoint;
    
    std::cout << "已回滚到保存点 '" << savepoint->getName() << "'" << std::endl;
}

// 释放保存点
void releaseSavepoint(ResourceOwner* savepoint) {
    if (!savepoint) {
        std::cerr << "错误：无效的保存点" << std::endl;
        return;
    }
    
    std::cout << "释放保存点 '" << savepoint->getName() << "'..." << std::endl;
    
    // 设置当前资源所有者为保存点的父所有者
    auto parent = savepoint->getParent();
    if (parent) {
        CurrentResourceOwner = parent;
    } else {
        CurrentResourceOwner = TopTransactionResourceOwner;
    }
    
    // 删除保存点
    delete savepoint;
    
    std::cout << "已释放保存点" << std::endl;
}
