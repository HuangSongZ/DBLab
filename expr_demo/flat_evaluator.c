#include "flat_evaluator.h"

/* 创建扁平化表达式 */
FlatExpr* create_flat_expr(int initial_capacity) {
    FlatExpr *flat_expr = (FlatExpr*)malloc(sizeof(FlatExpr));
    if (!flat_expr) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    flat_expr->instructions = (Instruction*)malloc(sizeof(Instruction) * initial_capacity);
    if (!flat_expr->instructions) {
        fprintf(stderr, "内存分配失败\n");
        free(flat_expr);
        exit(1);
    }
    
    flat_expr->count = 0;
    flat_expr->capacity = initial_capacity;
    
    return flat_expr;
}

/* 添加指令 */
static void add_instruction(FlatExpr *flat_expr, Instruction instr) {
    if (flat_expr->count >= flat_expr->capacity) {
        int new_capacity = flat_expr->capacity * 2;
        flat_expr->instructions = (Instruction*)realloc(
            flat_expr->instructions, sizeof(Instruction) * new_capacity);
        if (!flat_expr->instructions) {
            fprintf(stderr, "内存重分配失败\n");
            exit(1);
        }
        flat_expr->capacity = new_capacity;
    }
    
    flat_expr->instructions[flat_expr->count++] = instr;
}

/* 将表达式树编译为扁平化表达式 */
void compile_tree_to_flat(ExprNode *node, FlatExpr *flat_expr) {
    if (!node) return;
    
    Instruction instr;
    
    // 后序遍历，先处理左右子树，再处理当前节点
    if (node->type == NODE_ADD || node->type == NODE_SUB || 
        node->type == NODE_MUL || node->type == NODE_DIV) {
        compile_tree_to_flat(node->data.op.left, flat_expr);
        compile_tree_to_flat(node->data.op.right, flat_expr);
        
        // 添加操作指令
        switch (node->type) {
            case NODE_ADD:
                instr.op = OP_ADD;
                break;
            case NODE_SUB:
                instr.op = OP_SUB;
                break;
            case NODE_MUL:
                instr.op = OP_MUL;
                break;
            case NODE_DIV:
                instr.op = OP_DIV;
                break;
            default:
                break;
        }
        add_instruction(flat_expr, instr);
    } else if (node->type == NODE_CONST) {
        // 添加常量加载指令
        instr.op = OP_LOAD_CONST;
        instr.data.value = node->data.value;
        add_instruction(flat_expr, instr);
    } else if (node->type == NODE_VAR) {
        // 添加变量加载指令
        instr.op = OP_LOAD_VAR;
        strncpy(instr.data.var_name, node->data.var_name, 15);
        instr.data.var_name[15] = '\0';
        add_instruction(flat_expr, instr);
    }
}

/* 基于扁平数组的表达式计算 */
double evaluate_flat(FlatExpr *flat_expr, Context *ctx) {
    if (!flat_expr || flat_expr->count == 0) {
        return 0.0;
    }
    
    // 使用栈来存储中间结果
    double stack[1000];
    int stack_top = -1;
    
    for (int i = 0; i < flat_expr->count; i++) {
        Instruction *instr = &flat_expr->instructions[i];
        
        switch (instr->op) {
            case OP_LOAD_CONST:
                stack[++stack_top] = instr->data.value;
                break;
                
            case OP_LOAD_VAR:
                stack[++stack_top] = get_variable(ctx, instr->data.var_name);
                break;
                
            case OP_ADD: {
                double b = stack[stack_top--];
                double a = stack[stack_top--];
                stack[++stack_top] = a + b;
                break;
            }
                
            case OP_SUB: {
                double b = stack[stack_top--];
                double a = stack[stack_top--];
                stack[++stack_top] = a - b;
                break;
            }
                
            case OP_MUL: {
                double b = stack[stack_top--];
                double a = stack[stack_top--];
                stack[++stack_top] = a * b;
                break;
            }
                
            case OP_DIV: {
                double b = stack[stack_top--];
                if (b == 0.0) {
                    fprintf(stderr, "除零错误\n");
                    return 0.0;
                }
                double a = stack[stack_top--];
                stack[++stack_top] = a / b;
                break;
            }
        }
    }
    
    // 栈顶元素应该是最终结果
    if (stack_top != 0) {
        fprintf(stderr, "表达式计算错误，栈不平衡\n");
        return 0.0;
    }
    
    return stack[stack_top];
}

/* 释放扁平化表达式 */
void free_flat_expr(FlatExpr *flat_expr) {
    if (flat_expr) {
        if (flat_expr->instructions) {
            free(flat_expr->instructions);
        }
        free(flat_expr);
    }
}

/* 打印扁平化表达式 */
void print_flat_expr(FlatExpr *flat_expr) {
    if (!flat_expr) return;
    
    printf("扁平化表达式 (指令数: %d):\n", flat_expr->count);
    for (int i = 0; i < flat_expr->count; i++) {
        Instruction *instr = &flat_expr->instructions[i];
        printf("  %d: ", i);
        
        switch (instr->op) {
            case OP_LOAD_CONST:
                printf("LOAD_CONST %.2f", instr->data.value);
                break;
                
            case OP_LOAD_VAR:
                printf("LOAD_VAR %s", instr->data.var_name);
                break;
                
            case OP_ADD:
                printf("ADD");
                break;
                
            case OP_SUB:
                printf("SUB");
                break;
                
            case OP_MUL:
                printf("MUL");
                break;
                
            case OP_DIV:
                printf("DIV");
                break;
        }
        printf("\n");
    }
}
