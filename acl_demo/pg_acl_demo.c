#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * 模拟 PostgreSQL 权限控制系统的简单 demo
 * 
 * 这个程序实现了 PostgreSQL 权限控制系统的核心概念，包括：
 * - 角色和角色继承
 * - ACL 数据结构
 * - 权限检查逻辑
 */

/* 定义权限位 */
#define ACL_SELECT      (1<<0)  /* SELECT */
#define ACL_INSERT      (1<<1)  /* INSERT */
#define ACL_UPDATE      (1<<2)  /* UPDATE */
#define ACL_DELETE      (1<<3)  /* DELETE */

/* 定义授权选项位 */
#define ACL_GRANT_OPTION_SELECT (1<<8)   /* SELECT 的授权选项 */
#define ACL_GRANT_OPTION_INSERT (1<<9)   /* INSERT 的授权选项 */
#define ACL_GRANT_OPTION_UPDATE (1<<10)  /* UPDATE 的授权选项 */
#define ACL_GRANT_OPTION_DELETE (1<<11)  /* DELETE 的授权选项 */

/* 所有授权选项位的掩码 */
#define ACLITEM_ALL_GOPTION_BITS \
    (ACL_GRANT_OPTION_SELECT | ACL_GRANT_OPTION_INSERT | \
     ACL_GRANT_OPTION_UPDATE | ACL_GRANT_OPTION_DELETE)

/* 所有权限位的掩码 */
#define ACLITEM_ALL_PRIV_BITS \
    (ACL_SELECT | ACL_INSERT | ACL_UPDATE | ACL_DELETE)

/* 权限检查模式 */
typedef enum {
    ACLMASK_ALL,  /* 要求拥有所有指定权限 */
    ACLMASK_ANY   /* 只要拥有任一指定权限即可 */
} AclMaskHow;

/* 特殊角色 ID */
#define ACL_ID_PUBLIC 0  /* PUBLIC 角色 ID */

/* 角色 ID 类型 */
typedef int Oid;

/* 权限模式类型 */
typedef unsigned int AclMode;

/* ACL 项结构 */
typedef struct AclItem {
    Oid     ai_grantee;     /* 被授权者 ID */
    Oid     ai_grantor;     /* 授权者 ID */
    AclMode ai_privs;       /* 权限（包括授权选项） */
} AclItem;

/* ACL 结构 */
typedef struct Acl {
    int     ndim;           /* 数组维度 */
    int     nitems;         /* ACL 项数量 */
    AclItem items[1];       /* 实际的 ACL 项目，变长数组 */
} Acl;

/* 角色结构 */
typedef struct Role {
    Oid     roleid;         /* 角色 ID */
    char    *rolename;      /* 角色名称 */
    Oid     *members;       /* 成员角色 ID 数组 */
    int     nmembers;       /* 成员数量 */
} Role;

/* 对象结构 */
typedef struct Object {
    Oid     objid;          /* 对象 ID */
    char    *objname;       /* 对象名称 */
    Oid     owner;          /* 所有者 ID */
    Acl     *acl;           /* 对象的 ACL */
} Object;

/* 全局角色数组 */
Role *roles = NULL;
int nroles = 0;

/* 全局对象数组 */
Object *objects = NULL;
int nobjects = 0;

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
    
    nobjects++;
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
 */
AclMode aclmask(const Acl *acl, Oid roleid, Oid ownerId,
                AclMode mask, AclMaskHow how) {
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
    if ((mask & ACLITEM_ALL_GOPTION_BITS) &&
        has_privs_of_role(roleid, ownerId)) {
        result = mask & ACLITEM_ALL_GOPTION_BITS;
        if ((how == ACLMASK_ALL) ? (result == mask) : (result != 0))
            return result;
    }
    
    /* 检查直接授予角色或 PUBLIC 的权限 */
    for (int i = 0; i < acl->nitems; i++) {
        const AclItem *aidata = &acl->items[i];
        
        if (aidata->ai_grantee == ACL_ID_PUBLIC ||
            aidata->ai_grantee == roleid) {
            result |= aidata->ai_privs & mask;
            if ((how == ACLMASK_ALL) ? (result == mask) : (result != 0))
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
            if ((how == ACLMASK_ALL) ? (result == mask) : (result != 0))
                return result;
            remaining = mask & ~result;
        }
    }
    
    return result;
}

