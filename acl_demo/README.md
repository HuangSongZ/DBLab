# PostgreSQL 权限控制系统模拟 Demo

这个项目模拟了 PostgreSQL 权限控制系统的核心机制，包括：

- 角色和角色继承
- ACL 数据结构
- 表级和列级权限控制
- 权限检查逻辑
- 权限授予和撤销操作

## 文件说明

- `pg_acl_demo.c`: 基础版本，实现了表级权限控制
- `pg_acl_demo_column.c`: 扩展版本，添加了列级权限支持
- `pg_acl_demo_full.c`: 完整版本，包含查询执行和行级安全策略

## 编译和运行

```bash
# 编译基础版本
gcc -o pg_acl_demo pg_acl_demo.c

# 编译扩展版本
gcc -o pg_acl_demo_column pg_acl_demo_column.c

# 编译完整版本
gcc -o pg_acl_demo_full pg_acl_demo_full.c

# 运行
./pg_acl_demo
./pg_acl_demo_column
./pg_acl_demo_full
```

## 功能说明

1. **角色管理**：创建角色、设置角色继承关系
2. **对象管理**：创建表和列
3. **权限管理**：授予和撤销权限
4. **权限检查**：检查角色对表和列的权限
5. **查询执行**：模拟查询执行过程中的权限检查
6. **行级安全**：模拟行级安全策略与 ACL 的交互

## 参考资料

- PostgreSQL 官方文档 - 权限系统: https://www.postgresql.org/docs/current/ddl-priv.html
- PostgreSQL 源码中的 `acl.c` 和 `aclchk.c` 文件
