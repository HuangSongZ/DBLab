# PostgreSQL B+树搜索实现思路总结

## 核心算法

### 1. _bt_search() - 树下降
从根到叶子的搜索，每层执行：
- 处理并发分裂（_bt_moveright）
- 二分查找（_bt_binsrch）
- 保存父页面位置到栈
- 下降到子页面

### 2. _bt_binsrch() - 二分查找
- **叶子页**：返回第一个 >= key 的位置
- **内部页**：返回最后一个 < key 的位置（用于选择子树）

### 3. _bt_moveright() - 处理并发分裂
通过检查high key判断是否需要向右移动，实现Lehman & Yao算法

## 关键特性

1. **高并发**：基于Lehman & Yao算法，使用right-link和high key
2. **无父锁**：下降时不持有父页面锁
3. **二分优化**：页面内O(log M)查找
4. **父页面栈**：记录路径用于分裂

## 重要发现（基于实验验证）

### Lehman & Yao键范围约定

**键范围是左开右闭：Ki < v <= Ki+1**

```
内部页面: [K1, downlink1] [K2, downlink2]

键范围：
- downlink1 → (-∞, K1]   包含K1
- downlink2 → (K1, K2]   包含K2
```

### 关键洞察

1. **内部页面的键K是对应子页面的high key**
2. **二分查找返回"最后一个<"，可能需要right-link调整**
3. **_bt_moveright()处理边界情况**，确保找到正确页面

详见 `key_range_clarification.md` 的详细分析。

## 项目文件

- `btree_search_implementation.md` - 详细技术分析（已更新）
- `key_range_clarification.md` - 键范围约定的深入分析
- `btree_search_demo.c` - C语言演示程序
- `search_visualization.txt` - 搜索过程可视化
- `README.md` - 使用说明

## 验证结果

程序成功演示了5个测试场景，包括：
- 存在的键查找
- 不存在的键查找
- nextkey语义
- 边界情况

所有测试通过，正确展示了B+树搜索的核心机制。
