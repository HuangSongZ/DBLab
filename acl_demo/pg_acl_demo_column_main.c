#include "pg_acl_demo_column.h"

/**
 * 主函数
 */
int main() {
    printf("PostgreSQL 权限控制系统模拟 Demo (扩展版)\n");
    printf("===================================\n\n");
    
    /* 创建角色 */
    add_role(1, "postgres");    /* 超级用户 */
    add_role(2, "alice");       /* 表所有者 */
    add_role(3, "bob");         /* 普通用户 */
    add_role(4, "carol");       /* 普通用户 */
    add_role(5, "analysts");    /* 分析师角色组 */
    
    /* 设置角色成员关系 */
    add_role_member(5, 3);      /* bob 是 analysts 组的成员 */
    
    /* 创建对象 */
    add_object(1, "customer_data", 2);  /* alice 拥有的表 */
    
    /* 添加列 */
    add_column(101, 1, "id");
    add_column(102, 1, "name");
    add_column(103, 1, "email");
    add_column(104, 1, "credit_card");
    add_column(105, 1, "address");
    
    /* 设置表级 ACL */
    /* alice 授予 bob SELECT 权限 */
    add_acl_item(1, 3, 2, ACL_SELECT);
    
    /* alice 授予 analysts 组 SELECT 和 UPDATE 权限 */
    add_acl_item(1, 5, 2, ACL_SELECT | ACL_UPDATE);
    
    /* alice 授予 carol INSERT 权限和 SELECT 的授权选项 */
    add_acl_item(1, 4, 2, ACL_INSERT | ACL_SELECT | ACL_GRANT_OPTION_SELECT);
    
    /* 设置列级 ACL */
    /* alice 限制 bob 对 credit_card 列的访问 */
    add_column_acl_item(104, 3, 2, 0);  /* 撤销所有权限 */
    
    /* alice 授予 carol 对 credit_card 列的 SELECT 权限 */
    add_column_acl_item(104, 4, 2, ACL_SELECT);
    
    /* 打印对象的 ACL */
    print_object_acl(1);
    
    /* 打印列的 ACL */
    print_column_acl(104);  /* credit_card 列 */
    
    printf("\n表级权限检查示例:\n");
    printf("===================================\n\n");
    
    /* 检查 alice 的权限（所有者） */
    check_permission(2, 1, ACL_SELECT | ACL_INSERT | ACL_UPDATE | ACL_DELETE);
    
    /* 检查 bob 的权限（直接授予的权限） */
    check_permission(3, 1, ACL_SELECT);
    
    /* 检查 bob 的权限（通过 analysts 组继承的权限） */
    check_permission(3, 1, ACL_UPDATE);
    
    /* 检查 bob 的权限（没有的权限） */
    check_permission(3, 1, ACL_INSERT);
    
    printf("\n列级权限检查示例:\n");
    printf("===================================\n\n");
    
    /* 检查 bob 对普通列的权限 */
    check_column_permission(3, 103, ACL_SELECT);  /* email 列 */
    
    /* 检查 bob 对受限列的权限 */
    check_column_permission(3, 104, ACL_SELECT);  /* credit_card 列 */
    
    /* 检查 carol 对受限列的权限 */
    check_column_permission(4, 104, ACL_SELECT);  /* credit_card 列 */
    
    printf("\n查询执行示例:\n");
    printf("===================================\n\n");
    
    /* 创建查询 */
    Query query1 = {QUERY_SELECT, 1, (Oid[]){101, 102, 103}, 3};  /* 不包含 credit_card 列的查询 */
    Query query2 = {QUERY_SELECT, 1, (Oid[]){101, 102, 104}, 3};  /* 包含 credit_card 列的查询 */
    Query query3 = {QUERY_UPDATE, 1, (Oid[]){102, 103}, 2};       /* 更新 name 和 email 列的查询 */
    
    /* 执行查询 */
    printf("Bob 执行查询 1 (SELECT id, name, email FROM customer_data):\n");
    execute_query(&query1, 3);  /* bob 执行 */
    
    printf("\nBob 执行查询 2 (SELECT id, name, credit_card FROM customer_data):\n");
    execute_query(&query2, 3);  /* bob 执行 */
    
    printf("\nCarol 执行查询 2 (SELECT id, name, credit_card FROM customer_data):\n");
    execute_query(&query2, 4);  /* carol 执行 */
    
    printf("\nBob 执行查询 3 (UPDATE customer_data SET name=?, email=?):\n");
    execute_query(&query3, 3);  /* bob 执行 */
    
    /* 清理资源 */
    for (int i = 0; i < nroles; i++) {
        free(roles[i].rolename);
        free(roles[i].members);
    }
    free(roles);
    
    for (int i = 0; i < ncolumns; i++) {
        free(columns[i].colname);
        free(columns[i].acl);
    }
    free(columns);
    
    for (int i = 0; i < nobjects; i++) {
        free(objects[i].objname);
        free(objects[i].acl);
        free(objects[i].columns);
    }
    free(objects);
    
    return 0;
}
