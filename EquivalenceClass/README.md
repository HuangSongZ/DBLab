# PostgreSQL 等价类（EquivalenceClass）机制研究

## 概述

本项目深入研究了 PostgreSQL 查询优化器中的等价类生成机制，分析了如何通过等价类实现过滤条件下推和连接优化。

## 1.1 查询计划优化

```sql
-- 原始查询
SELECT * FROM a, b WHERE a.a = b.a AND b.a = 5;

-- 优化后的执行计划
Nested Loop
  -> Seq Scan on a
       Filter: (a = 5)        -- 过滤条件下推
  -> Seq Scan on b  
       Filter: (a = 5)        -- 过滤条件下推
-- 注意：原始连接条件 a.a = b.a 被消除
```

### 1.1.1 等价类工作原理

当查询包含等值条件如 `a.a = b.a AND b.a = 5` 时：

1. **等价类创建：** 优化器创建包含 `{a.a, b.a, 5}` 的等价类
2. **隐含等式推导：** 从等价类推导出 `a.a = 5`
3. **过滤条件下推：** 将 `a.a = 5` 和 `b.a = 5` 下推到基表扫描
4. **连接条件消除：** 原始连接条件 `a.a = b.a` 变为冗余并被移除


### 1.1.2 优化策略
- **包含常量的等价类：** 生成 "member = constant" 形式的子句
- **不包含常量的等价类：** 生成 "member1 = member2" 形式的子句

### 1.1.3 实际应用场景
- 多表连接查询优化
- 复杂 WHERE 条件简化
- 索引扫描条件推导
- 排序键优化



## 1.2 核心数据结构

### 1.2.1 EquivalenceClass（等价类）

```c
typedef struct EquivalenceClass {
    NodeTag type;
    List *ec_opfamilies;        // 操作符族列表
    Oid ec_collation;           // 排序规则
    List *ec_members;           // 等价类成员列表
    List *ec_sources;           // 源限制信息列表
    List *ec_derives;           // 派生限制信息列表
    Relids ec_relids;           // 涉及的关系ID集合
    bool ec_has_const;          // 是否包含常量
    bool ec_has_volatile;       // 是否包含易变表达式
    bool ec_below_outer_join;   // 是否在外连接下方
    bool ec_broken;             // 是否生成失败
    Index ec_sortref;           // 排序引用
    Index ec_min_security;      // 最小安全级别
    Index ec_max_security;      // 最大安全级别
    struct EquivalenceClass *ec_merged; // 合并指针
} EquivalenceClass;
```

**关键字段说明：**
- `ec_members`: 存储所有等价的表达式
- `ec_sources`: 记录产生此等价类的原始 WHERE 子句
- `ec_has_const`: 标记是否包含常量，用于过滤条件下推
- `ec_relids`: 记录涉及的所有表，用于连接规划

### 1.2.2 EquivalenceMember（等价类成员）

```c
typedef struct EquivalenceMember {
    NodeTag type;
    Expr *em_expr;              // 表达式（如 a.a, b.a, 5）
    Relids em_relids;           // 表达式涉及的关系ID
    Relids em_nullable_relids;  // 可空关系ID
    bool em_is_const;           // 是否为常量
    bool em_is_child;           // 是否为子关系
    Oid em_datatype;            // 数据类型OID
} EquivalenceMember;
```

**关键字段说明：**
- `em_expr`: 实际的表达式节点
- `em_is_const`: 区分变量和常量，常量用于生成过滤条件
- `em_relids`: 确定表达式属于哪个表

### 1.2.3 RestrictInfo（限制信息）

```c
typedef struct RestrictInfo {
    // ... 其他字段
    EquivalenceClass *left_ec;   // 左侧表达式所属等价类
    EquivalenceClass *right_ec;  // 右侧表达式所属等价类
    EquivalenceMember *left_em;  // 左侧等价类成员
    EquivalenceMember *right_em; // 右侧等价类成员
    EquivalenceClass *parent_ec; // 生成此限制的等价类
} RestrictInfo;
```

## 1.3 信息收集流程

### 1.3.1 阶段 1：初始化和检查

```c
check_mergejoinable(restrictinfo);  // 检查操作符是否支持合并连接

if (restrictinfo->mergeopfamilies) {
    if (maybe_equivalence) {
        if (check_equivalence_delay(root, restrictinfo) &&
            process_equivalence(root, &restrictinfo, below_outer_join))
            return;  // 成功处理为等价类
    }
}
```

**检查条件：**
1. 操作符必须是可合并连接的（如 =, <, > 等）
2. 不能是延迟处理的安全敏感操作
3. 必须满足等价性条件

### 1.3.2 阶段 2：等价类处理核心逻辑

`process_equivalence()` 函数实现四种处理情况：

#### 1.3.2.1 情况 1：两个表达式已在同一等价类中
```c
if (ec1 && ec2 && ec1 == ec2) {
    // 只需添加到源列表，不创建新结构
    ec1->ec_sources = lappend(ec1->ec_sources, restrictinfo);
    restrictinfo->left_ec = ec1;
    restrictinfo->right_ec = ec1;
    return true;
}
```

