#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

/*
 * 这是一个演示程序，展示PostgreSQL中QueryCancelHoldoffCount的作用
 * 模拟了PostgreSQL中断处理机制的核心部分
 */

/* 全局变量，模拟PostgreSQL中的中断标志 */
volatile sig_atomic_t InterruptPending = false;
volatile sig_atomic_t QueryCancelPending = false;
volatile sig_atomic_t ProcDiePending = false;

/* 中断控制计数器 */
volatile unsigned int InterruptHoldoffCount = 0;
volatile unsigned int QueryCancelHoldoffCount = 0;
volatile unsigned int CritSectionCount = 0;

/* 模拟长时间运行的查询 */
volatile bool QueryRunning = false;
volatile int QueryProgress = 0;
volatile int TotalSteps = 10;

/* 模拟前端-后端通信状态 */
volatile bool ReadingFromFrontend = false;

/* 模拟事务状态 */
volatile bool InTransaction = false;

/* 模拟错误处理 */
sigjmp_buf JumpBuffer;
volatile bool ErrorInProgress = false;

/* 宏定义，类似于PostgreSQL中的宏 */
#define INTERRUPTS_PENDING_CONDITION() (InterruptPending)

#define INTERRUPTS_CAN_BE_PROCESSED() \
    (InterruptHoldoffCount == 0 && CritSectionCount == 0 && \
     QueryCancelHoldoffCount == 0)

#define CHECK_FOR_INTERRUPTS() \
do { \
    if (INTERRUPTS_PENDING_CONDITION()) \
        ProcessInterrupts(); \
} while(0)

#define HOLD_INTERRUPTS() (InterruptHoldoffCount++)
#define RESUME_INTERRUPTS() \
do { \
    if (InterruptHoldoffCount > 0) \
        InterruptHoldoffCount--; \
} while(0)

#define HOLD_CANCEL_INTERRUPTS() (QueryCancelHoldoffCount++)
#define RESUME_CANCEL_INTERRUPTS() \
do { \
    if (QueryCancelHoldoffCount > 0) \
        QueryCancelHoldoffCount--; \
} while(0)

#define START_CRIT_SECTION() (CritSectionCount++)
#define END_CRIT_SECTION() \
do { \
    if (CritSectionCount > 0) \
        CritSectionCount--; \
} while(0)

/* 信号处理函数，模拟PostgreSQL的StatementCancelHandler */
void StatementCancelHandler(int sig) {
    printf("\n[信号处理] 收到SIGINT信号\n");
    InterruptPending = true;
    QueryCancelPending = true;
}

/* 错误处理函数，模拟PostgreSQL的ereport */
void ReportError(const char *message, bool is_fatal) {
    if (ErrorInProgress)
        return;  /* 防止递归错误 */
    
    ErrorInProgress = true;
    
    /* 在关键部分中，将ERROR升级为FATAL */
    if (CritSectionCount > 0 && !is_fatal) {
        printf("\n[错误处理] 在关键部分中ERROR被升级为FATAL: %s\n", message);
        is_fatal = true;
    } else {
        printf("\n[错误处理] %s: %s\n", is_fatal ? "FATAL" : "ERROR", message);
    }
    
    /* 重置中断计数器 */
    InterruptHoldoffCount = 0;
    QueryCancelHoldoffCount = 0;
    
    if (is_fatal) {
        printf("[错误处理] 模拟数据库实例重启\n");
        exit(1);  /* 模拟FATAL错误导致进程终止 */
    } else {
        /* 模拟ERROR错误的非局部跳转 */
        ErrorInProgress = false;
        siglongjmp(JumpBuffer, 1);
    }
}

/* 中断处理函数，模拟PostgreSQL的ProcessInterrupts */
void ProcessInterrupts(void) {
    /* 检查是否可以处理中断 */
    if (!INTERRUPTS_CAN_BE_PROCESSED()) {
        printf("[中断处理] 无法处理中断: HoldoffCount=%u, CritSectionCount=%u, QueryCancelHoldoffCount=%u\n",
               InterruptHoldoffCount, CritSectionCount, QueryCancelHoldoffCount);
        return;
    }

    InterruptPending = false;

    if (ProcDiePending) {
        ProcDiePending = false;
        QueryCancelPending = false;  /* ProcDie优先级高于QueryCancel */
        printf("[中断处理] 处理进程终止请求\n");
        ReportError("进程终止", true);  /* FATAL错误 */
    }
    
    if (QueryCancelPending) {
        if (ReadingFromFrontend) {
            /* 
             * 如果正在从前端读取数据，重新设置InterruptPending
             * 这样可以在读取完成后再处理中断
             */
            printf("[中断处理] 正在从前端读取数据，推迟处理查询取消\n");
            InterruptPending = true;
        } else {
            QueryCancelPending = false;
            printf("[中断处理] 处理查询取消请求\n");
            QueryRunning = false;  /* 停止查询 */
        }
    }
}

