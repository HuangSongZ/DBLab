# PostgreSQL 权限控制系统模拟 Demo

这个项目模拟了 PostgreSQL 权限控制系统的核心机制，包括：

- 角色和角色继承
- ACL 数据结构
- 表级和列级权限控制
- 权限检查逻辑
- 权限授予和撤销操作


## 功能说明

1. **角色管理**：创建角色、设置角色继承关系
2. **对象管理**：创建表和列
3. **权限管理**：授予和撤销权限
4. **权限检查**：检查角色对表和列的权限
5. **查询执行**：模拟查询执行过程中的权限检查

## 参考资料

- PostgreSQL 官方文档 - 权限系统: https://www.postgresql.org/docs/current/ddl-priv.html
- PostgreSQL 源码中的 `acl.c` 和 `aclchk.c` 文件
