# B+树键范围约定的重要澄清

## 问题发现

通过实际实验数据，发现了对Lehman & Yao键范围约定的理解错误。

## 实验数据

```sql
-- 根节点 (page 3)
postgres=# select * from bt_page_items('idx_t_a',3);
 itemoffset | ctid  | itemlen | nulls | vars |          data           
------------+-------+---------+-------+------+-------------------------
          1 | (1,0) |       8 | f     | f    |                         
          2 | (2,1) |      16 | f     | f    | 6f 01 00 00 00 00 00 00  -- 0x016f = 367
          3 | (4,1) |      16 | f     | f    | dd 02 00 00 00 00 00 00  -- 0x02dd = 733

-- 子节点 2 (downlink1指向)
postgres=# select * from bt_page_items('idx_t_a',2) limit 3;
 itemoffset |  ctid   | itemlen | nulls | vars |          data           
------------+---------+---------+-------+------+-------------------------
          1 | (3,1)   |      16 | f     | f    | dd 02 00 00 00 00 00 00  -- 733 (high key)
          2 | (1,141) |      16 | f     | f    | 6f 01 00 00 00 00 00 00  -- 367 (第一个数据)
          3 | (1,142) |      16 | f     | f    | 70 01 00 00 00 00 00 00  -- 368

-- 子节点 4 (downlink2指向)
postgres=# select * from bt_page_items('idx_t_a',4) limit 3;
 itemoffset |  ctid  | itemlen | nulls | vars |          data           
------------+--------+---------+-------+------+-------------------------
          1 | (3,55) |      16 | f     | f    | dd 02 00 00 00 00 00 00  -- 733 (第一个数据)
          2 | (3,56) |      16 | f     | f    | de 02 00 00 00 00 00 00  -- 734
          3 | (3,57) |      16 | f     | f    | df 02 00 00 00 00 00 00  -- 735
```

## 关键发现

**内部页面的键值实际上是对应子页面的第一个键（或high key）！**

- 根节点的键 `367` 出现在子节点2的**开始位置**（第一个数据项）
- 根节点的键 `733` 出现在子节点4的**开始位置**（第一个数据项）

## 正确的键范围约定

根据Lehman & Yao论文和PostgreSQL README（第44-45行）：

**"the key range for a subtree S is described by Ki < v <= Ki+1"**

这意味着键范围是**左开右闭**：**(Ki, Ki+1]**

### 树结构解析

```
根节点 (page 3):
    [367, downlink1→page2] [733, downlink2→page4]

键范围分配：
    downlink1 → page 2: (-∞, 367]    包含367
    downlink2 → page 4: (367, 733]   包含733
    (隐含的) → page ?: (733, +∞)
```

### 验证

**子节点2的内容：**
- High key: 733
- 数据范围: 367, 368, 369, ..., 732
- 符合 (-∞, 367] ∪ (367, 733) = (-∞, 733)
- 实际上是 (-∞, 733)，因为high key是733

**子节点4的内容：**
- 数据范围: 733, 734, 735, ...
- 符合 (367, 733] ∪ (733, +∞) = (367, +∞)
- 实际上从733开始

## 二分查找的正确理解

### 内部页面的二分查找

```c
// 在内部页面查找时
// nextkey=false: 返回最后一个 < scankey 的位置
// nextkey=true:  返回最后一个 <= scankey 的位置
```

**为什么这样设计？**

因为键范围是 **(Ki, Ki+1]**，所以：

1. **查找key=367时：**
   - 需要找到包含367的子树
   - 367属于 (-∞, 367]
   - 应该选择 downlink1
   - 二分查找应该返回：最后一个 <= 367 的位置
   - 但实际代码返回：最后一个 < 367 的位置？

**等等，这里有个问题！**

让我重新分析代码...

### 重新理解_bt_binsrch()

查看源码 `nbtsearch.c:347-427`：

```c
// 内部页面：返回最后一个 < scankey (nextkey=false)
//         或最后一个 <= scankey (nextkey=true)
if (P_ISLEAF(opaque))
    return low;

// 内部页面返回 low-1
return OffsetNumberPrev(low);
```

**循环不变式：**
- `nextkey=false (cmpval=1)`: 
  - low之前的所有槽 < scankey
  - high及之后的所有槽 >= scankey
  
结束时 `low == high`，所以：
- `low` 指向第一个 >= scankey 的位置
- `low-1` 指向最后一个 < scankey 的位置

### 实际查找过程

**查找key=367：**

```
根节点: [367, 733]
二分查找: 367 与 [367, 733] 比较
- 367 >= 367 ✓ → low指向offset 0 (键367)
- 返回 low-1 = -1？不对！

实际上第一个键是特殊的：
offset 0: 负无穷（第一个键被视为-∞）
offset 1: 367
offset 2: 733

所以：
- 367 > -∞ → 继续
- 367 == 367 → low = 1
- 返回 low-1 = 0 → downlink1
```

