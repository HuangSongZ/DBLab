#include "varlena.h"
#include <cstdlib>
#include <algorithm>
#include <stdexcept>

// varlena类的实现

// 从C风格字符串创建varlena
varlena* varlena::cstring_to_varlena(const char* str, size_t len) {
    // 判断是否可以使用短格式
    bool use_short = (len + VARHDRSZ_SHORT <= VARATT_SHORT_MAX);
    size_t total_size = len + (use_short ? VARHDRSZ_SHORT : VARHDRSZ);
    
    // 分配内存
    varlena* result = static_cast<varlena*>(malloc(total_size));
    if (!result) {
        throw std::bad_alloc();
    }
    
    // 设置头部
    if (use_short) {
        // 设置短格式头部：最低位为1，其余位存储长度
        reinterpret_cast<uint8_t*>(result)[0] = (total_size << 1) | 0x01;
        // 复制数据
        memcpy(reinterpret_cast<char*>(result) + VARHDRSZ_SHORT, str, len);
    } else {
        // 设置标准格式头部：最低位为0，其余位存储长度
        reinterpret_cast<uint32_t*>(result)[0] = (total_size << 2);
        // 复制数据
        memcpy(reinterpret_cast<char*>(result) + VARHDRSZ, str, len);
    }
    
    return result;
}

// 从std::string创建varlena
varlena* varlena::string_to_varlena(const std::string& str) {
    return cstring_to_varlena(str.c_str(), str.length());
}

// 获取数据指针
char* varlena::data() {
    return is_short() ? 
        reinterpret_cast<char*>(this) + VARHDRSZ_SHORT : 
        reinterpret_cast<char*>(this) + VARHDRSZ;
}

const char* varlena::data() const {
    return is_short() ? 
        reinterpret_cast<const char*>(this) + VARHDRSZ_SHORT : 
        reinterpret_cast<const char*>(this) + VARHDRSZ;
}

// 获取数据长度（不包括头部）
size_t varlena::length() const {
    if (is_short()) {
        // 短格式：长度在第一个字节的高7位
        return (reinterpret_cast<const uint8_t*>(this)[0] >> 1) - VARHDRSZ_SHORT;
    } else {
        // 标准格式：长度在前4个字节的高30位
        return (reinterpret_cast<const uint32_t*>(this)[0] >> 2) - VARHDRSZ;
    }
}

// 获取总大小（包括头部）
size_t varlena::size() const {
    if (is_short()) {
        // 短格式：长度在第一个字节的高7位
        return reinterpret_cast<const uint8_t*>(this)[0] >> 1;
    } else {
        // 标准格式：长度在前4个字节的高30位
        return reinterpret_cast<const uint32_t*>(this)[0] >> 2;
    }
}

// 转换为std::string
std::string varlena::to_string() const {
    return std::string(data(), length());
}

// 判断是否为短格式
bool varlena::is_short() const {
    // 检查第一个字节的最低位
    // 在PostgreSQL中，这个判断更复杂，考虑了字节序
    return (reinterpret_cast<const uint8_t*>(this)[0] & 0x01) != 0;
}

// 释放内存
void varlena::free() {
    ::free(this);
}

// text类的实现

// 从C风格字符串创建text
text* text::cstring_to_text(const char* str) {
    return cstring_to_text_with_len(str, strlen(str));
}

// 从指定长度的C风格字符串创建text
text* text::cstring_to_text_with_len(const char* str, size_t len) {
    return static_cast<text*>(varlena::cstring_to_varlena(str, len));
}

// 从std::string创建text
text* text::string_to_text(const std::string& str) {
    return static_cast<text*>(varlena::string_to_varlena(str));
}

// 转换为C风格字符串
char* text::text_to_cstring() const {
    size_t len = length();
    char* result = static_cast<char*>(malloc(len + 1));
    if (!result) {
        throw std::bad_alloc();
    }
    
    memcpy(result, data(), len);
    result[len] = '\0';
    
    return result;
}

// 文本连接
text* text::text_concat(const text* t1, const text* t2) {
    size_t len1 = t1->length();
    size_t len2 = t2->length();
    size_t total_len = len1 + len2;
    
    // 创建新的text
    bool use_short = (total_len + VARHDRSZ_SHORT <= VARATT_SHORT_MAX);
    size_t total_size = total_len + (use_short ? VARHDRSZ_SHORT : VARHDRSZ);
    
    text* result = static_cast<text*>(malloc(total_size));
    if (!result) {
        throw std::bad_alloc();
    }
    
    // 设置头部
    if (use_short) {
        // 设置短格式头部：最低位为1，其余位存储长度
        reinterpret_cast<uint8_t*>(result)[0] = (total_size << 1) | 0x01;
        // 复制数据
        char* dest = reinterpret_cast<char*>(result) + VARHDRSZ_SHORT;
        memcpy(dest, t1->data(), len1);
        memcpy(dest + len1, t2->data(), len2);
    } else {
        // 设置标准格式头部：最低位为0，其余位存储长度
        reinterpret_cast<uint32_t*>(result)[0] = (total_size << 2);
        // 复制数据
        char* dest = reinterpret_cast<char*>(result) + VARHDRSZ;
        memcpy(dest, t1->data(), len1);
        memcpy(dest + len1, t2->data(), len2);
    }
    
    return result;
}

// 文本子串
text* text::text_substring(const text* t, int start, int length) {
    size_t text_len = t->length();
    
    // 调整起始位置（PostgreSQL风格，1-based索引）
    if (start <= 0) {
        start = 1;
    }
    
    // 转换为0-based索引
    start = start - 1;
    
    // 边界检查
    if (start >= static_cast<int>(text_len)) {
        // 返回空字符串
        return cstring_to_text("");
    }
    
    // 调整长度
    if (length < 0) {
        length = text_len - start;
    } else {
        length = std::min(static_cast<size_t>(length), text_len - start);
    }
    
    // 创建子串
    return cstring_to_text_with_len(t->data() + start, length);
}
