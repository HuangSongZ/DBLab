#ifndef PG_ACL_DEMO_COLUMN_H
#define PG_ACL_DEMO_COLUMN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * PostgreSQL 权限控制系统模拟 Demo (扩展版)
 * 
 * 这个头文件定义了模拟 PostgreSQL 权限控制系统所需的数据结构和函数声明
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

/* 我们简化了权限检查逻辑，移除了 AclMaskHow 参数
 * 默认使用 ACLMASK_ALL 模式，即要求拥有所有指定权限
 */

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

/* 列结构 */
typedef struct Column {
    Oid     colid;          /* 列 ID */
    Oid     objid;          /* 所属对象 ID */
    char    *colname;       /* 列名称 */
    Acl     *acl;           /* 列的 ACL */
} Column;

/* 对象结构 */
typedef struct Object {
    Oid     objid;          /* 对象 ID */
    char    *objname;       /* 对象名称 */
    Oid     owner;          /* 所有者 ID */
    Acl     *acl;           /* 对象的 ACL */
    Column  **columns;      /* 列数组 */
    int     ncolumns;       /* 列数量 */
} Object;

/* 查询类型 */
typedef enum {
    QUERY_SELECT,
    QUERY_INSERT,
    QUERY_UPDATE,
    QUERY_DELETE
} QueryType;

/* 简单的查询结构 */
typedef struct Query {
    QueryType type;         /* 查询类型 */
    Oid     objid;          /* 对象 ID */
    Oid     *colids;        /* 涉及的列 ID 数组 */
    int     ncolids;        /* 列数量 */
} Query;

/* 全局角色数组 */
extern Role *roles;
extern int nroles;

/* 全局对象数组 */
extern Object *objects;
extern int nobjects;

/* 全局列数组 */
extern Column *columns;
extern int ncolumns;

/* 函数声明 */

/* ACL 操作函数 */
Acl *create_acl(int nitems);
void add_acl_item(Oid objid, Oid grantee, Oid grantor, AclMode privs);
void add_column_acl_item(Oid colid, Oid grantee, Oid grantor, AclMode privs);

/* 角色操作函数 */
void add_role(Oid roleid, const char *rolename);
void add_role_member(Oid roleid, Oid memberid);
bool has_privs_of_role(Oid roleid, Oid target_roleid);

/* 对象操作函数 */
void add_object(Oid objid, const char *objname, Oid owner);
void add_column(Oid colid, Oid objid, const char *colname);

/* 权限检查函数 */
AclMode aclmask(const Acl *acl, Oid roleid, Oid ownerId, AclMode mask);
AclMode pg_object_aclmask(Oid objid, Oid roleid, AclMode mask);
AclMode pg_column_aclmask(Oid colid, Oid roleid, AclMode mask);

/* 权限授予和撤销函数 */
void grant_object_permission(Oid objid, Oid grantee, Oid grantor, AclMode privs);
void revoke_object_permission(Oid objid, Oid grantee, Oid grantor, AclMode privs);
void grant_column_permission(Oid colid, Oid grantee, Oid grantor, AclMode privs);
void revoke_column_permission(Oid colid, Oid grantee, Oid grantor, AclMode privs);

/* 查询执行函数 */
bool check_query_permissions(Query *query, Oid roleid);
bool execute_query(Query *query, Oid roleid);

/* 辅助函数 */
const char *get_role_name(Oid roleid);
const char *get_object_name(Oid objid);
const char *get_column_name(Oid colid);
void print_privs(AclMode privs);
void print_object_acl(Oid objid);
void print_column_acl(Oid colid);
void check_permission(Oid roleid, Oid objid, AclMode mask);
void check_column_permission(Oid roleid, Oid colid, AclMode mask);

#endif /* PG_ACL_DEMO_COLUMN_H */