**查找key=733：**

```
根节点: [367, 733]
- 733 > 367 → 继续
- 733 == 733 → low = 2
- 返回 low-1 = 1 → downlink2
```

## 关键洞察

### 1. 第一个键的特殊处理

在`_bt_compare()`函数中（`nbtsearch.c:665`）：

```c
// 内部页面的第一个数据键被视为"负无穷"
if (!P_ISLEAF(opaque) && offnum == P_FIRSTDATAKEY(opaque))
    return 1;  // scankey > 负无穷
```

这意味着：
- offset 0 (P_FIRSTDATAKEY) 总是被视为 -∞
- 实际的键从 offset 1 开始

### 2. 正确的键范围理解

```
内部页面: [-∞, downlink0] [K1, downlink1] [K2, downlink2]

实际键范围：
- downlink0: (-∞, K1]   包含K1
- downlink1: (K1, K2]   包含K2
- downlink2: (K2, +∞)
```

### 3. 二分查找的逻辑

**nextkey=false时：**
- 查找第一个 >= scankey 的位置（low）
- 返回 low-1，即最后一个 < scankey 的位置
- 这个位置的downlink指向包含scankey的子树

**为什么？**
- 如果 scankey == Ki，则 low 指向 Ki
- low-1 指向 Ki-1
- Ki-1 的downlink指向范围 (Ki-1, Ki]
- 这个范围包含 Ki！

## 完整示例

### 树结构

```
根节点:
    offset 0: [-∞, downlink→page1]
    offset 1: [367, downlink→page2]
    offset 2: [733, downlink→page4]

键范围：
    page1: (-∞, 367]    包含 ..., 365, 366, 367
    page2: (367, 733]   包含 368, 369, ..., 732, 733
    page4: (733, +∞)    包含 734, 735, ...
```

### 查找过程

**查找key=367：**
1. 二分查找找到第一个 >= 367 的位置：offset 1 (键367)
2. 返回 offset 0 (键-∞)
3. downlink0 指向 page1，范围 (-∞, 367]
4. ✓ 367 在这个范围内

**查找key=368：**
1. 二分查找找到第一个 >= 368 的位置：offset 2 (键733)
2. 返回 offset 1 (键367)
3. downlink1 指向 page2，范围 (367, 733]
4. ✓ 368 在这个范围内

**查找key=733：**
1. 二分查找找到第一个 >= 733 的位置：offset 2 (键733)
2. 返回 offset 1 (键367)
3. downlink1 指向 page2，范围 (367, 733]
4. ✓ 733 在这个范围内

**等等！733应该在page4！**

让我重新检查实验数据...

### 重新分析实验数据

根节点 page 3:
- offset 1: ctid=(1,0) - 这是指向page 1的downlink
- offset 2: ctid=(2,1), data=0x016f(367) - 指向page 2
- offset 3: ctid=(4,1), data=0x02dd(733) - 指向page 4

所以实际结构是：
```
offset 1: [-∞, downlink→page1]
offset 2: [367, downlink→page2]  
offset 3: [733, downlink→page4]
```

**查找key=733：**
1. 二分查找找到第一个 >= 733 的位置：offset 3 (键733)
2. 返回 offset 2 (键367)
3. downlink2 指向 page2

但是page2的high key是733，数据是367-732！
所以733不在page2中，需要通过right-link移动到page4！

## 最终理解

### Lehman & Yao的精妙之处

**内部页面的键K实际上是对应子页面的high key！**

```
根节点:
    [367, downlink→page2]  -- 367是page2的high key
    [733, downlink→page4]  -- 733是page4的high key（或第一个键）

page2:
    high key: 733
    数据: 367, 368, ..., 732
    范围: [367, 733)  -- 不包含733

page4:
    数据: 733, 734, ...
    范围: [733, +∞)
```

### _bt_moveright()的作用

当查找key=733时：
1. 二分查找返回offset 2，下降到page2
2. 到达page2后，检查high key
3. 733 >= 733（page2的high key）
4. 需要向右移动！
5. 跟随right-link到page4
6. 找到733

**这就是Lehman & Yao算法的核心！**

## 总结

1. **键范围是左开右闭**：(Ki, Ki+1]
2. **内部页面的键是子页面的high key**
3. **二分查找返回"最后一个<"**，可能需要通过right-link调整
4. **_bt_moveright()负责处理边界情况**，确保找到正确页面

这个设计允许：
- 并发页面分裂
- 无需持有父页面锁
- 通过right-link恢复正确位置

非常精妙的设计！
