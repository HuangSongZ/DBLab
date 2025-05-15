# PostgreSQL中断机制演示程序

这个演示程序展示了PostgreSQL中断机制的工作原理和应用场景。

## 背景

在PostgreSQL中，中断机制是一个重要的组成部分，允许用户通过发送信号（如Ctrl+C）来取消正在执行的查询。然而，在某些关键操作期间，立即处理这些中断可能会导致数据不一致或协议同步问题。

PostgreSQL实现了一套精细的中断机制，包含三层保护体系：
1. **QueryCancelHoldoffCount** - 只保护查询取消中断
2. **InterruptHoldoffCount** - 保护所有类型的中断
3. **CritSectionCount** - 最严格的保护，不仅阻止中断还升级错误级别

本演示程序主要聚焦于`QueryCancelHoldoffCount`机制，它是PostgreSQL中用于暂时阻止查询取消中断处理的计数器，特别是在处理前端协议消息时。

## 程序功能

这个演示程序模拟了PostgreSQL中断机制的核心部分，包括：

1. 信号处理（模拟`StatementCancelHandler`）- 捕获用户发送的中断信号
2. 中断处理（模拟`ProcessInterrupts`）- 检查并处理挂起的中断请求
3. 前端-后端通信（模拟读取消息过程）- 模拟客户端与服务器之间的协议通信
4. 查询执行（模拟长时间运行的查询）- 模拟可被中断的耗时操作
5. 事务提交（模拟`SimulateTransactionCommit`）- 演示`InterruptHoldoffCount`的作用
6. WAL日志写入（模拟`SimulateWALWrite`）- 演示`CritSectionCount`的作用

程序演示了三种中断保护机制：

1. **QueryCancelHoldoffCount**：
   - 不使用保护时 - 前端读取过程可能被立即中断
   - 使用保护时 - 查询取消中断被推迟到读取完成后

2. **InterruptHoldoffCount**：
   - 不使用保护时 - 事务提交过程可能被中断
   - 使用保护时 - 所有类型的中断都被推迟到事务提交完成

3. **CritSectionCount**：
   - 不使用保护时 - WAL写入过程中的错误保持为ERROR级别
   - 使用保护时 - WAL写入过程中的ERROR错误被升级为FATAL

## 编译和运行

```bash
# 编译程序
make

# 运行程序
./pg_interrupt_demo
```

## 使用方法

1. 运行程序后，会显示一个菜单，提供三种中断机制的演示选项：
   ```
   === 请选择要演示的程序 ===
   1. 演示QueryCancelHoldoffCount
   2. 演示InterruptHoldoffCount
   3. 演示CritSectionCount
   4. 退出
   ```

2. **演示 QueryCancelHoldoffCount**：
   - 选择选项 1 后，程序将模拟执行查询和前端读取过程
   - 在查询执行过程中按 Ctrl+C 可以取消查询
   - 在前端读取过程中按 Ctrl+C，观察有无保护时的行为差异

3. **演示 InterruptHoldoffCount**：
   - 选择选项 2 后，程序将模拟事务提交过程
   - 先演示不使用 InterruptHoldoffCount 保护的情况
   - 再演示使用 InterruptHoldoffCount 保护的情况
   - 在事务提交过程中按 Ctrl+C，观察中断处理的差异

4. **演示 CritSectionCount**：
   - 选择选项 3 后，程序将模拟 WAL 日志写入过程
   - 程序会要求选择是否使用关键部分保护：
     ```
     选择情况: 1. 不使用关键部分, 2. 使用关键部分保护
     ```
   - 在 WAL 写入过程中会模拟错误情况，观察错误级别的差异

5. 在每个演示过程中，可以按 Ctrl+C 发送中断信号，观察程序如何处理中断

## 关键概念

### 中断标志

- `InterruptPending`: 通用中断标志，表示有待处理的中断
- `QueryCancelPending`: 查询取消标志，表示有待处理的查询取消请求

### 中断保护计数器

- `QueryCancelHoldoffCount`: 查询取消暂停计数器，非零时推迟查询取消处理
- `InterruptHoldoffCount`: 通用中断保护计数器，非零时阻止所有中断处理（本演示未实现）
- `CritSectionCount`: 关键部分计数器，非零时阻止中断并升级错误级别（本演示未实现）