/**
 * 检查对象的权限
 * 
 * 这个函数模拟 PostgreSQL 中的 pg_class_aclmask 函数
 */
AclMode pg_object_aclmask(Oid objid, Oid roleid,
                          AclMode mask, AclMaskHow how) {
    /* 查找对象 */
    for (int i = 0; i < nobjects; i++) {
        if (objects[i].objid == objid) {
            /* 调用 aclmask 计算有效权限 */
            return aclmask(objects[i].acl, roleid, objects[i].owner, mask, how);
        }
    }
    
    fprintf(stderr, "对象 %d 不存在\n", objid);
    return 0;
}

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
 * 检查并打印权限
 */
void check_permission(Oid roleid, Oid objid, AclMode mask, AclMaskHow how) {
    const char *role_name = get_role_name(roleid);
    const char *obj_name = get_object_name(objid);
    AclMode result = pg_object_aclmask(objid, roleid, mask, how);
    
    printf("检查角色 '%s' (ID: %d) 对对象 '%s' (ID: %d) 的权限:\n",
           role_name, roleid, obj_name, objid);
    
    printf("请求的权限: ");
    if (mask & ACL_SELECT) printf("SELECT ");
    if (mask & ACL_INSERT) printf("INSERT ");
    if (mask & ACL_UPDATE) printf("UPDATE ");
    if (mask & ACL_DELETE) printf("DELETE ");
    printf("\n");
    
    printf("有效权限: ");
    if (result & ACL_SELECT) printf("SELECT ");
    if (result & ACL_INSERT) printf("INSERT ");
    if (result & ACL_UPDATE) printf("UPDATE ");
    if (result & ACL_DELETE) printf("DELETE ");
    printf("\n");
    
    if (how == ACLMASK_ALL) {
        if (result == mask) {
            printf("结果: 拥有所有请求的权限\n");
        } else {
            printf("结果: 缺少一些请求的权限\n");
        }
    } else { /* ACLMASK_ANY */
        if (result != 0) {
            printf("结果: 拥有至少一个请求的权限\n");
        } else {
            printf("结果: 没有任何请求的权限\n");
        }
    }
    printf("\n");
}

/**
 * 主函数
 */
int main() {
    printf("PostgreSQL 权限控制系统模拟 Demo\n");
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
    
    /* 设置 ACL */
    /* alice 授予 bob SELECT 权限 */
    add_acl_item(1, 3, 2, ACL_SELECT);
    
    /* alice 授予 analysts 组 SELECT 和 UPDATE 权限 */
    add_acl_item(1, 5, 2, ACL_SELECT | ACL_UPDATE);
    
    /* alice 授予 carol INSERT 权限和 SELECT 的授权选项 */
    add_acl_item(1, 4, 2, ACL_INSERT | ACL_SELECT | ACL_GRANT_OPTION_SELECT);
    
    /* 打印对象的 ACL */
    print_object_acl(1);
    
    printf("\n权限检查示例:\n");
    printf("===================================\n\n");
    
    /* 检查 alice 的权限（所有者） */
    check_permission(2, 1, ACL_SELECT | ACL_INSERT | ACL_UPDATE | ACL_DELETE, ACLMASK_ALL);
    
    /* 检查 bob 的权限（直接授予的权限） */
    check_permission(3, 1, ACL_SELECT, ACLMASK_ALL);
    
    /* 检查 bob 的权限（通过 analysts 组继承的权限） */
    check_permission(3, 1, ACL_UPDATE, ACLMASK_ALL);
    
    /* 检查 bob 的权限（没有的权限） */
    check_permission(3, 1, ACL_INSERT, ACLMASK_ALL);
    
    /* 检查 carol 的权限（直接授予的权限和授权选项） */
    check_permission(4, 1, ACL_INSERT | ACL_GRANT_OPTION_SELECT, ACLMASK_ALL);
    
    /* 使用 ACLMASK_ANY 模式检查 bob 的权限 */
    check_permission(3, 1, ACL_SELECT | ACL_INSERT, ACLMASK_ANY);
    
    /* 清理资源 */
    for (int i = 0; i < nroles; i++) {
        free(roles[i].rolename);
        free(roles[i].members);
    }
    free(roles);
    
    for (int i = 0; i < nobjects; i++) {
        free(objects[i].objname);
        free(objects[i].acl);
    }
    free(objects);
    
    return 0;
}
