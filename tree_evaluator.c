#include "tree_evaluator.h"

/* 基于树遍历的表达式计算 */
double evaluate_tree(ExprNode *node, Context *ctx) {
    if (!node) {
        fprintf(stderr, "空节点\n");
        return 0.0;
    }
    
    switch (node->type) {
        case NODE_CONST:
            return node->data.value;
            
        case NODE_VAR:
            return get_variable(ctx, node->data.var_name);
            
        case NODE_ADD:
            return evaluate_tree(node->data.op.left, ctx) + 
                   evaluate_tree(node->data.op.right, ctx);
            
        case NODE_SUB:
            return evaluate_tree(node->data.op.left, ctx) - 
                   evaluate_tree(node->data.op.right, ctx);
            
        case NODE_MUL:
            return evaluate_tree(node->data.op.left, ctx) * 
                   evaluate_tree(node->data.op.right, ctx);
            
        case NODE_DIV: {
            double divisor = evaluate_tree(node->data.op.right, ctx);
            if (divisor == 0.0) {
                fprintf(stderr, "除零错误\n");
                return 0.0;
            }
            return evaluate_tree(node->data.op.left, ctx) / divisor;
        }
            
        default:
            fprintf(stderr, "未知节点类型\n");
            return 0.0;
    }
}