/* 模拟从前端读取消息 */
void ReadFromFrontend(void) {
    printf("\n[前端通信] 开始从前端读取消息...\n");
    
    /* 模拟情况1: 不使用QueryCancelHoldoffCount */
    printf("[前端通信] 情况1: 不保护前端读取过程\n");
    ReadingFromFrontend = true;
    
    /* 模拟读取过程 */
    for (int i = 1; i <= 5; i++) {
        printf("[前端通信] 读取消息部分 %d/5...\n", i);
        sleep(1);
        
        /* 检查中断 */
        CHECK_FOR_INTERRUPTS();
    }
    
    ReadingFromFrontend = false;
    printf("[前端通信] 消息读取完成\n");
    
    /* 模拟情况2: 使用QueryCancelHoldoffCount保护 */
    printf("\n[前端通信] 情况2: 使用QueryCancelHoldoffCount保护前端读取过程\n");
    ReadingFromFrontend = true;
    
    /* 保护前端读取过程 */
    HOLD_CANCEL_INTERRUPTS();
    printf("[前端通信] QueryCancelHoldoffCount增加到%u\n", QueryCancelHoldoffCount);
    
    /* 模拟读取过程 */
    for (int i = 1; i <= 5; i++) {
        printf("[前端通信] 读取消息部分 %d/5...\n", i);
        sleep(1);
        
        /* 检查中断 */
        CHECK_FOR_INTERRUPTS();
    }
    
    /* 恢复中断处理 */
    RESUME_CANCEL_INTERRUPTS();
    printf("[前端通信] QueryCancelHoldoffCount减少到%u\n", QueryCancelHoldoffCount);
    ReadingFromFrontend = false;
    printf("[前端通信] 消息读取完成\n");
    
    /* 现在可以安全处理之前推迟的中断 */
    CHECK_FOR_INTERRUPTS();
}

/* 模拟执行长时间查询 */
void ExecuteQuery(void) {
    printf("\n[查询执行] 开始执行长时间查询...\n");
    QueryRunning = true;
    QueryProgress = 0;
    
    while (QueryRunning && QueryProgress < TotalSteps) {
        QueryProgress++;
        printf("[查询执行] 查询进度: %d/%d\n", QueryProgress, TotalSteps);
        sleep(1);
        
        /* 定期检查中断 */
        CHECK_FOR_INTERRUPTS();
    }
    
    if (!QueryRunning) {
        printf("[查询执行] 查询被取消\n");
    } else {
        printf("[查询执行] 查询成功完成\n");
    }
}

/* 模拟事务提交 */
void SimulateTransactionCommit(void) {
    printf("\n[事务] 开始提交事务...\n");
    
    /* 设置错误处理跳转点 */
    if (sigsetjmp(JumpBuffer, 1) != 0) {
        printf("[事务] 事务提交失败，已回滚\n");
        return;
    }
    
    /* 模拟情况1: 不使用InterruptHoldoffCount */
    printf("[事务] 情况1: 不保护事务提交过程\n");
    InTransaction = true;
    
    for (int i = 1; i <= 3; i++) {
        printf("[事务] 提交步骤 %d/3...\n", i);
        sleep(1);
        
        /* 检查中断 */
        CHECK_FOR_INTERRUPTS();
    }
    
    InTransaction = false;
    printf("[事务] 事务提交完成\n");
    
    /* 模拟情况2: 使用InterruptHoldoffCount保护 */
    printf("\n[事务] 情况2: 使用InterruptHoldoffCount保护事务提交过程\n");
    InTransaction = true;
    
    /* 保护事务提交过程 */
    HOLD_INTERRUPTS();
    printf("[事务] InterruptHoldoffCount增加到%u\n", InterruptHoldoffCount);
    
    for (int i = 1; i <= 3; i++) {
        printf("[事务] 提交步骤 %d/3...\n", i);
        sleep(1);
        
        /* 检查中断 */
        CHECK_FOR_INTERRUPTS();
    }
    
    /* 恢复中断处理 */
    RESUME_INTERRUPTS();
    printf("[事务] InterruptHoldoffCount减少到%u\n", InterruptHoldoffCount);
    InTransaction = false;
    printf("[事务] 事务提交完成\n");
    
    /* 现在可以安全处理之前推迟的中断 */
    CHECK_FOR_INTERRUPTS();
}

