#include "pg_acl_demo_column.h"

/**
 * 检查对象的权限
 * 
 * 这个函数模拟 PostgreSQL 中的 pg_class_aclmask 函数
 */
AclMode pg_object_aclmask(Oid objid, Oid roleid, AclMode mask) {
    /* 查找对象 */
    for (int i = 0; i < nobjects; i++) {
        if (objects[i].objid == objid) {
            /* 调用 aclmask 计算有效权限 */
            return aclmask(objects[i].acl, roleid, objects[i].owner, mask);
        }
    }
    
    fprintf(stderr, "对象 %d 不存在\n", objid);
    return 0;
}

/**
 * 检查列的权限
 * 
 * 这个函数模拟 PostgreSQL 中的 pg_attribute_aclcheck 函数
 */
AclMode pg_column_aclmask(Oid colid, Oid roleid, AclMode mask) {
    Oid objid = 0;
    Oid owner = 0;
    
    /* 查找列 */
    for (int i = 0; i < ncolumns; i++) {
        if (columns[i].colid == colid) {
            objid = columns[i].objid;
            
            /* 查找列所属对象的所有者 */
            for (int j = 0; j < nobjects; j++) {
                if (objects[j].objid == objid) {
                    owner = objects[j].owner;
                    break;
                }
            }
            
            /* 首先检查列级 ACL */
            AclMode col_result = aclmask(columns[i].acl, roleid, owner, mask);
            
            /* 如果列级 ACL 不为空且未通过权限检查，直接返回结果 */
            if (columns[i].acl->nitems > 0 && col_result != mask) {
                return col_result;
            }
            
            /* 否则，检查表级权限 */
            AclMode tbl_result = pg_object_aclmask(objid, roleid, mask);
            
            /* 如果列级 ACL 为空，直接返回表级结果 */
            if (columns[i].acl->nitems == 0) {
                return tbl_result;
            }
            
            /* 合并列级和表级权限结果 */
            /* 所有权限都必须满足 */
            return (col_result & tbl_result) == mask ? mask : (col_result & tbl_result);
        }
    }
    
    fprintf(stderr, "列 %d 不存在\n", colid);
    return 0;
}

/**
 * 授予对象权限
 */
void grant_object_permission(Oid objid, Oid grantee, Oid grantor, AclMode privs) {
    /* 查找对象 */
    for (int i = 0; i < nobjects; i++) {
        if (objects[i].objid == objid) {
            /* 检查现有 ACL 项 */
            for (int j = 0; j < objects[i].acl->nitems; j++) {
                if (objects[i].acl->items[j].ai_grantee == grantee &&
                    objects[i].acl->items[j].ai_grantor == grantor) {
                    /* 更新现有 ACL 项 */
                    objects[i].acl->items[j].ai_privs |= privs;
                    printf("已更新对象 '%s' 的权限\n", objects[i].objname);
                    return;
                }
            }
            
            /* 添加新的 ACL 项 */
            add_acl_item(objid, grantee, grantor, privs);
            printf("已授予对象 '%s' 的权限\n", objects[i].objname);
            return;
        }
    }
    
    fprintf(stderr, "对象 %d 不存在\n", objid);
}

/**
 * 撤销对象权限
 */
void revoke_object_permission(Oid objid, Oid grantee, Oid grantor, AclMode privs) {
    /* 查找对象 */
    for (int i = 0; i < nobjects; i++) {
        if (objects[i].objid == objid) {
            /* 检查现有 ACL 项 */
            for (int j = 0; j < objects[i].acl->nitems; j++) {
                if (objects[i].acl->items[j].ai_grantee == grantee &&
                    objects[i].acl->items[j].ai_grantor == grantor) {
                    /* 更新现有 ACL 项 */
                    objects[i].acl->items[j].ai_privs &= ~privs;
                    printf("已撤销对象 '%s' 的权限\n", objects[i].objname);
                    return;
                }
            }
            
            printf("未找到要撤销的权限\n");
            return;
        }
    }
    
    fprintf(stderr, "对象 %d 不存在\n", objid);
}

/**
 * 授予列权限
 */
