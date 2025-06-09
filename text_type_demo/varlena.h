#ifndef VARLENA_H
#define VARLENA_H

#include <cstdint>
#include <cstring>
#include <string>

// 定义一些常量，类似于PostgreSQL中的定义
#define VARHDRSZ 4  // 标准varlena头部大小
#define VARHDRSZ_SHORT 1  // 短varlena头部大小
#define VARATT_SHORT_MAX 0x7F  // 短varlena最大长度

// 变长数据类型的头部结构
struct varlena_header {
    uint32_t length;  // 包含头部的总长度
};

// 短变长数据类型的头部结构
struct varlena_short_header {
    uint8_t length;  // 包含头部的总长度
};

// 变长数据类型的基本结构
class varlena {
public:
    // 从C风格字符串创建varlena
    static varlena* cstring_to_varlena(const char* str, size_t len);
    
    // 从std::string创建varlena
    static varlena* string_to_varlena(const std::string& str);
    
    // 获取数据指针
    char* data();
    const char* data() const;
    
    // 获取数据长度（不包括头部）
    size_t length() const;
    
    // 获取总大小（包括头部）
    size_t size() const;
    
    // 转换为std::string
    std::string to_string() const;
    
    // 判断是否为短格式
    bool is_short() const;
    
    // 释放内存
    void free();

private:
    // 私有构造函数，不允许直接创建
    varlena() = default;
};

// text类型 - 继承自varlena
class text : public varlena {
public:
    // 从C风格字符串创建text
    static text* cstring_to_text(const char* str);
    static text* cstring_to_text_with_len(const char* str, size_t len);
    
    // 从std::string创建text
    static text* string_to_text(const std::string& str);
    
    // 转换为C风格字符串（调用者负责释放内存）
    char* text_to_cstring() const;
    
    // 文本连接
    static text* text_concat(const text* t1, const text* t2);
    
    // 文本子串
    static text* text_substring(const text* t, int start, int length);
};

// 宏定义
#define VARDATA(ptr) (reinterpret_cast<char*>(ptr) + VARHDRSZ)
#define VARSIZE(ptr) (reinterpret_cast<const varlena_header*>(ptr)->length)
#define VARSIZE_SHORT(ptr) (reinterpret_cast<const varlena_short_header*>(ptr)->length)
#define SET_VARSIZE(ptr, len) (reinterpret_cast<varlena_header*>(ptr)->length = (len))
#define SET_VARSIZE_SHORT(ptr, len) (reinterpret_cast<varlena_short_header*>(ptr)->length = (len))

#endif // VARLENA_H
