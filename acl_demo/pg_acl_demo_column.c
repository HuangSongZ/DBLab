#include "pg_acl_demo_column.h"

/* 全局变量定义 */
Role *roles = NULL;
int nroles = 0;

Object *objects = NULL;
int nobjects = 0;

Column *columns = NULL;
int ncolumns = 0;

/**
 * 创建一个新的 ACL
 */
Acl *create_acl(int nitems) {
    Acl *acl = (Acl *)malloc(sizeof(Acl) + (nitems - 1) * sizeof(AclItem));
    if (acl == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    acl->ndim = 1;
    acl->nitems = nitems;
    return acl;
}

/**
 * 添加一个角色
 */
void add_role(Oid roleid, const char *rolename) {
    roles = (Role *)realloc(roles, (nroles + 1) * sizeof(Role));
    if (roles == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    roles[nroles].roleid = roleid;
    roles[nroles].rolename = strdup(rolename);
    roles[nroles].members = NULL;
    roles[nroles].nmembers = 0;
    
    nroles++;
}

/**
 * 添加角色成员关系
 */
void add_role_member(Oid roleid, Oid memberid) {
    int i;
    for (i = 0; i < nroles; i++) {
        if (roles[i].roleid == roleid) {
            roles[i].members = (Oid *)realloc(roles[i].members, 
                                             (roles[i].nmembers + 1) * sizeof(Oid));
            if (roles[i].members == NULL) {
                fprintf(stderr, "内存分配失败\n");
                exit(1);
            }
            roles[i].members[roles[i].nmembers] = memberid;
            roles[i].nmembers++;
            return;
        }
    }
    fprintf(stderr, "角色 %d 不存在\n", roleid);
}

/**
 * 添加一个对象
 */
void add_object(Oid objid, const char *objname, Oid owner) {
    objects = (Object *)realloc(objects, (nobjects + 1) * sizeof(Object));
    if (objects == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    objects[nobjects].objid = objid;
    objects[nobjects].objname = strdup(objname);
    objects[nobjects].owner = owner;
    objects[nobjects].acl = create_acl(0);
    objects[nobjects].columns = NULL;
    objects[nobjects].ncolumns = 0;
    
    nobjects++;
}

/**
 * 添加一个列
 */
void add_column(Oid colid, Oid objid, const char *colname) {
    int i;
    
    /* 添加到全局列数组 */
    columns = (Column *)realloc(columns, (ncolumns + 1) * sizeof(Column));
    if (columns == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    columns[ncolumns].colid = colid;
    columns[ncolumns].objid = objid;
    columns[ncolumns].colname = strdup(colname);
    columns[ncolumns].acl = create_acl(0);
    
    /* 添加到对象的列数组 */
    for (i = 0; i < nobjects; i++) {
        if (objects[i].objid == objid) {
            objects[i].columns = (Column **)realloc(objects[i].columns, 
                                                  (objects[i].ncolumns + 1) * sizeof(Column *));
            if (objects[i].columns == NULL) {
                fprintf(stderr, "内存分配失败\n");
                exit(1);
            }
            objects[i].columns[objects[i].ncolumns] = &columns[ncolumns];
            objects[i].ncolumns++;
            break;
        }
    }
    
    ncolumns++;
}

/**
 * 为对象添加 ACL 项
 */
void add_acl_item(Oid objid, Oid grantee, Oid grantor, AclMode privs) {
    int i;
    for (i = 0; i < nobjects; i++) {
        if (objects[i].objid == objid) {
            int nitems = objects[i].acl->nitems;
            Acl *new_acl = create_acl(nitems + 1);
            
            /* 复制现有 ACL 项 */
            for (int j = 0; j < nitems; j++) {
                new_acl->items[j] = objects[i].acl->items[j];
            }
            
            /* 添加新的 ACL 项 */
            new_acl->items[nitems].ai_grantee = grantee;
            new_acl->items[nitems].ai_grantor = grantor;
            new_acl->items[nitems].ai_privs = privs;
            
            /* 替换旧的 ACL */
            free(objects[i].acl);
            objects[i].acl = new_acl;
            
            return;
        }
    }
    fprintf(stderr, "对象 %d 不存在\n", objid);
}

/**
 * 为列添加 ACL 项
 */
void add_column_acl_item(Oid colid, Oid grantee, Oid grantor, AclMode privs) {
    int i;
    for (i = 0; i < ncolumns; i++) {
        if (columns[i].colid == colid) {
            int nitems = columns[i].acl->nitems;
            Acl *new_acl = create_acl(nitems + 1);
            
            /* 复制现有 ACL 项 */
            for (int j = 0; j < nitems; j++) {
                new_acl->items[j] = columns[i].acl->items[j];
            }
            
            /* 添加新的 ACL 项 */
            new_acl->items[nitems].ai_grantee = grantee;
            new_acl->items[nitems].ai_grantor = grantor;
            new_acl->items[nitems].ai_privs = privs;
            
            /* 替换旧的 ACL */
            free(columns[i].acl);
            columns[i].acl = new_acl;
            
            return;
        }
    }
    fprintf(stderr, "列 %d 不存在\n", colid);
}

/**
 * 检查角色是否有另一个角色的权限
 */
bool has_privs_of_role(Oid roleid, Oid target_roleid) {
    /* 如果是同一个角色，直接返回 true */
    if (roleid == target_roleid)
        return true;
    
    /* 遍历所有角色，检查成员关系 */
    for (int i = 0; i < nroles; i++) {
        if (roles[i].roleid == target_roleid) {
            /* 检查直接成员关系 */
            for (int j = 0; j < roles[i].nmembers; j++) {
                if (roles[i].members[j] == roleid)
                    return true;
                
                /* 递归检查间接成员关系 */
                if (has_privs_of_role(roleid, roles[i].members[j]))
                    return true;
            }
            break;
        }
    }
    
    return false;
}

/**
 * 计算角色对对象的有效权限
 * 
 * 这个函数模拟 PostgreSQL 中的 aclmask 函数
 * 简化版本：默认使用 ACLMASK_ALL 模式，要求拥有所有指定权限
 */
AclMode aclmask(const Acl *acl, Oid roleid, Oid ownerId, AclMode mask) {
    AclMode result = 0;
    AclMode remaining;
    
    /* 空 ACL 检查 */
    if (acl == NULL) {
        fprintf(stderr, "ACL 为空\n");
        return 0;
    }
    
    /* 快速退出 */
    if (mask == 0)
        return 0;
    
    /* 对象所有者自动拥有所有授权选项 */
    if (has_privs_of_role(roleid, ownerId)) {
        result = mask & (ACLITEM_ALL_PRIV_BITS | ACLITEM_ALL_GOPTION_BITS);
        return result;
    }
    
    /* 检查直接授予角色或 PUBLIC 的权限 */
    for (int i = 0; i < acl->nitems; i++) {
        const AclItem *aidata = &acl->items[i];
        
        if (aidata->ai_grantee == ACL_ID_PUBLIC ||
            aidata->ai_grantee == roleid) {
            result |= aidata->ai_privs & mask;
            if (result == mask)  /* 已获得所有请求的权限 */
                return result;
        }
    }
    
    /* 检查通过角色成员关系间接授予的权限 */
    remaining = mask & ~result;
    for (int i = 0; i < acl->nitems; i++) {
        const AclItem *aidata = &acl->items[i];
        
        if (aidata->ai_grantee == ACL_ID_PUBLIC ||
            aidata->ai_grantee == roleid)
            continue;  /* 已经检查过 */
        
        if ((aidata->ai_privs & remaining) &&
            has_privs_of_role(roleid, aidata->ai_grantee)) {
            result |= aidata->ai_privs & mask;
            if (result == mask)  /* 已获得所有请求的权限 */
                return result;
            remaining = mask & ~result;
        }
    }
    
    return result;
}
