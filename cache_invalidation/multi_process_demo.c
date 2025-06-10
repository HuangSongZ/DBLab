#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include "shared_memory.h"
#include "backend_process.h"

// 用于演示的常量
#define DB_ID_1 1
#define DB_ID_2 2
#define REL_ID_1 101
#define REL_ID_2 102
#define HASH_VALUE_1 201
#define HASH_VALUE_2 202

// 打印分隔线
void print_separator() {
    printf("\n----------------------------------------\n\n");
}

// 等待用户按Enter键继续
void wait_for_enter() {
    printf("\n按Enter键继续...\n");
    getchar();
}

// 后端进程函数
void backend_process_main(int backendId, uint32_t dbId, int demo_type) {
    // 初始化后端
    init_backend(dbId);
    
    // 添加缓存项
    if (demo_type == 1 || demo_type == 3) {
        add_cache_item(&g_relCache, REL_ID_1, "users表");
        add_cache_item(&g_sysCache, HASH_VALUE_1, "用户索引");
    } else if (demo_type == 2) {
        add_cache_item(&g_relCache, REL_ID_2, "orders表");
        add_cache_item(&g_sysCache, HASH_VALUE_2, "订单索引");
    }
    
    // 打印初始缓存状态
    print_cache_status();
    
    // 根据后端ID和演示类型执行不同操作
    if (backendId == 0) {  // 第一个后端
        if (demo_type == 1) {
            // 演示1：基本的缓存失效流程
            sleep(2);  // 等待其他后端初始化
            
            // 开始事务
            begin_transaction();
            
            // 注册缓存失效
            register_relcache_invalidation(REL_ID_1);
            
            // 命令结束
            command_end_invalidation_messages();
            
            // 提交事务
            commit_transaction();
            
            // 等待一段时间，让其他后端处理失效消息
            sleep(2);
            
            // 打印最终缓存状态
            print_cache_status();
            
        } else if (demo_type == 2) {
            // 演示2：事务回滚
            sleep(2);  // 等待其他后端初始化
            
            // 开始事务
            begin_transaction();
            
            // 注册缓存失效
            register_relcache_invalidation(REL_ID_2);
            register_syscache_invalidation(1, HASH_VALUE_2);
            
            // 命令结束
            command_end_invalidation_messages();
            
            // 回滚事务
            rollback_transaction();
            
            // 等待一段时间
            sleep(2);
            
            // 打印最终缓存状态
            print_cache_status();
            
        } else if (demo_type == 3) {
            // 演示3：缓存队列溢出和重置
            sleep(2);  // 等待其他后端初始化
            
            // 开始事务
            begin_transaction();
            
            // 生成大量失效消息
            printf("【后端】生成大量失效消息...\n");
            for (int i = 0; i < 50; i++) {
                register_relcache_invalidation(REL_ID_1 + i);
                command_end_invalidation_messages();
            }
            
            // 提交事务
            commit_transaction();
            
            // 等待一段时间
            sleep(2);
            
            // 打印最终缓存状态
            print_cache_status();
        }
    } else {  // 其他后端
        // 循环检查是否有新消息
        for (int i = 0; i < 10; i++) {
            sleep(1);
            
            if (g_hasNewMessages) {
                if (demo_type == 3) {
                    // 演示3：在事务开始时处理失效消息
                    begin_transaction();
                } else {
                    // 演示1和2：直接处理失效消息
                    accept_invalidation_messages();
                }
            }
            
            // 每次循环后打印缓存状态
            if (i == 9) {
                print_cache_status();
            }
        }
    }
    
    // 退出
    exit(0);
}

// 主函数
int main(int argc, char *argv[]) {
    int demo_type = 1;
    
    // 检查命令行参数
    if (argc > 1) {
        demo_type = atoi(argv[1]);
        if (demo_type < 1 || demo_type > 3) {
            demo_type = 1;
        }
    }
    
    // 清理可能存在的旧共享内存和信号量
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
    
    // 初始化共享内存和信号量
    init_shared_memory();
    sem_t *sem = create_semaphore();
    if (sem == SEM_FAILED) {
        cleanup_shared_memory();
        return 1;
    }
    
    // 打印演示标题
    printf("\n【PostgreSQL缓存失效机制多进程演示】\n");
    print_separator();
    
    if (demo_type == 1) {
        printf("【演示1】基本的缓存失效流程\n");
    } else if (demo_type == 2) {
        printf("【演示2】事务回滚时的缓存失效处理\n");
    } else {
        printf("【演示3】缓存队列溢出和后端重置\n");
    }
    
    print_separator();
    
    // 创建后端进程
    pid_t backend_pids[2];
    
    for (int i = 0; i < 2; i++) {
        backend_pids[i] = fork();
        
        if (backend_pids[i] < 0) {
            perror("fork");
            cleanup_semaphore();
            cleanup_shared_memory();
            return 1;
        }
        
        if (backend_pids[i] == 0) {
            // 子进程（后端进程）
            backend_process_main(i, DB_ID_1, demo_type);
            // 子进程不应该返回到这里
            exit(0);
        }
        
        // 父进程继续创建下一个子进程
    }
    
    // 等待所有子进程结束
    for (int i = 0; i < 2; i++) {
        waitpid(backend_pids[i], NULL, 0);
    }
    
    // 清理资源
    sem_close(sem);
    cleanup_semaphore();
    cleanup_shared_memory();
    
    printf("\n演示结束\n");
    
    return 0;
}