void grant_column_permission(Oid colid, Oid grantee, Oid grantor, AclMode privs) {
    /* 查找列 */
    for (int i = 0; i < ncolumns; i++) {
        if (columns[i].colid == colid) {
            /* 检查现有 ACL 项 */
            for (int j = 0; j < columns[i].acl->nitems; j++) {
                if (columns[i].acl->items[j].ai_grantee == grantee &&
                    columns[i].acl->items[j].ai_grantor == grantor) {
                    /* 更新现有 ACL 项 */
                    columns[i].acl->items[j].ai_privs |= privs;
                    printf("已更新列 '%s' 的权限\n", columns[i].colname);
                    return;
                }
            }
            
            /* 添加新的 ACL 项 */
            add_column_acl_item(colid, grantee, grantor, privs);
            printf("已授予列 '%s' 的权限\n", columns[i].colname);
            return;
        }
    }
    
    fprintf(stderr, "列 %d 不存在\n", colid);
}

/**
 * 撤销列权限
 */
void revoke_column_permission(Oid colid, Oid grantee, Oid grantor, AclMode privs) {
    /* 查找列 */
    for (int i = 0; i < ncolumns; i++) {
        if (columns[i].colid == colid) {
            /* 检查现有 ACL 项 */
            for (int j = 0; j < columns[i].acl->nitems; j++) {
                if (columns[i].acl->items[j].ai_grantee == grantee &&
                    columns[i].acl->items[j].ai_grantor == grantor) {
                    /* 更新现有 ACL 项 */
                    columns[i].acl->items[j].ai_privs &= ~privs;
                    printf("已撤销列 '%s' 的权限\n", columns[i].colname);
                    return;
                }
            }
            
            printf("未找到要撤销的权限\n");
            return;
        }
    }
    
    fprintf(stderr, "列 %d 不存在\n", colid);
}

/**
 * 检查查询权限
 */
bool check_query_permissions(Query *query, Oid roleid) {
    AclMode required_privs = 0;
    
    /* 根据查询类型确定所需权限 */
    switch (query->type) {
        case QUERY_SELECT:
            required_privs = ACL_SELECT;
            break;
        case QUERY_INSERT:
            required_privs = ACL_INSERT;
            break;
        case QUERY_UPDATE:
            required_privs = ACL_UPDATE;
            break;
        case QUERY_DELETE:
            required_privs = ACL_DELETE;
            break;
    }
    
    /* 检查表级权限 */
    AclMode table_result = pg_object_aclmask(query->objid, roleid, required_privs);
    
    /* 如果表级权限满足，不需要检查列级权限 */
    if (table_result == required_privs) {
        return true;
    }
    
    /* 检查列级权限 */
    for (int i = 0; i < query->ncolids; i++) {
        AclMode col_result = pg_column_aclmask(query->colids[i], roleid, required_privs);
        if (col_result != required_privs) {
            return false;  /* 列级权限检查失败 */
        }
    }
    
    return true;
}

/**
 * 执行查询
 */
bool execute_query(Query *query, Oid roleid) {
    const char *role_name = get_role_name(roleid);
    const char *obj_name = get_object_name(query->objid);
    
    /* 打印查询信息 */
    printf("角色 '%s' 执行 ", role_name);
    
    switch (query->type) {
        case QUERY_SELECT:
            printf("SELECT ");
            break;
        case QUERY_INSERT:
            printf("INSERT ");
            break;
        case QUERY_UPDATE:
            printf("UPDATE ");
            break;
        case QUERY_DELETE:
            printf("DELETE ");
            break;
    }
    
    printf("查询，涉及对象 '%s' 和列 ", obj_name);
    
    for (int i = 0; i < query->ncolids; i++) {
        printf("'%s'", get_column_name(query->colids[i]));
        if (i < query->ncolids - 1) {
            printf(", ");
        }
    }
    printf("\n");
    
    /* 检查权限 */
    bool has_permission = check_query_permissions(query, roleid);
    
    if (has_permission) {
        printf("权限检查通过，查询执行成功\n");
        return true;
    } else {
        printf("权限检查失败，查询被拒绝\n");
        return false;
    }
}