#### 1.3.2.2 情况 2：两个表达式在不同等价类中
```c
if (ec1 && ec2 && ec1 != ec2) {
    // 合并两个等价类
    ec1->ec_members = list_concat(ec1->ec_members, ec2->ec_members);
    ec1->ec_sources = list_concat(ec1->ec_sources, ec2->ec_sources);
    ec1->ec_relids = bms_join(ec1->ec_relids, ec2->ec_relids);
    ec1->ec_has_const |= ec2->ec_has_const;
    
    // 标记 ec2 已合并
    ec2->ec_merged = ec1;
    root->eq_classes = list_delete_ptr(root->eq_classes, ec2);
}
```

#### 1.3.2.3 情况 3：只找到一个表达式
```c
if (ec1 && !ec2) {
    // 将新表达式添加到现有等价类
    em2 = add_eq_member(ec1, item2, item2_relids, item2_nullable_relids,
                        false, item2_type);
    ec1->ec_sources = lappend(ec1->ec_sources, restrictinfo);
}
```

#### 1.3.2.4 情况 4：都没找到，创建新等价类
```c
if (!ec1 && !ec2) {
    EquivalenceClass *ec = makeNode(EquivalenceClass);
    
    // 初始化等价类属性
    ec->ec_opfamilies = opfamilies;
    ec->ec_collation = collation;
    ec->ec_members = NIL;
    ec->ec_sources = list_make1(restrictinfo);
    ec->ec_has_const = false;
    
    // 添加两个成员
    em1 = add_eq_member(ec, item1, item1_relids, item1_nullable_relids,
                        false, item1_type);
    em2 = add_eq_member(ec, item2, item2_relids, item2_nullable_relids,
                        false, item2_type);
    
    // 添加到全局等价类列表
    root->eq_classes = lappend(root->eq_classes, ec);
}
```

### 1.3.3 阶段 3：成员添加机制

`add_eq_member()` 函数负责创建和添加等价类成员：

```c
static EquivalenceMember *
add_eq_member(EquivalenceClass *ec, Expr *expr, Relids relids,
              Relids nullable_relids, bool is_child, Oid datatype)
{
    EquivalenceMember *em = makeNode(EquivalenceMember);
    
    em->em_expr = expr;
    em->em_relids = relids;
    em->em_nullable_relids = nullable_relids;
    em->em_is_const = false;
    em->em_is_child = is_child;
    em->em_datatype = datatype;
    
    if (bms_is_empty(relids)) {
        // 没有变量，标记为常量
        em->em_is_const = true;
        ec->ec_has_const = true;  // 等价类包含常量
    } else if (!is_child) {
        // 更新等价类的关系ID集合
        ec->ec_relids = bms_add_members(ec->ec_relids, relids);
    }
    
    // 添加到等价类成员列表
    ec->ec_members = lappend(ec->ec_members, em);
    return em;
}
```

## 1.4 示例：`a.a = b.a AND b.a = 5` 的完整处理过程

### 1.4.1 步骤1：处理 `a.a = b.a`

1. **提取信息：**
```c
   item1 = a.a (Var节点: varno=1, varattno=1)
   item2 = b.a (Var节点: varno=2, varattno=1)
   opno = 等号操作符的OID
```

2. **搜索现有等价类：** 没有找到匹配项

3. **创建新等价类（情况 4）：**
```c
   EquivalenceClass *ec = makeNode(EquivalenceClass);
   ec->ec_opfamilies = {integer_ops};
   ec->ec_collation = 0;
   ec->ec_has_const = false;
   ec->ec_relids = {1, 2};  // 包含表1和表2
   
   // 添加两个成员
   em1: {em_expr=a.a, em_relids={1}, em_is_const=false}
   em2: {em_expr=b.a, em_relids={2}, em_is_const=false}
```

4. **更新全局状态：**
```c
   root->eq_classes = [ec];
   restrictinfo->left_ec = ec;
   restrictinfo->right_ec = ec;
```

### 1.4.2 步骤2：处理 `b.a = 5`

1. **提取信息：**
```c
   item1 = b.a (Var节点: varno=2, varattno=1)
   item2 = 5 (Const节点: value=5)
```

2. **搜索现有等价类：** 找到 `b.a` 在已存在的等价类中

3. **添加常量到现有等价类（情况 3）：**
```c
   em3 = add_eq_member(ec, 5, {}, {}, false, INTEGER_OID);
   // em3: {em_expr=5, em_relids={}, em_is_const=true}
```

4. **更新等价类状态：**
```c
   ec->ec_members = [em1(a.a), em2(b.a), em3(5)];
   ec->ec_has_const = true;  // 现在包含常量
   ec->ec_sources = [restrictinfo1, restrictinfo2];
```

### 1.4.3 最终数据结构状态

```c
EquivalenceClass {
    ec_opfamilies: [integer_ops],
    ec_collation: 0,
    ec_members: [
        EquivalenceMember {
            em_expr: Var(a.a),
            em_relids: {1},
            em_is_const: false,
            em_datatype: INTEGER_OID
        },
        EquivalenceMember {
            em_expr: Var(b.a),
            em_relids: {2},
            em_is_const: false,
            em_datatype: INTEGER_OID
        },
        EquivalenceMember {
            em_expr: Const(5),
            em_relids: {},
            em_is_const: true,
            em_datatype: INTEGER_OID
        }
    ],
    ec_sources: [
        RestrictInfo(a.a = b.a),
        RestrictInfo(b.a = 5)
    ],
    ec_relids: {1, 2},
    ec_has_const: true,
    ec_has_volatile: false,
    ec_below_outer_join: false,
    ec_broken: false
}
```

