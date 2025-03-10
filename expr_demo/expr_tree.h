#ifndef EXPR_TREE_H
#define EXPR_TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 表达式节点类型 */
typedef enum {
    NODE_CONST,    // 常量
    NODE_VAR,      // 变量
    NODE_ADD,      // 加法
    NODE_SUB,      // 减法
    NODE_MUL,      // 乘法
    NODE_DIV       // 除法
} NodeType;

/* 表达式树节点 */
typedef struct ExprNode {
    NodeType type;
    union {
        double value;      // 常量值
        char var_name[16]; // 变量名
        struct {
            struct ExprNode *left;
            struct ExprNode *right;
        } op;              // 二元操作
    } data;
} ExprNode;

/* 变量上下文 */
typedef struct {
    char name[16];
    double value;
} Variable;

typedef struct {
    Variable *vars;
    int var_count;
} Context;

/* 创建常量节点 */
ExprNode* create_const_node(double value);

/* 创建变量节点 */
ExprNode* create_var_node(const char *name);

/* 创建操作节点 */
ExprNode* create_op_node(NodeType type, ExprNode *left, ExprNode *right);

/* 释放表达式树 */
void free_expr_tree(ExprNode *node);

/* 打印表达式树 */
void print_expr_tree(ExprNode *node);

/* 创建上下文 */
Context* create_context(int var_count);

/* 设置变量值 */
void set_variable(Context *ctx, const char *name, double value);

/* 获取变量值 */
double get_variable(Context *ctx, const char *name);

/* 释放上下文 */
void free_context(Context *ctx);

#endif /* EXPR_TREE_H */
