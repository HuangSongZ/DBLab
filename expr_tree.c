#include "expr_tree.h"

/* 创建常量节点 */
ExprNode* create_const_node(double value) {
    ExprNode *node = (ExprNode*)malloc(sizeof(ExprNode));
    if (!node) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    node->type = NODE_CONST;
    node->data.value = value;
    return node;
}

/* 创建变量节点 */
ExprNode* create_var_node(const char *name) {
    ExprNode *node = (ExprNode*)malloc(sizeof(ExprNode));
    if (!node) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    node->type = NODE_VAR;
    strncpy(node->data.var_name, name, 15);
    node->data.var_name[15] = '\0';
    return node;
}

/* 创建操作节点 */
ExprNode* create_op_node(NodeType type, ExprNode *left, ExprNode *right) {
    ExprNode *node = (ExprNode*)malloc(sizeof(ExprNode));
    if (!node) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    node->type = type;
    node->data.op.left = left;
    node->data.op.right = right;
    return node;
}

/* 释放表达式树 */
void free_expr_tree(ExprNode *node) {
    if (!node) return;
    
    if (node->type == NODE_ADD || node->type == NODE_SUB || 
        node->type == NODE_MUL || node->type == NODE_DIV) {
        free_expr_tree(node->data.op.left);
        free_expr_tree(node->data.op.right);
    }
    
    free(node);
}

/* 打印表达式树 */
void print_expr_tree(ExprNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_CONST:
            printf("%.2f", node->data.value);
            break;
        case NODE_VAR:
            printf("%s", node->data.var_name);
            break;
        case NODE_ADD:
            printf("(");
            print_expr_tree(node->data.op.left);
            printf(" + ");
            print_expr_tree(node->data.op.right);
            printf(")");
            break;
        case NODE_SUB:
            printf("(");
            print_expr_tree(node->data.op.left);
            printf(" - ");
            print_expr_tree(node->data.op.right);
            printf(")");
            break;
        case NODE_MUL:
            printf("(");
            print_expr_tree(node->data.op.left);
            printf(" * ");
            print_expr_tree(node->data.op.right);
            printf(")");
            break;
        case NODE_DIV:
            printf("(");
            print_expr_tree(node->data.op.left);
            printf(" / ");
            print_expr_tree(node->data.op.right);
            printf(")");
            break;
    }
}

/* 创建上下文 */
Context* create_context(int var_count) {
    Context *ctx = (Context*)malloc(sizeof(Context));
    if (!ctx) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    ctx->vars = (Variable*)malloc(sizeof(Variable) * var_count);
    if (!ctx->vars) {
        fprintf(stderr, "内存分配失败\n");
        free(ctx);
        exit(1);
    }
    
    ctx->var_count = var_count;
    for (int i = 0; i < var_count; i++) {
        ctx->vars[i].name[0] = '\0';
        ctx->vars[i].value = 0.0;
    }
    
    return ctx;
}

/* 设置变量值 */
void set_variable(Context *ctx, const char *name, double value) {
    for (int i = 0; i < ctx->var_count; i++) {
        if (ctx->vars[i].name[0] == '\0') {
            strncpy(ctx->vars[i].name, name, 15);
            ctx->vars[i].name[15] = '\0';
            ctx->vars[i].value = value;
            return;
        } else if (strcmp(ctx->vars[i].name, name) == 0) {
            ctx->vars[i].value = value;
            return;
        }
    }
    fprintf(stderr, "变量空间已满或变量'%s'不存在\n", name);
}

/* 获取变量值 */
double get_variable(Context *ctx, const char *name) {
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            return ctx->vars[i].value;
        }
    }
    fprintf(stderr, "变量'%s'不存在\n", name);
    return 0.0;
}

/* 释放上下文 */
void free_context(Context *ctx) {
    if (ctx) {
        if (ctx->vars) {
            free(ctx->vars);
        }
        free(ctx);
    }
}
