#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include "shared_inval_queue.h"
#include "backend.h"

// 用于演示的常量
const uint32_t DB_ID_1 = 1;
const uint32_t DB_ID_2 = 2;
const uint32_t REL_ID_1 = 101;
const uint32_t REL_ID_2 = 102;
const uint32_t HASH_VALUE_1 = 201;
const uint32_t HASH_VALUE_2 = 202;

// 打印分隔线
void printSeparator() {
    std::cout << "\n----------------------------------------\n" << std::endl;
}

// 演示1：基本的缓存失效流程
void demo1(std::shared_ptr<SharedInvalQueue> sharedQueue) {
    std::cout << "【演示1】基本的缓存失效流程" << std::endl;
    printSeparator();
    
    // 创建两个后端进程
    Backend backend1(sharedQueue, DB_ID_1);
    Backend backend2(sharedQueue, DB_ID_1);
    
    // 在两个后端中添加相同的缓存项
    backend1.addRelCacheEntry(REL_ID_1, "users表");
    backend1.addSysCacheEntry(HASH_VALUE_1, "用户索引");
    
    backend2.addRelCacheEntry(REL_ID_1, "users表");
    backend2.addSysCacheEntry(HASH_VALUE_1, "用户索引");
    
    printSeparator();
    std::cout << "初始缓存状态:" << std::endl;
    backend1.printCacheStatus();
    backend2.printCacheStatus();
    
    printSeparator();
    std::cout << "后端1修改数据并生成失效消息:" << std::endl;
    
    // 后端1开始事务
    backend1.beginTransaction();
    
    // 注册缓存失效
    backend1.registerRelcacheInvalidation(REL_ID_1);
    
    // 执行命令
    backend1.executeCommand();
    
    // 提交事务，发送失效消息
    backend1.commitTransaction();
    
    printSeparator();
    std::cout << "后端2接收并处理失效消息:" << std::endl;
    
    // 后端2接收失效消息
    backend2.acceptInvalidationMessages();
    
    printSeparator();
    std::cout << "处理失效消息后的缓存状态:" << std::endl;
    backend1.printCacheStatus();
    backend2.printCacheStatus();
}

// 演示2：事务回滚时的缓存失效处理
void demo2(std::shared_ptr<SharedInvalQueue> sharedQueue) {
    std::cout << "【演示2】事务回滚时的缓存失效处理" << std::endl;
    printSeparator();
    
    // 创建两个后端进程
    Backend backend1(sharedQueue, DB_ID_1);
    Backend backend2(sharedQueue, DB_ID_1);
    
    // 在两个后端中添加相同的缓存项
    backend1.addRelCacheEntry(REL_ID_2, "orders表");
    backend1.addSysCacheEntry(HASH_VALUE_2, "订单索引");
    
    backend2.addRelCacheEntry(REL_ID_2, "orders表");
    backend2.addSysCacheEntry(HASH_VALUE_2, "订单索引");
    
    printSeparator();
    std::cout << "初始缓存状态:" << std::endl;
    backend1.printCacheStatus();
    backend2.printCacheStatus();
    
    printSeparator();
    std::cout << "后端1修改数据但回滚事务:" << std::endl;
    
    // 后端1开始事务
    backend1.beginTransaction();
    
    // 注册缓存失效
    backend1.registerRelcacheInvalidation(REL_ID_2);
    backend1.registerSyscacheInvalidation(1, HASH_VALUE_2);
    
    // 执行命令
    backend1.executeCommand();
    
    // 回滚事务，不发送失效消息
    backend1.rollbackTransaction();
    
    printSeparator();
    std::cout << "后端2尝试接收失效消息:" << std::endl;
    
    // 后端2接收失效消息（应该没有消息）
    backend2.acceptInvalidationMessages();
    
    printSeparator();
    std::cout << "事务回滚后的缓存状态:" << std::endl;
    backend1.printCacheStatus();
    backend2.printCacheStatus();
}

// 演示3：事务开始时处理缓存失效消息
void demo3(std::shared_ptr<SharedInvalQueue> sharedQueue) {
    std::cout << "【演示3】事务开始时处理缓存失效消息" << std::endl;
    printSeparator();
    
    // 创建两个后端进程
    Backend backend1(sharedQueue, DB_ID_1);
    Backend backend2(sharedQueue, DB_ID_1);
    
    // 在两个后端中添加相同的缓存项
    backend1.addRelCacheEntry(REL_ID_1, "users表");
    backend2.addRelCacheEntry(REL_ID_1, "users表");
    
    printSeparator();
    std::cout << "初始缓存状态:" << std::endl;
    backend1.printCacheStatus();
    backend2.printCacheStatus();
    
    printSeparator();
    std::cout << "后端1修改数据并生成失效消息:" << std::endl;
    
    // 后端1开始事务
    backend1.beginTransaction();
    
    // 注册缓存失效
    backend1.registerRelcacheInvalidation(REL_ID_1);
    
    // 执行命令
    backend1.executeCommand();
    
    // 提交事务，发送失效消息
    backend1.commitTransaction();
    
    printSeparator();
    std::cout << "后端2开始新事务，自动处理失效消息:" << std::endl;
    
    // 后端2开始事务，会自动接收失效消息
    backend2.beginTransaction();
    
    printSeparator();
    std::cout << "事务开始后的缓存状态:" << std::endl;
    backend1.printCacheStatus();
    backend2.printCacheStatus();
    
    // 后端2提交事务
    backend2.commitTransaction();
}

// 主函数
int main() {
    // 创建共享失效队列
    auto sharedQueue = std::make_shared<SharedInvalQueue>();
    
    // 运行演示1
    demo1(sharedQueue);
    
    printSeparator();
    std::cout << "按Enter键继续..." << std::endl;
    std::cin.get();
    
    // 运行演示2
    demo2(sharedQueue);
    
    printSeparator();
    std::cout << "按Enter键继续..." << std::endl;
    std::cin.get();
    
    // 运行演示3
    demo3(sharedQueue);
    
    return 0;
}
