#ifndef TREE_EVALUATOR_H
#define TREE_EVALUATOR_H

#include "expr_tree.h"

/* 基于树遍历的表达式计算 */
double evaluate_tree(ExprNode *node, Context *ctx);

#endif /* TREE_EVALUATOR_H */