### 中断处理宏

- `CHECK_FOR_INTERRUPTS()`: 检查并处理中断的宏，在代码中的关键点调用
- `HOLD_CANCEL_INTERRUPTS()`: 增加查询取消暂停计数器，开始保护区域
- `RESUME_CANCEL_INTERRUPTS()`: 减少查询取消暂停计数器，结束保护区域
- `HOLD_INTERRUPTS()`: 增加通用中断保护计数器（本演示未实现）
- `RESUME_INTERRUPTS()`: 减少通用中断保护计数器（本演示未实现）
- `START_CRIT_SECTION()`: 增加关键部分计数器（本演示未实现）
- `END_CRIT_SECTION()`: 减少关键部分计数器（本演示未实现）

## 预期结果

### 1. QueryCancelHoldoffCount 演示结果

1. 不使用`QueryCancelHoldoffCount`保护时：
   - 如果在前端读取过程中按Ctrl+C，读取过程可能被立即中断
   - 程序会显示“[中断处理] 处理查询取消请求”
   - 这在实际的PostgreSQL中可能导致协议同步问题

2. 使用`QueryCancelHoldoffCount`保护时：
   - 即使在前端读取过程中按Ctrl+C，中断也会被推迟
   - 程序会显示“[中断处理] 正在从前端读取数据，推迟处理查询取消”
   - 直到读取完成并调用`RESUME_CANCEL_INTERRUPTS()`后才会处理中断
   - 这保证了协议的完整性和同步

### 2. InterruptHoldoffCount 演示结果

1. 不使用`InterruptHoldoffCount`保护时：
   - 如果在事务提交过程中按Ctrl+C，事务提交可能被中断
   - 程序会在检查中断点处理中断请求
   - 这可能导致事务状态不一致

2. 使用`InterruptHoldoffCount`保护时：
   - 即使在事务提交过程中按Ctrl+C，所有中断也会被推迟
   - 程序会显示“[中断处理] 无法处理中断: HoldoffCount=1...”
   - 直到事务提交完成并调用`RESUME_INTERRUPTS()`后才会处理中断
   - 这确保了事务提交的原子性

### 3. CritSectionCount 演示结果

1. 不使用`CritSectionCount`保护时：
   - 在WAL写入过程中遇到错误时，错误级别保持为ERROR
   - 程序会显示“[错误处理] ERROR: WAL写入过程中遇到错误”
   - 程序会跳出当前函数，但不会终止

2. 使用`CritSectionCount`保护时：
   - 在WAL写入过程中遇到错误时，错误级别被升级为FATAL
   - 程序会显示“[错误处理] 在关键部分中ERROR被升级为FATAL”
   - 程序会模拟数据库实例重启（调用exit(1)）
   - 这确保了在关键操作中不会出现部分完成的情况

## 中断机制的应用场景

PostgreSQL中的中断保护机制在以下场景中特别重要：

1. **前端-后端通信**：
   - 在读取客户端消息时，必须保证协议的完整性
   - 如果在读取消息过程中处理取消请求，可能导致协议同步问题

2. **事务处理**：
   - 在事务提交、准备和中止时，需要保护关键操作不被中断
   - 确保事务状态的一致性

3. **WAL(预写日志)操作**：
   - WAL日志的写入是数据库持久性的核心
   - 必须保证WAL操作的原子性，不能被中断或部分完成

4. **锁管理**：
   - 在获取和释放锁时，防止中断导致死锁或资源泄漏

## 源代码结构

- `pg_interrupt_demo.c`: 主程序文件，包含中断机制的模拟实现
- `Makefile`: 用于编译程序的makefile
- `README.md`: 本文档，提供程序的概述和使用说明

## 进一步探索

obsidian://open?vault=MyDB&file=PostgreSQL%2F%E4%BD%93%E7%B3%BB%E7%BB%93%E6%9E%84%2F%E4%B8%AD%E6%96%AD%E6%9C%BA%E5%88%B6%E5%8E%9F%E7%90%86
