# 勘误说明

## 关于键范围约定的重要更正

### 原始错误理解

在最初的分析中，对Lehman & Yao的键范围约定理解有误：

**错误理解：**
```
- downlink1指向的子树包含: (-∞, K2]
- downlink2指向的子树包含: (K2, K3]
```

这导致对二分查找返回值的解释不够准确。

### 正确理解（基于实验验证）

根据PostgreSQL README和实际实验数据：

**Lehman & Yao的键范围约定：Ki < v <= Ki+1（左开右闭）**

```
内部页面: [K1, downlink1] [K2, downlink2] [K3, downlink3]

正确的键范围：
- downlink1 → (-∞, K1]   包含K1
- downlink2 → (K1, K2]   包含K2  
- downlink3 → (K2, +∞)
```

### 关键发现

1. **内部页面的键K实际上是对应子页面的high key**
2. **该键K包含在对应downlink指向的子树中**
3. **二分查找返回"最后一个<"，配合_bt_moveright()找到正确页面**

### 实验证据

```sql
-- 根节点: [367, 733]
-- 子节点2: 从367开始，high key是733
-- 子节点4: 从733开始

这证明：
- 键367包含在downlink1指向的子树中
- 键733包含在downlink2指向的子树中（通过right-link到达）
```

### 更新的文件

以下文件已更新以反映正确理解：
- `btree_search_implementation.md` - 第386-418行
- `SUMMARY.md` - 添加"重要发现"部分
- `key_range_clarification.md` - 新增详细分析文档

### 感谢

感谢通过实际实验数据指出这个问题，这让我们对Lehman & Yao算法有了更深入和准确的理解！

---

**日期：** 2025-11-17  
**版本：** 1.1
