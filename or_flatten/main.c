#include <stdio.h>
#include <stdlib.h>
#include "bool_expr.h"

// 创建测试用例1：简单的OR嵌套
Node *create_test_case1() {
    // 创建表达式: A OR (B OR C)
    Node *a = makeVar(1, "A");
    Node *b = makeVar(2, "B");
    Node *c = makeVar(3, "C");
    
    // 创建内部OR: (B OR C)
    List *inner_args = list_make1(b);
    inner_args = lappend(inner_args, c);
    Node *inner_or = makeBoolExpr(OR_EXPR, inner_args);
    
    // 创建外部OR: A OR (B OR C)
    List *outer_args = list_make1(a);
    outer_args = lappend(outer_args, inner_or);
    
    return makeBoolExpr(OR_EXPR, outer_args);
}

// 创建测试用例2：多层嵌套的OR
Node *create_test_case2() {
    // 创建表达式: A OR (B OR (C OR D))
    Node *a = makeVar(1, "A");
    Node *b = makeVar(2, "B");
    Node *c = makeVar(3, "C");
    Node *d = makeVar(4, "D");
    
    // 创建最内层OR: (C OR D)
    List *innermost_args = list_make1(c);
    innermost_args = lappend(innermost_args, d);
    Node *innermost_or = makeBoolExpr(OR_EXPR, innermost_args);
    
    // 创建中间层OR: (B OR (C OR D))
    List *middle_args = list_make1(b);
    middle_args = lappend(middle_args, innermost_or);
    Node *middle_or = makeBoolExpr(OR_EXPR, middle_args);
    
    // 创建最外层OR: A OR (B OR (C OR D))
    List *outer_args = list_make1(a);
    outer_args = lappend(outer_args, middle_or);
    
    return makeBoolExpr(OR_EXPR, outer_args);
}

// 创建测试用例3：混合AND和OR
Node *create_test_case3() {
    // 创建表达式: (A AND B) OR (C AND D) OR E
    Node *a = makeVar(1, "A");
    Node *b = makeVar(2, "B");
    Node *c = makeVar(3, "C");
    Node *d = makeVar(4, "D");
    Node *e = makeVar(5, "E");
    
    // 创建第一个AND: (A AND B)
    List *and1_args = list_make1(a);
    and1_args = lappend(and1_args, b);
    Node *and1 = makeBoolExpr(AND_EXPR, and1_args);
    
    // 创建第二个AND: (C AND D)
    List *and2_args = list_make1(c);
    and2_args = lappend(and2_args, d);
    Node *and2 = makeBoolExpr(AND_EXPR, and2_args);
    
    // 创建最外层OR: (A AND B) OR (C AND D) OR E
    List *or_args = list_make1(and1);
    or_args = lappend(or_args, and2);
    or_args = lappend(or_args, e);
    
    return makeBoolExpr(OR_EXPR, or_args);
}

// 释放表达式树
void free_expr(Node *node) {
    if (node == NULL) {
        return;
    }
    
    switch (node->type) {
        case T_Const:
            free(node);
            break;
            
        case T_Var: {
            Var *v = (Var *)node;
            free(v->varname);
            free(v);
            break;
        }
            
        case T_BoolExpr: {
            BoolExpr *b = (BoolExpr *)node;
            if (b->args != NULL) {
                ListCell *cell;
                for (cell = b->args->head; cell != NULL; ) {
                    ListCell *next = cell->next;
                    free_expr((Node *)cell->data.ptr_value);
                    free(cell);
                    cell = next;
                }
                free(b->args);
            }
            free(b);
            break;
        }
    }
}

// 测试OR表达式扁平化
void test_flatten_or_expression() {
    printf("=== Testing OR Expression Flattening ===\n\n");
    
    // 测试用例1: A OR (B OR C)
    printf("Test Case 1: A OR (B OR C)\n");
    printf("Original expression: ");
    Node *expr1 = create_test_case1();
    print_expr(expr1, 0);
    printf("\n");
    
    printf("Flattened expression: ");
    // 拉平OR表达式
    List *flattened1 = pull_ors(list_copy(((BoolExpr *)expr1)->args));
    printf("(OR ");
    print_list(flattened1);
    printf(")\n\n");
    
    // 释放原始表达式
    free_expr(expr1);
    // 释放拉平后的列表和节点
    free_expr((Node *)makeBoolExpr(OR_EXPR, flattened1));
    
    // 测试用例2: A OR (B OR (C OR D))
    printf("Test Case 2: A OR (B OR (C OR D))\n");
    printf("Original expression: ");
    Node *expr2 = create_test_case2();
    print_expr(expr2, 0);
    printf("\n");
    
    printf("Flattened expression: ");
    // 拉平OR表达式
    List *flattened2 = pull_ors(list_copy(((BoolExpr *)expr2)->args));
    printf("(OR ");
    print_list(flattened2);
    printf(")\n\n");
    
    // 释放原始表达式
    free_expr(expr2);
    // 释放拉平后的列表和节点
    free_expr((Node *)makeBoolExpr(OR_EXPR, flattened2));
    
    // 测试用例3: (A AND B) OR (C AND D) OR E
    printf("Test Case 3: (A AND B) OR (C AND D) OR E\n");
    printf("Original expression: ");
    Node *expr3 = create_test_case3();
    print_expr(expr3, 0);
    printf("\n");
    
    printf("Flattened expression: ");
    // 拉平OR表达式
    List *flattened3 = pull_ors(list_copy(((BoolExpr *)expr3)->args));
    printf("(OR ");
    print_list(flattened3);
    printf(")\n");
    
    // 释放原始表达式
    free_expr(expr3);
    // 释放拉平后的列表和节点
    free_expr((Node *)makeBoolExpr(OR_EXPR, flattened3));
}

int main() {
    test_flatten_or_expression();
    return 0;
}
