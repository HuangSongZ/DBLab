# PostgreSQL B+树搜索实现思路详解

## 概述

PostgreSQL的B+树搜索实现基于**Lehman & Yao高并发B树算法**，这是一种支持高并发读写的B树管理算法。核心实现位于`src/backend/access/nbtree/nbtsearch.c`文件中。

## 核心数据结构

### 1. BTScanInsert - 搜索键结构

```c
typedef struct BTScanInsertData
{
    bool        heapkeyspace;      // 是否使用heap TID作为tiebreaker
    bool        allequalimage;     // 是否支持去重
    bool        anynullkeys;       // 是否包含NULL键
    bool        nextkey;           // false: 查找>=key; true: 查找>key
    bool        pivotsearch;       // 是否用于重新查找叶子页
    ItemPointer scantid;           // heap TID作为最终tiebreaker
    int         keysz;             // scankeys数组大小
    ScanKeyData scankeys[INDEX_MAX_KEYS];  // 扫描键数组
} BTScanInsertData;
```

**关键字段说明：**
- `nextkey`: 控制搜索语义
  - `false`: 查找第一个 >= scankey 的项
  - `true`: 查找第一个 > scankey 的项
- `scantid`: 在heapkeyspace索引中，heap TID用作最终的tiebreaker属性

### 2. BTStack - 父页面栈

```c
typedef struct BTStackData
{
    BlockNumber  bts_blkno;      // 父页面的块号
    OffsetNumber bts_offset;     // 父页面中pivot tuple的偏移
    struct BTStackData *bts_parent;  // 指向更上层父页面的指针
} BTStackData;

typedef BTStackData *BTStack;
```

**作用：**
- 记录从根到叶子的下降路径
- 用于页面分裂时回溯到父页面插入新的pivot tuple
- 不持有父页面的锁，只记录位置信息

## 核心搜索算法

### 1. _bt_search() - 树搜索主函数

**函数签名：**
```c
BTStack _bt_search(Relation rel, BTScanInsert key, Buffer *bufP, 
                   int access, Snapshot snapshot)
```

**参数说明：**
- `rel`: 索引关系
- `key`: 插入类型的扫描键
- `bufP`: 输出参数，返回叶子页面的buffer
- `access`: 访问模式（BT_READ或BT_WRITE）
- `snapshot`: 快照（用于old snapshot检查，插入/删除时为NULL）

**算法流程：**

```
1. 获取根页面
   └─> _bt_getroot(rel, access)
   
2. 循环下降树的每一层
   ├─> 处理并发页面分裂（向右移动）
   │   └─> _bt_moveright(rel, key, *bufP, ...)
   │
   ├─> 检查是否到达叶子页
   │   └─> if (P_ISLEAF(opaque)) break;
   │
   ├─> 在内部页面上二分查找
   │   └─> offnum = _bt_binsrch(rel, key, *bufP)
   │
   ├─> 获取子页面的块号
   │   └─> blkno = BTreeTupleGetDownLink(itup)
   │
   ├─> 保存父页面位置到栈
   │   └─> new_stack->bts_blkno = par_blkno
   │       new_stack->bts_offset = offnum
   │       new_stack->bts_parent = stack_in
   │
   └─> 释放父页面锁，获取子页面
       └─> *bufP = _bt_relandgetbuf(rel, *bufP, blkno, page_access)

3. 返回父页面栈
```

**关键特性：**
- **无父页面锁持有**：下降过程中不持有父页面的锁，只记录位置
- **并发分裂处理**：每层都调用`_bt_moveright()`处理并发分裂
- **锁升级优化**：level 1时如果需要写锁，提前对下一层（叶子页）加写锁

### 2. _bt_moveright() - 向右移动处理并发分裂

**核心思想：**
Lehman & Yao算法的关键：当跟随指针到达页面时，页面可能已经分裂。通过检查页面的high key来判断是否需要向右移动。

**算法逻辑：**

```c
// 判断是否需要向右移动
cmpval = key->nextkey ? 0 : 1;

while (true) {
    // 检查页面的high key
    if (!P_RIGHTMOST(opaque)) {
        // 比较scankey和high key
        if (_bt_compare(rel, key, page, P_HIKEY) >= cmpval) {
            // scankey > high key (或 >= high key when nextkey=true)
            // 需要向右移动
            buf = _bt_relandgetbuf(rel, buf, opaque->btpo_next, access);
            continue;
        }
    }
    
    // 找到正确的页面
    break;
}
```

**关键点：**
- `nextkey=false`: 如果 scankey > high key，向右移动
- `nextkey=true`: 如果 scankey >= high key，向右移动
- 可能需要多次向右移动（页面可能分裂多次）
- 处理死页面的情况

### 3. _bt_binsrch() - 页面内二分查找

**函数签名：**
```c
static OffsetNumber _bt_binsrch(Relation rel, BTScanInsert key, Buffer buf)
```

**返回值语义：**

**叶子页面：**
- 返回第一个 >= scankey 的项的偏移（nextkey=false）
- 返回第一个 > scankey 的项的偏移（nextkey=true）
- 可能返回 maxoff + 1（scankey大于所有键）

