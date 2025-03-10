#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "expr_tree.h"
#include "tree_evaluator.h"
#include "flat_evaluator.h"

// 生成随机表达式树
ExprNode* generate_random_expr(int depth, int max_depth, Context *ctx) {
    if (depth >= max_depth || (depth > 0 && rand() % 100 < 30)) {
        // 生成叶子节点 (常量或变量)
        if (rand() % 100 < 70) {
            // 常量
            return create_const_node((double)(rand() % 100) / 10.0);
        } else {
            // 变量
            char var_name[16];
            sprintf(var_name, "x%d", rand() % 5);
            return create_var_node(var_name);
        }
    } else {
        // 生成操作节点
        NodeType op_type = (NodeType)(NODE_ADD + (rand() % 4));
        ExprNode *left = generate_random_expr(depth + 1, max_depth, ctx);
        ExprNode *right = generate_random_expr(depth + 1, max_depth, ctx);
        return create_op_node(op_type, left, right);
    }
}

// 性能测试
void performance_test(int num_tests, int max_depth) {
    printf("开始性能测试 (测试次数: %d, 最大深度: %d)\n", num_tests, max_depth);
    
    // 创建上下文并设置变量
    Context *ctx = create_context(10);
    for (int i = 0; i < 5; i++) {
        char var_name[16];
        sprintf(var_name, "x%d", i);
        set_variable(ctx, var_name, (double)(rand() % 100) / 10.0);
    }
    
    // 总时间统计
    double total_tree_time = 0.0;
    double total_flat_time = 0.0;
    
    for (int test = 0; test < num_tests; test++) {
        // 生成随机表达式
        ExprNode *expr = generate_random_expr(0, max_depth, ctx);
        
        // 编译为扁平表达式
        FlatExpr *flat_expr = create_flat_expr(100);
        compile_tree_to_flat(expr, flat_expr);
        
        // 预热
        evaluate_tree(expr, ctx);
        evaluate_flat(flat_expr, ctx);
        
        // 测试树遍历方法
        clock_t start = clock();
        for (int i = 0; i < 1000000; i++) {
            evaluate_tree(expr, ctx);
        }
        clock_t end = clock();
        double tree_time = (double)(end - start) / CLOCKS_PER_SEC;
        total_tree_time += tree_time;
        
        // 测试扁平数组方法
        start = clock();
        for (int i = 0; i < 1000000; i++) {
            evaluate_flat(flat_expr, ctx);
        }
        end = clock();
        double flat_time = (double)(end - start) / CLOCKS_PER_SEC;
        total_flat_time += flat_time;
        
        // 打印当前测试结果
        printf("测试 #%d:\n", test + 1);
        printf("  表达式: ");
        print_expr_tree(expr);
        printf("\n");
        printf("  树遍历时间: %.6f 秒\n", tree_time);
        printf("  扁平数组时间: %.6f 秒\n", flat_time);
        printf("  性能提升: %.2f%%\n", (tree_time - flat_time) / tree_time * 100.0);
        
        // 释放资源
        free_expr_tree(expr);
        free_flat_expr(flat_expr);
    }
    
    // 打印总结
    printf("\n总结:\n");
    printf("  平均树遍历时间: %.6f 秒\n", total_tree_time / num_tests);
    printf("  平均扁平数组时间: %.6f 秒\n", total_flat_time / num_tests);
    printf("  平均性能提升: %.2f%%\n", 
           (total_tree_time - total_flat_time) / total_tree_time * 100.0);
    
    free_context(ctx);
}

// 示例表达式: (x0 + 2.5) * (x1 - 1.0) / (x2 + x3)
ExprNode* create_example_expr() {
    ExprNode *x0 = create_var_node("x0");
    ExprNode *const1 = create_const_node(2.5);
    ExprNode *add1 = create_op_node(NODE_ADD, x0, const1);
    
    ExprNode *x1 = create_var_node("x1");
    ExprNode *const2 = create_const_node(1.0);
    ExprNode *sub1 = create_op_node(NODE_SUB, x1, const2);
    
    ExprNode *mul1 = create_op_node(NODE_MUL, add1, sub1);
    
    ExprNode *x2 = create_var_node("x2");
    ExprNode *x3 = create_var_node("x3");
    ExprNode *add2 = create_op_node(NODE_ADD, x2, x3);
    
    return create_op_node(NODE_DIV, mul1, add2);
}

int main() {
    // 设置随机数种子
    srand(time(NULL));
    
    // 创建上下文并设置变量
    Context *ctx = create_context(10);
    set_variable(ctx, "x0", 5.0);
    set_variable(ctx, "x1", 3.0);
    set_variable(ctx, "x2", 2.0);
    set_variable(ctx, "x3", 1.0);
    
    printf("表达式计算演示\n");
    printf("----------------\n\n");
    
    // 创建示例表达式
    ExprNode *expr = create_example_expr();
    
    // 打印表达式
    printf("表达式: ");
    print_expr_tree(expr);
    printf("\n\n");
    
    // 使用树遍历方法计算
    double tree_result = evaluate_tree(expr, ctx);
    printf("树遍历结果: %.6f\n", tree_result);
    
    // 编译为扁平表达式
    FlatExpr *flat_expr = create_flat_expr(20);
    compile_tree_to_flat(expr, flat_expr);
    
    // 打印扁平表达式
    print_flat_expr(flat_expr);
    printf("\n");
    
    // 使用扁平数组方法计算
    double flat_result = evaluate_flat(flat_expr, ctx);
    printf("扁平数组结果: %.6f\n\n", flat_result);
    
    // 进行性能测试
    performance_test(5, 5);
    
    // 释放资源
    free_expr_tree(expr);
    free_flat_expr(flat_expr);
    free_context(ctx);
    
    return 0;
}
