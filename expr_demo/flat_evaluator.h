#ifndef FLAT_EVALUATOR_H
#define FLAT_EVALUATOR_H

#include "expr_tree.h"

/* 操作码 */
typedef enum {
    OP_LOAD_CONST,    // 加载常量
    OP_LOAD_VAR,      // 加载变量
    OP_ADD,           // 加法
    OP_SUB,           // 减法
    OP_MUL,           // 乘法
    OP_DIV            // 除法
} OpCode;

/* 指令 */
typedef struct {
    OpCode op;
    union {
        double value;      // 常量值
        char var_name[16]; // 变量名
    } data;
} Instruction;

/* 扁平化表达式 */
typedef struct {
    Instruction *instructions;
    int count;
    int capacity;
} FlatExpr;

/* 创建扁平化表达式 */
FlatExpr* create_flat_expr(int initial_capacity);

/* 将表达式树编译为扁平化表达式 */
void compile_tree_to_flat(ExprNode *node, FlatExpr *flat_expr);

/* 基于扁平数组的表达式计算 */
double evaluate_flat(FlatExpr *flat_expr, Context *ctx);

/* 释放扁平化表达式 */
void free_flat_expr(FlatExpr *flat_expr);

/* 打印扁平化表达式 */
void print_flat_expr(FlatExpr *flat_expr);

#endif /* FLAT_EVALUATOR_H */