**内部页面：**
- 返回最后一个 < scankey 的项的偏移（nextkey=false）
- 返回最后一个 <= scankey 的项的偏移（nextkey=true）
- 这个项指向应该下降的子页面

**算法实现：**

```c
low = P_FIRSTDATAKEY(opaque);
high = PageGetMaxOffsetNumber(page);

// 空页面处理
if (unlikely(high < low))
    return low;

// 二分查找
high++;  // 建立循环不变式
cmpval = key->nextkey ? 0 : 1;

while (high > low) {
    mid = low + ((high - low) / 2);
    result = _bt_compare(rel, key, page, mid);
    
    if (result >= cmpval)
        low = mid + 1;
    else
        high = mid;
}

// 叶子页：返回low（第一个>=或>的位置）
if (P_ISLEAF(opaque))
    return low;

// 内部页：返回low-1（最后一个<或<=的位置）
return OffsetNumberPrev(low);
```

**循环不变式：**
- `nextkey=false (cmpval=1)`:
  - low之前的所有槽 < scankey
  - high及之后的所有槽 >= scankey
  
- `nextkey=true (cmpval=0)`:
  - low之前的所有槽 <= scankey
  - high及之后的所有槽 > scankey

### 4. _bt_compare() - 键比较函数

**关键特性：**

1. **内部页面第一个键的特殊处理：**
   ```c
   // 内部页面的第一个数据键被视为"负无穷"
   // 总是小于scankey
   if (!P_ISLEAF(opaque) && offnum == P_FIRSTDATAKEY(opaque))
       return 1;  // scankey > 负无穷
   ```

2. **支持heap TID作为tiebreaker：**
   - 在heapkeyspace索引中，所有键都是物理唯一的
   - heap TID用作最终的tiebreaker属性

3. **NULL值处理：**
   - NULL被视为可排序的值
   - "相等"不一定意味着应该返回给调用者

## 搜索优化技术

### 1. 快速路径优化（Fastpath）

**场景：** 插入到最右叶子页面

```c
// _bt_search_insert()中的优化
if (RelationGetTargetBlock(rel) != InvalidBlockNumber) {
    // 尝试使用缓存的最右页面
    if (P_RIGHTMOST(lpageop) && 
        P_ISLEAF(lpageop) &&
        PageGetFreeSpace(page) > insertstate->itemsz &&
        _bt_compare(rel, insertstate->itup_key, page, P_HIKEY) > 0) {
        // 可以直接使用，无需下降树
        return NULL;  // NULL栈表示使用fastpath
    }
}
```

**优势：**
- 避免从根开始下降
- 减少锁竞争
- 提高顺序插入性能

### 2. 缓存二分查找边界

**_bt_binsrch_insert()函数：**

```c
typedef struct BTInsertState {
    Buffer      buf;
    OffsetNumber low;          // 缓存的下界
    OffsetNumber stricthigh;   // 缓存的严格上界
    bool        bounds_valid;  // 边界是否有效
    int         postingoff;    // posting list中的偏移
    // ...
} BTInsertStateData;
```

**优化原理：**
- 第一次搜索时计算并缓存边界
- 后续搜索（如唯一性检查）可以重用边界
- 减少重复的二分查找开销

### 3. Posting List二分查找

**场景：** 在posting list tuple中查找特定heap TID

```c
static int _bt_binsrch_posting(BTScanInsert key, Page page, OffsetNumber offnum)
{
    // 在posting list中二分查找scantid
    low = 0;
    high = BTreeTupleGetNPosting(itup);
    
    while (high > low) {
        mid = low + ((high - low) / 2);
        res = ItemPointerCompare(key->scantid, 
                                 BTreeTupleGetPostingN(itup, mid));
        if (res > 0)
            low = mid + 1;
        else if (res < 0)
            high = mid;
        else
            return mid;  // 精确匹配
    }
    
    return low;  // 返回插入位置
}
```

## 并发控制机制

### 1. Lehman & Yao的right-link协议

**核心思想：**
- 每个页面有指向右兄弟的指针
- 每个页面有high key（页面键范围的上界）
- 搜索时不需要持有读锁（除了读取单个页面时）

**并发分裂处理：**
```
假设页面P分裂为P和P'：
1. 创建新页面P'
2. 移动部分数据到P'
3. 设置P的right-link指向P'
4. 设置P的high key
5. 在父页面插入P'的downlink

搜索过程：
1. 跟随downlink到达P
2. 检查scankey与P的high key
3. 如果scankey > high key，跟随right-link到P'
4. 重复直到找到正确页面
```

### 2. 锁协议

**读操作（BT_READ）：**
- 只在读取页面时持有读锁
- 读完立即释放
- 不持有父页面锁

**写操作（BT_WRITE）：**
- 在level 1时提前对叶子页加写锁
- 避免锁升级的竞争
- 完成不完整的分裂

