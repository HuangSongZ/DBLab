# PostgreSQL text数据类型C++实现演示

这个项目是PostgreSQL中text数据类型的C++实现演示。它模拟了PostgreSQL中变长数据类型的内部结构和操作方式，特别是text类型的实现。

## 项目结构

- `varlena.h` - 变长数据类型的头文件，定义了基本结构和接口
- `varlena.cpp` - 变长数据类型的实现文件
- `main.cpp` - 演示程序，展示如何使用text数据类型
- `Makefile` - 用于编译项目

## 核心概念

本演示实现了以下PostgreSQL中的核心概念：

1. **变长数据结构 (varlena)** - 基本的变长数据结构，包含头部和数据部分
2. **短格式优化** - 对于小数据使用1字节头部而不是4字节头部
3. **text数据类型** - 基于varlena的文本数据类型
4. **常用操作** - 创建、连接、截取子串等操作

## 编译和运行

```bash
# 编译项目
make

# 运行演示程序
./text_demo
```

## 主要接口

### varlena类

- `cstring_to_varlena()` - 从C字符串创建varlena
- `string_to_varlena()` - 从std::string创建varlena
- `data()` - 获取数据指针
- `length()` - 获取数据长度（不包括头部）
- `size()` - 获取总大小（包括头部）
- `to_string()` - 转换为std::string
- `is_short()` - 判断是否为短格式
- `free()` - 释放内存

### text类

- `cstring_to_text()` - 从C字符串创建text
- `string_to_text()` - 从std::string创建text
- `text_to_cstring()` - 转换为C字符串
- `text_concat()` - 文本连接
- `text_substring()` - 文本子串

## 与PostgreSQL的区别

这个实现是PostgreSQL text类型的简化版，与实际PostgreSQL实现的主要区别：

1. 没有实现TOAST机制（用于处理大型数据）
2. 没有实现压缩功能
3. 简化了短格式判断逻辑
4. 使用C++类而不是C结构体

## 参考资料

- PostgreSQL源代码: `src/include/c.h`中的`struct varlena`定义
- PostgreSQL源代码: `src/backend/utils/adt/varlena.c`中的text操作函数
