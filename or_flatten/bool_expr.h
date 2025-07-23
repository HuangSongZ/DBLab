#ifndef BOOL_EXPR_H
#define BOOL_EXPR_H

#include <stdbool.h>

// 表达式节点类型
typedef enum NodeTag {
    T_Const,
    T_Var,
    T_BoolExpr
} NodeTag;

// 布尔操作类型
typedef enum BoolExprType {
    AND_EXPR,
    OR_EXPR,
    NOT_EXPR
} BoolExprType;

// 表达式节点
typedef struct Node {
    NodeTag type;
} Node;

// 常量节点
typedef struct Const {
    NodeTag type;       // T_Const
    int constvalue;     // 简单起见，用int表示值
    bool constisnull;   // 是否为NULL
} Const;

// 变量节点
typedef struct Var {
    NodeTag type;       // T_Var
    int varno;          // 变量编号
    char *varname;      // 变量名
} Var;

// 布尔表达式节点
typedef struct BoolExpr {
    NodeTag type;       // T_BoolExpr
    BoolExprType boolop; // AND_EXPR, OR_EXPR, NOT_EXPR
    struct List *args;   // 参数列表
} BoolExpr;

// 列表节点
typedef struct ListCell {
    union {
        void *ptr_value;
        int int_value;
    } data;
    struct ListCell *next;
} ListCell;

typedef struct List {
    int length;
    ListCell *head;
    ListCell *tail;
} List;

// 函数声明
Node *makeConst(int value, bool isnull);
Node *makeVar(int varno, const char *varname);
Node *makeBoolExpr(BoolExprType boolop, List *args);
List *list_make1(void *datum);
List *lappend(List *list, void *datum);
List *list_concat(List *list1, List *list2);
List *list_copy(List *list);
void print_expr(Node *node, int indent);
void print_list(List *list);
List *pull_ors(List *orlist);

#endif // BOOL_EXPR_H
