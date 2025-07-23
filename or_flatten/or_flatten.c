#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bool_expr.h"

// 创建常量节点
Node *makeConst(int value, bool isnull) {
    Const *c = (Const *)malloc(sizeof(Const));
    c->type = T_Const;
    c->constvalue = value;
    c->constisnull = isnull;
    return (Node *)c;
}

// 创建变量节点
Node *makeVar(int varno, const char *varname) {
    Var *v = (Var *)malloc(sizeof(Var));
    v->type = T_Var;
    v->varno = varno;
    v->varname = strdup(varname);
    return (Node *)v;
}

// 创建布尔表达式节点
Node *makeBoolExpr(BoolExprType boolop, List *args) {
    BoolExpr *b = (BoolExpr *)malloc(sizeof(BoolExpr));
    b->type = T_BoolExpr;
    b->boolop = boolop;
    b->args = args;
    return (Node *)b;
}

// 创建包含单个元素的列表
List *list_make1(void *datum) {
    List *list = (List *)malloc(sizeof(List));
    ListCell *cell = (ListCell *)malloc(sizeof(ListCell));
    
    cell->data.ptr_value = datum;
    cell->next = NULL;
    
    list->length = 1;
    list->head = cell;
    list->tail = cell;
    
    return list;
}

// 在列表末尾添加元素
List *lappend(List *list, void *datum) {
    if (list == NULL) {
        return list_make1(datum);
    }
    
    ListCell *cell = (ListCell *)malloc(sizeof(ListCell));
    cell->data.ptr_value = datum;
    cell->next = NULL;
    
    if (list->tail == NULL) {
        list->head = cell;
        list->tail = cell;
    } else {
        list->tail->next = cell;
        list->tail = cell;
    }
    
    list->length++;
    return list;
}

// 连接两个列表
List *list_concat(List *list1, List *list2) {
    if (list1 == NULL) {
        return list2;
    }
    if (list2 == NULL) {
        return list1;
    }
    
    if (list1->tail == NULL) {
        list1->head = list2->head;
        list1->tail = list2->tail;
    } else {
        list1->tail->next = list2->head;
        list1->tail = list2->tail;
    }
    
    list1->length += list2->length;
    free(list2);
    return list1;
}

// 判断节点是否为OR表达式
bool is_orclause(Node *node) {
    return (node != NULL && 
            node->type == T_BoolExpr && 
            ((BoolExpr *)node)->boolop == OR_EXPR);
}

// 判断节点是否为AND表达式
bool is_andclause(Node *node) {
    return (node != NULL && 
            node->type == T_BoolExpr && 
            ((BoolExpr *)node)->boolop == AND_EXPR);
}

// 递归扁平化OR表达式
List *pull_ors(List *orlist) {
    List *out_list = NULL;
    ListCell *cell;
    
    if (orlist == NULL) {
        return NULL;
    }
    
    // 创建新列表来存储结果
    out_list = (List *)malloc(sizeof(List));
    out_list->length = 0;
    out_list->head = NULL;
    out_list->tail = NULL;
    
    for (cell = orlist->head; cell != NULL; cell = cell->next) {
        Node *subexpr = (Node *)cell->data.ptr_value;
        
        if (is_orclause(subexpr)) {
            // 递归处理嵌套的OR表达式
            List *sublist = pull_ors(list_copy(((BoolExpr *)subexpr)->args));
            out_list = list_concat(out_list, sublist);
            // 释放当前OR节点，因为我们已经处理了它的子节点
            free(subexpr);
        } else {
            // 非OR表达式直接添加到结果列表
            out_list = lappend(out_list, subexpr);
        }
    }
    
    // 释放原始列表结构（不释放节点，因为它们已经被移动或复制到out_list）
    free(orlist);
    return out_list;
}

// 打印缩进
static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

// 递归打印表达式
void print_expr(Node *node, int indent) {
    if (node == NULL) {
        printf("NULL");
        return;
    }
    
    switch (node->type) {
        case T_Const: {
            Const *c = (Const *)node;
            if (c->constisnull) {
                printf("NULL");
            } else {
                printf("%d", c->constvalue);
            }
            break;
        }
        case T_Var: {
            Var *v = (Var *)node;
            printf("%s", v->varname);
            break;
        }
        case T_BoolExpr: {
            BoolExpr *b = (BoolExpr *)node;
            const char *opname = "?";
            
            switch (b->boolop) {
                case AND_EXPR: opname = "AND"; break;
                case OR_EXPR:  opname = "OR";  break;
                case NOT_EXPR: opname = "NOT"; break;
            }
            
            printf("(%s\n", opname);
            
            ListCell *cell;
            bool first = true;
            for (cell = b->args->head; cell != NULL; cell = cell->next) {
                if (!first) {
                    printf(",\n");
                }
                print_indent(indent + 1);
                print_expr((Node *)cell->data.ptr_value, indent + 1);
                first = false;
            }
            
            printf(")");
            break;
        }
        default:
            printf("UNKNOWN_NODE");
    }
}

// 复制列表结构（浅拷贝）
List *list_copy(List *list) {
    if (list == NULL) {
        return NULL;
    }
    
    List *new_list = (List *)malloc(sizeof(List));
    if (new_list == NULL) {
        return NULL;
    }
    
    new_list->length = list->length;
    new_list->head = NULL;
    new_list->tail = NULL;
    
    ListCell *cell;
    for (cell = list->head; cell != NULL; cell = cell->next) {
        ListCell *new_cell = (ListCell *)malloc(sizeof(ListCell));
        if (new_cell == NULL) {
            // 内存分配失败，清理已分配的内存
            while (new_list->head != NULL) {
                ListCell *temp = new_list->head;
                new_list->head = temp->next;
                free(temp);
            }
            free(new_list);
            return NULL;
        }
        
        new_cell->data = cell->data;  // 浅拷贝
        new_cell->next = NULL;
        
        if (new_list->tail == NULL) {
            new_list->head = new_cell;
            new_list->tail = new_cell;
        } else {
            new_list->tail->next = new_cell;
            new_list->tail = new_cell;
        }
    }
    
    return new_list;
}

// 打印列表
void print_list(List *list) {
    if (list == NULL) {
        printf("NULL");
        return;
    }
    
    printf("List (length=%d): [", list->length);
    
    if (list->head != NULL) {
        ListCell *cell;
        bool first = true;
        for (cell = list->head; cell != NULL; cell = cell->next) {
            if (!first) {
                printf(", ");
            }
            if (cell->data.ptr_value != NULL) {
                print_expr((Node *)cell->data.ptr_value, 0);
            } else {
                printf("NULL");
            }
            first = false;
        }
    }
    
    printf("]");
}