/* 模拟WAL日志写入 */
void SimulateWALWrite(void) {
    printf("\n[WAL] 开始WAL日志写入...\n");
    
    /* 设置错误处理跳转点 */
    if (sigsetjmp(JumpBuffer, 1) != 0) {
        printf("[WAL] WAL日志写入失败\n");
        return;
    }
    
    /* 选择情况 */
    int choice;
    printf("选择情况: 1. 不使用关键部分, 2. 使用关键部分保护\n");
    scanf("%d", &choice);
    
    switch (choice) {
        case 1:
            /* 模拟情况1: 不使用关键部分 */
            printf("[WAL] 情况1: 不使用关键部分保护WAL写入\n");
            
            for (int i = 1; i <= 3; i++) {
                printf("[WAL] 写入WAL记录 %d/3...\n", i);
                sleep(1);
                
                /* 模拟错误情况 */
                /* 在第2步发生ERROR级别错误 */
                if (i == 2) {
                    printf("[WAL] 模拟遇到错误情况\n");
                    ReportError("WAL写入过程中遇到错误", false);  /* ERROR级别错误，会退出sigsetjmp */
                }
                
                /* 检查中断 */
                CHECK_FOR_INTERRUPTS();
            }
            
            printf("[WAL] WAL日志写入完成\n");
            break;
        case 2:
            /* 模拟情况2: 使用关键部分保护 */
            printf("\n[WAL] 情况2: 使用关键部分保护WAL写入\n");
            
            /* 进入关键部分 */
            START_CRIT_SECTION();
            printf("[WAL] CritSectionCount增加到%u\n", CritSectionCount);
            
            for (int i = 1; i <= 3; i++) {
                printf("[WAL] 写入WAL记录 %d/3...\n", i);
                sleep(1);
                
                /* 模拟错误情况 */
                /* 在第2步发生ERROR级别错误 */
                if (i == 2) {
                    printf("[WAL] 模拟遇到错误情况\n");
                    ReportError("WAL写入过程中遇到错误", false);  /* 在关键部分中会被升级为FATAL */
                }
                
                /* 检查中断 */
                CHECK_FOR_INTERRUPTS();
            }
            
            break;
        default:
            printf("选择错误\n");
    }
    
    /* 退出关键部分 */
    END_CRIT_SECTION();
    printf("[WAL] CritSectionCount减少到%u\n", CritSectionCount);
    printf("[WAL] WAL日志写入完成\n");
}

/* 主函数 */
int main(void) {
    /* 注册信号处理函数 */
    signal(SIGINT, StatementCancelHandler);
    
    printf("=== PostgreSQL中断机制演示程序 ===\n");
    printf("这个程序演示了PostgreSQL中断机制中的三种保护机制:\n");
    printf("1. QueryCancelHoldoffCount: 保护前端-后端通信过程\n");
    printf("2. InterruptHoldoffCount: 保护事务提交等关键操作\n");
    printf("3. CritSectionCount: 保护WAL写入等绝对关键操作\n\n");
    
    printf("按Ctrl+C可以模拟发送查询取消请求\n\n");
    
    int choice;
    while (true) {
        printf("=== 请选择要演示的程序 ===\n");
        printf("1. 演示QueryCancelHoldoffCount\n");
        printf("2. 演示InterruptHoldoffCount\n");
        printf("3. 演示CritSectionCount\n");
        printf("4. 退出\n");
        printf("请键入选择:");
        scanf("%d", &choice);
        if (choice == 1) {
            printf("\n=== 演示1: QueryCancelHoldoffCount ===\n");
            ExecuteQuery();
            ReadFromFrontend();
            ExecuteQuery();
        } else if (choice == 2) {
            printf("\n=== 演示2: InterruptHoldoffCount ===\n");
            SimulateTransactionCommit();
        } else if (choice == 3) {
            printf("\n=== 演示3: CritSectionCount ===\n");
            SimulateWALWrite();
        } else if (choice == 4) {
            break;
        }
    }
    return 0;
}
