# PostgreSQL缓存失效机制演示

这个演示程序模拟了PostgreSQL的缓存失效机制，帮助理解其工作原理。

## 核心概念

1. **共享内存队列**：存储缓存失效消息
2. **后端进程**：模拟PostgreSQL的后端进程，每个进程维护自己的缓存
3. **事务处理**：模拟事务开始、执行和提交/回滚过程中的缓存失效处理
4. **消息传播**：演示消息如何从一个后端传播到其他后端

## 程序组件

1. `SharedInvalQueue`：模拟共享内存中的失效消息队列
2. `Backend`：模拟PostgreSQL后端进程
3. `Transaction`：模拟事务处理
4. `Cache`：模拟关系缓存和系统缓存
5. `InvalidationMessage`：模拟缓存失效消息

## 使用方法

```bash
# 编译程序
make

# 运行演示
./pg_cache_inval_demo
```

## 演示内容

1. 多个后端进程同时运行
2. 一个后端修改数据并提交事务
3. 其他后端接收并处理缓存失效消息
4. 观察缓存状态的变化

## 设计原理
obsidian://open?vault=MyDB&file=PostgreSQL%2F%E4%BD%93%E7%B3%BB%E7%BB%93%E6%9E%84%2F%E7%BC%93%E5%AD%98%E5%A4%B1%E6%95%88%E6%9C%BA%E5%88%B6%E5%AE%9E%E7%8E%B0
