#include "pg_acl_demo_column.h"

/**
 * 获取角色名称
 */
const char *get_role_name(Oid roleid) {
    for (int i = 0; i < nroles; i++) {
        if (roles[i].roleid == roleid)
            return roles[i].rolename;
    }
    if (roleid == ACL_ID_PUBLIC)
        return "PUBLIC";
    return "未知角色";
}

/**
 * 获取对象名称
 */
const char *get_object_name(Oid objid) {
    for (int i = 0; i < nobjects; i++) {
        if (objects[i].objid == objid)
            return objects[i].objname;
    }
    return "未知对象";
}

/**
 * 获取列名称
 */
const char *get_column_name(Oid colid) {
    for (int i = 0; i < ncolumns; i++) {
        if (columns[i].colid == colid)
            return columns[i].colname;
    }
    return "未知列";
}

/**
 * 打印权限字符串
 */
void print_privs(AclMode privs) {
    printf("权限: ");
    if (privs & ACL_SELECT)
        printf("SELECT ");
    if (privs & ACL_INSERT)
        printf("INSERT ");
    if (privs & ACL_UPDATE)
        printf("UPDATE ");
    if (privs & ACL_DELETE)
        printf("DELETE ");
    
    printf("\n授权选项: ");
    if (privs & ACL_GRANT_OPTION_SELECT)
        printf("SELECT ");
    if (privs & ACL_GRANT_OPTION_INSERT)
        printf("INSERT ");
    if (privs & ACL_GRANT_OPTION_UPDATE)
        printf("UPDATE ");
    if (privs & ACL_GRANT_OPTION_DELETE)
        printf("DELETE ");
    printf("\n");
}

/**
 * 打印对象的 ACL
 */
void print_object_acl(Oid objid) {
    for (int i = 0; i < nobjects; i++) {
        if (objects[i].objid == objid) {
            printf("对象 '%s' (ID: %d) 的 ACL:\n", 
                   objects[i].objname, objects[i].objid);
            printf("所有者: %s (ID: %d)\n", 
                   get_role_name(objects[i].owner), objects[i].owner);
            
            for (int j = 0; j < objects[i].acl->nitems; j++) {
                AclItem *item = &objects[i].acl->items[j];
                printf("  被授权者: %s (ID: %d), 授权者: %s (ID: %d)\n",
                       get_role_name(item->ai_grantee), item->ai_grantee,
                       get_role_name(item->ai_grantor), item->ai_grantor);
                print_privs(item->ai_privs);
                printf("\n");
            }
            return;
        }
    }
    fprintf(stderr, "对象 %d 不存在\n", objid);
}

/**
 * 打印列的 ACL
 */
void print_column_acl(Oid colid) {
    for (int i = 0; i < ncolumns; i++) {
        if (columns[i].colid == colid) {
            printf("列 '%s' (ID: %d) 的 ACL:\n", 
                   columns[i].colname, columns[i].colid);
            
            /* 获取列所属对象和所有者 */
            Oid objid = columns[i].objid;
            Oid owner = 0;
            for (int j = 0; j < nobjects; j++) {
                if (objects[j].objid == objid) {
                    owner = objects[j].owner;
                    printf("所属对象: %s (ID: %d)\n", 
                           objects[j].objname, objid);
                    printf("所有者: %s (ID: %d)\n", 
                           get_role_name(owner), owner);
                    break;
                }
            }
            
            for (int j = 0; j < columns[i].acl->nitems; j++) {
                AclItem *item = &columns[i].acl->items[j];
                printf("  被授权者: %s (ID: %d), 授权者: %s (ID: %d)\n",
                       get_role_name(item->ai_grantee), item->ai_grantee,
                       get_role_name(item->ai_grantor), item->ai_grantor);
                print_privs(item->ai_privs);
                printf("\n");
            }
            return;
        }
    }
    fprintf(stderr, "列 %d 不存在\n", colid);
}

/**
 * 检查并打印对象权限检查结果
 */
void check_permission(Oid roleid, Oid objid, AclMode mask) {
    const char *rolename = get_role_name(roleid);
    const char *objname = get_object_name(objid);
    
    printf("\n检查角色 '%s' (ID: %d) 对对象 '%s' (ID: %d) 的权限:\n",
           rolename, roleid, objname, objid);
    
    printf("请求的权限: ");
    print_privs(mask);
    printf("\n");
    
    AclMode result = pg_object_aclmask(objid, roleid, mask);
    
    printf("有效权限: ");
    print_privs(result);
    printf("\n");
    
    if (result == mask) {
        printf("结果: 拥有所有请求的权限\n");
    } else {
        printf("结果: 缺少一些请求的权限\n");
    }
}

/**
 * 检查并打印列权限检查结果
 */
void check_column_permission(Oid roleid, Oid colid, AclMode mask) {
    const char *rolename = get_role_name(roleid);
    const char *colname = get_column_name(colid);
    
    printf("\n检查角色 '%s' (ID: %d) 对列 '%s' (ID: %d) 的权限:\n",
           rolename, roleid, colname, colid);
    
    printf("请求的权限: ");
    print_privs(mask);
    printf("\n");
    
    AclMode result = pg_column_aclmask(colid, roleid, mask);
    
    printf("有效权限: ");
    print_privs(result);
    printf("\n");
    
    if (result == mask) {
        printf("结果: 拥有所有请求的权限\n");
    } else {
        printf("结果: 缺少一些请求的权限\n");
    }
}