**锁升级处理：**
```c
// 如果最初以读锁到达叶子页，但需要写锁
if (access == BT_WRITE && page_access == BT_READ) {
    // 升级锁
    LockBuffer(*bufP, BUFFER_LOCK_UNLOCK);
    LockBuffer(*bufP, BT_WRITE);
    
    // 重新检查页面是否分裂
    *bufP = _bt_moveright(rel, key, *bufP, true, stack_in, 
                          BT_WRITE, snapshot);
}
```

## 搜索场景示例

### 场景1：简单查询搜索

```sql
SELECT * FROM table WHERE indexed_col = 100;
```

**搜索过程：**
1. 构造BTScanInsert，nextkey=false，scankey=100
2. 从根页面开始
3. 在每个内部页面：
   - 二分查找找到最后一个 < 100 的pivot
   - 跟随其downlink下降
4. 到达叶子页面：
   - 二分查找找到第一个 >= 100 的tuple
   - 返回该位置

### 场景2：范围查询搜索

```sql
SELECT * FROM table WHERE indexed_col > 100;
```

**搜索过程：**
1. 构造BTScanInsert，nextkey=true，scankey=100
2. 搜索逻辑与场景1类似
3. 到达叶子页面：
   - 二分查找找到第一个 > 100 的tuple
   - 开始顺序扫描

### 场景3：唯一索引插入

```sql
INSERT INTO table VALUES (..., 100, ...);
```

**搜索过程：**
1. 构造BTScanInsert，包含heap TID
2. 尝试fastpath优化（最右页面）
3. 如果失败，执行完整树搜索
4. 到达叶子页面后：
   - 使用_bt_binsrch_insert()查找插入位置
   - 检查唯一性约束
   - 如果遇到posting list，进行posting list二分查找

## 关键设计决策

### 1. 为什么内部页面返回"最后一个<"而不是"第一个>="？

**原因：** Lehman & Yao的键范围约定

根据Lehman & Yao论文和PostgreSQL README，键范围约定是：**Ki < v <= Ki+1**（左开右闭）

```
内部页面结构（pivot tuples）：
[K1, downlink1] [K2, downlink2] [K3, downlink3]

键范围约定（左开右闭）：
- downlink1指向的子树包含: (-∞, K1]
- downlink2指向的子树包含: (K1, K2]  
- downlink3指向的子树包含: (K2, +∞)

实际例子（根据你的实验数据）：
根节点: [0x6f01(367), 0x02dd(733)]
- downlink1 → 子页面2，包含 (-∞, 367]，实际从367开始
- downlink2 → 子页面4，包含 (367, 733]，实际从733开始

查找key=367时：
- 应该下降到downlink1（最后一个 <= 367）
- 因为367属于(-∞, 367]范围

查找key=733时：
- 应该下降到downlink2（最后一个 <= 733）
- 因为733属于(367, 733]范围
```

**关键理解：**
- 内部页面的键K实际上是对应子页面的**第一个键（或high key）**
- 该键K**包含在**对应downlink指向的子树中
- 这就是为什么二分查找返回"最后一个 < key"或"最后一个 <= key"

### 2. 为什么需要nextkey参数？

**原因：** 支持不同的搜索语义

- **nextkey=false**: 用于等值查询和范围查询的起点
  - 查找 >= key
  - 包含等于key的项
  
- **nextkey=true**: 用于范围查询的终点和某些特殊场景
  - 查找 > key
  - 排除等于key的项

### 3. 为什么不持有父页面锁？

**原因：** 提高并发性

- Lehman & Yao算法保证：即使父页面信息过时，也能通过right-link找到正确页面
- 减少锁竞争
- 允许更高的并发度
- 代价：可能需要向右移动多次

## 性能特点

### 优势

1. **高并发性**
   - 读操作几乎无锁
   - 写操作锁持有时间短
   - 支持并发分裂

2. **缓存友好**
   - 二分查找局部性好
   - 顺序扫描利用预取

3. **优化技术**
   - Fastpath优化
   - 边界缓存
   - Posting list压缩

### 复杂度

- **时间复杂度**: O(log N) 树高度 + O(log M) 页内二分查找
- **空间复杂度**: O(h) 栈空间，h为树高度
- **并发移动**: 最坏情况O(k)次right-link跟随，k为并发分裂次数

## 总结

PostgreSQL的B+树搜索实现是一个精心设计的高并发算法，核心特点包括：

1. **基于Lehman & Yao算法**：通过right-link和high key支持无锁搜索
2. **二分查找优化**：页面内使用高效的二分查找
3. **并发分裂处理**：通过_bt_moveright()优雅处理并发分裂
4. **多种优化技术**：fastpath、边界缓存、posting list等
5. **灵活的搜索语义**：通过nextkey参数支持不同查询类型

这个实现在保证正确性的同时，实现了极高的并发性能，是数据库索引实现的经典案例。

## 相关源码文件

- `src/backend/access/nbtree/nbtsearch.c` - 搜索核心实现
- `src/backend/access/nbtree/nbtinsert.c` - 插入相关搜索
- `src/backend/access/nbtree/README` - 算法详细说明
- `src/include/access/nbtree.h` - 数据结构定义
