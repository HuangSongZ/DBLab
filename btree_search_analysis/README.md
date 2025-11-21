# PostgreSQL B+树搜索算法演示项目

## 项目简介

本项目提供了PostgreSQL B+树搜索算法的详细分析和C语言演示实现，帮助理解B+树在数据库索引中的核心搜索机制。

## 项目内容

### 1. 文档

- **btree_search_implementation.md** - B+树搜索实现的完整技术分析
  - 核心数据结构详解
  - 搜索算法流程
  - 并发控制机制
  - 优化技术
  - 设计决策分析

### 2. 演示程序

- **btree_search_demo.c** - B+树搜索算法的C语言演示实现
  - 实现了树的下降过程
  - 页面内二分查找
  - 并发分裂处理（right-link跟随）
  - 父页面栈的构建

## 核心算法

### 搜索流程

```
1. 从根页面开始
   ↓
2. 处理并发分裂（_bt_moveright）
   ↓
3. 检查是否到达叶子页
   ├─ 是 → 返回结果
   └─ 否 → 继续
       ↓
4. 页面内二分查找（_bt_binsrch）
   ↓
5. 保存父页面位置到栈
   ↓
6. 下降到子页面
   ↓
   回到步骤2
```

### 关键特性

1. **Lehman & Yao算法**
   - 使用right-link指针处理并发分裂
   - 使用high key判断页面范围
   - 无需持有父页面锁

2. **二分查找优化**
   - 叶子页面：返回第一个 >= key 的位置
   - 内部页面：返回最后一个 < key 的位置
   - 支持nextkey语义（>= vs >）

3. **父页面栈**
   - 记录下降路径
   - 用于页面分裂时的回溯
   - 不持有锁，只记录位置

## 编译和运行

### 编译

```bash
make
```

### 运行

```bash
make run
```

或直接运行：

```bash
./btree_search_demo
```

### 清理

```bash
make clean
```

## 示例输出

程序会创建一个示例B+树并执行多个搜索测试：

```
=== B-tree Structure ===
Root: Block 0
Total pages: 11

Block 0 (INTERNAL):
  Keys: [50, 100]
  Children: [1, 2, 3]

...

=== Starting B-tree search for key=75 (nextkey=false) ===

  Level 0: Visiting page 0
    Binary search on page 0 (type=INTERNAL, num_keys=2):
      [low=0, mid=1, high=3] compare(key=75, page[1]=100) = -25
      [low=0, mid=0, high=1] compare(key=75, page[0]=50) = 25
      Internal page: return offset 0 (child block=2)
    Descending to child: page[0].children[0] = block 2

  Level 1: Visiting page 2
    Binary search on page 2 (type=INTERNAL, num_keys=2):
      ...

  Reached leaf page 8

=== Search complete: found position 1 on leaf page 8 ===
```

## 测试用例

演示程序包含5个测试用例：

1. **查找存在的键** (key=75, nextkey=false)
   - 验证精确匹配的搜索

2. **查找不存在的键** (key=72, nextkey=false)
   - 验证返回第一个大于等于的位置

3. **使用nextkey语义** (key=70, nextkey=true)
   - 验证查找第一个大于的位置

4. **查找最小键** (key=5, nextkey=false)
   - 验证边界情况

5. **查找最大键** (key=115, nextkey=false)
   - 验证边界情况

## 树结构说明

演示程序创建的B+树结构：

```
                    [50, 100]
                   /    |     \
                  /     |      \
           [20,35]   [70,85]   [120]
           /  |  \    /  |  \    |
          /   |   \  /   |   \   |
      [5,10,15] [20,25,30] [35,40,45] [50,55,60,65] [70,75,80] [85,90,95] [100,110,115]
```

- 3层结构（根 + 1层内部页 + 叶子层）
- 11个页面（1个根 + 3个内部页 + 7个叶子页）
- 每个页面都有right-link和high key（除最右页面）

## 与PostgreSQL源码对应关系

| 演示程序 | PostgreSQL源码 | 说明 |
|---------|---------------|------|
| `_bt_search()` | `src/backend/access/nbtree/nbtsearch.c:100` | 主搜索函数 |
| `_bt_binsrch()` | `src/backend/access/nbtree/nbtsearch.c:347` | 二分查找 |
| `_bt_moveright()` | `src/backend/access/nbtree/nbtsearch.c:245` | 右移处理 |
| `_bt_compare()` | `src/backend/access/nbtree/nbtsearch.c:665` | 键比较 |
| `BTStackData` | `src/include/access/nbtree.h:600` | 父页面栈 |
| `BTScanInsert` | `src/include/access/nbtree.h:657` | 扫描键 |

## 学习要点

### 1. 理解Lehman & Yao算法

- **right-link的作用**：处理并发页面分裂
- **high key的作用**：判断键是否在页面范围内
- **无锁搜索**：不需要持有父页面锁

### 2. 理解二分查找的不对称性

- **叶子页面**：返回 >= 或 > 的位置（用于定位数据）
- **内部页面**：返回 < 或 <= 的位置（用于选择子树）

### 3. 理解父页面栈

- **作用**：记录下降路径，用于页面分裂
- **特点**：不持有锁，只记录位置
- **风险**：位置可能过时，但算法保证能找到正确位置

### 4. 理解nextkey语义

- **nextkey=false**：用于等值查询和范围查询起点
- **nextkey=true**：用于范围查询终点

## 扩展阅读

1. **Lehman & Yao论文**
   - "Efficient Locking for Concurrent Operations on B-Trees"
   - ACM TODS, Vol 6, No. 4, December 1981

2. **PostgreSQL文档**
   - `src/backend/access/nbtree/README`
   - B树实现的详细说明

3. **相关源码**
   - `src/backend/access/nbtree/nbtsearch.c` - 搜索实现
   - `src/backend/access/nbtree/nbtinsert.c` - 插入实现
   - `src/include/access/nbtree.h` - 数据结构定义

## 注意事项

1. 本演示程序是简化版本，用于教学目的
2. 实际PostgreSQL实现包含更多优化和错误处理
3. 演示程序不包含并发控制和锁机制
4. 演示程序不包含页面分裂和删除操作

## 许可证

本项目仅用于学习和研究目的。

## 贡献

欢迎提出问题和改进建议！
