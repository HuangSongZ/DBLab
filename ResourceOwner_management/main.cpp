#include "resource_owner.h"
#include <iostream>
#include <memory>

// 模拟数据库操作的函数
void simulateSelectOperation() {
    std::cout << "\n执行 SELECT 操作..." << std::endl;
    
    // 创建查询级别的资源所有者
    ResourceOwner* queryOwner = new ResourceOwner("SelectQuery", CurrentResourceOwner);
    CurrentResourceOwner = queryOwner;
    
    // 获取一些资源
    auto buffer1 = std::make_shared<BufferResource>(101);
    auto relation1 = std::make_shared<RelationResource>("users");
    auto snapshot1 = std::make_shared<SnapshotResource>(201);
    
    // 将资源与资源所有者关联
    CurrentResourceOwner->rememberBuffer(buffer1);
    CurrentResourceOwner->rememberRelation(relation1);
    CurrentResourceOwner->rememberSnapshot(snapshot1);
    
    // 打印当前资源情况
    CurrentResourceOwner->printResources();
    
    // 手动释放一些资源
    CurrentResourceOwner->forgetBuffer(buffer1);
    
    // 查询完成，释放资源所有者
    ResourceOwner* parent = CurrentResourceOwner->getParent();
    delete CurrentResourceOwner;
    CurrentResourceOwner = parent;
    
    std::cout << "SELECT 操作完成" << std::endl;
}

void simulateUpdateOperation() {
    std::cout << "\n执行 UPDATE 操作..." << std::endl;
    
    // 创建查询级别的资源所有者
    ResourceOwner* queryOwner = new ResourceOwner("UpdateQuery", CurrentResourceOwner);
    CurrentResourceOwner = queryOwner;
    
    // 获取一些资源
    auto buffer2 = std::make_shared<BufferResource>(102);
    auto relation2 = std::make_shared<RelationResource>("orders");
    auto lock1 = std::make_shared<LockResource>("orders_pkey");
    auto file1 = std::make_shared<FileResource>(10);
    
    // 将资源与资源所有者关联
    CurrentResourceOwner->rememberBuffer(buffer2);
    CurrentResourceOwner->rememberRelation(relation2);
    CurrentResourceOwner->rememberLock(lock1);
    CurrentResourceOwner->rememberFile(file1);
    
    // 打印当前资源情况
    CurrentResourceOwner->printResources();
    
    // 查询完成，释放资源所有者
    ResourceOwner* parent = CurrentResourceOwner->getParent();
    delete CurrentResourceOwner;
    CurrentResourceOwner = parent;
    
    std::cout << "UPDATE 操作完成" << std::endl;
}

void demonstrateNormalTransaction() {
    std::cout << "\n=== 演示正常事务流程 ===" << std::endl;
    
    // 开始事务
    startTransaction();
    
    // 执行一些操作
    simulateSelectOperation();
    simulateUpdateOperation();
    
    // 提交事务
    commitTransaction();
}

void demonstrateSavepoints() {
    std::cout << "\n=== 演示保存点操作 ===" << std::endl;
    
    // 开始事务
    startTransaction();
    
    // 执行第一个操作
    simulateSelectOperation();
    
    // 创建保存点
    ResourceOwner* savepoint1 = createSavepoint("SP1");
    
    // 在保存点后执行操作
    auto buffer3 = std::make_shared<BufferResource>(103);
    CurrentResourceOwner->rememberBuffer(buffer3);
    
    // 创建第二个保存点
    ResourceOwner* savepoint2 = createSavepoint("SP2");
    
    // 在第二个保存点后执行操作
    auto relation3 = std::make_shared<RelationResource>("products");
    CurrentResourceOwner->rememberRelation(relation3);
    
    // 打印资源情况
    TopTransactionResourceOwner->printResources();
    
    // 回滚到第一个保存点
    rollbackToSavepoint(savepoint1);
    
    // 打印回滚后的资源情况
    TopTransactionResourceOwner->printResources();
    
    // 执行另一个操作
    simulateUpdateOperation();
    
    // 提交事务
    commitTransaction();
}

void demonstrateResourceLeak() {
    std::cout << "\n=== 演示资源泄漏检测 ===" << std::endl;
    
    // 开始事务
    startTransaction();
    
    // 获取资源但不释放（模拟泄漏）
    auto leakedBuffer = std::make_shared<BufferResource>(999);
    TopTransactionResourceOwner->rememberBuffer(leakedBuffer);
    
    // 提交事务时会检测到泄漏
    commitTransaction();
}

void demonstrateAbortTransaction() {
    std::cout << "\n=== 演示事务回滚 ===" << std::endl;
    
    // 开始事务
    startTransaction();
    
    // 执行一些操作
    simulateSelectOperation();
    simulateUpdateOperation();
    
    // 回滚事务
    abortTransaction();
}

int main() {
    std::cout << "PostgreSQL 资源所有者管理演示程序" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // 演示正常事务流程
    demonstrateNormalTransaction();
    
    // 演示保存点操作
    demonstrateSavepoints();
    
    // 演示资源泄漏检测
    demonstrateResourceLeak();
    
    // 演示事务回滚
    demonstrateAbortTransaction();
    
    std::cout << "\n演示程序结束" << std::endl;
    return 0;
}
