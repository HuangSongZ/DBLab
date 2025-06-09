#include "varlena.h"
#include <iostream>
#include <cassert>

// 测试函数
void test_text_creation() {
    std::cout << "Testing text creation..." << std::endl;
    
    // 从C字符串创建text
    text* t1 = text::cstring_to_text("Hello, PostgreSQL!");
    std::cout << "Text content: " << t1->to_string() << std::endl;
    std::cout << "Text length: " << t1->length() << std::endl;
    std::cout << "Is short format: " << (t1->is_short() ? "Yes" : "No") << std::endl;
    
    // 从std::string创建text
    std::string str = "这是一个中文字符串测试";
    text* t2 = text::string_to_text(str);
    std::cout << "Text content: " << t2->to_string() << std::endl;
    std::cout << "Text length: " << t2->length() << std::endl;
    std::cout << "Is short format: " << (t2->is_short() ? "Yes" : "No") << std::endl;
    
    // 创建一个长字符串，确保使用标准格式
    std::string long_str(100, 'A');
    text* t3 = text::string_to_text(long_str);
    std::cout << "Long text length: " << t3->length() << std::endl;
    std::cout << "Is short format: " << (t3->is_short() ? "Yes" : "No") << std::endl;
    
    // 释放内存
    t1->free();
    t2->free();
    t3->free();
}

void test_text_operations() {
    std::cout << "\nTesting text operations..." << std::endl;
    
    // 创建两个text对象
    text* t1 = text::cstring_to_text("Hello, ");
    text* t2 = text::cstring_to_text("PostgreSQL!");
    
    // 测试连接
    text* t3 = text::text_concat(t1, t2);
    std::cout << "Concatenated text: " << t3->to_string() << std::endl;
    
    // 测试子串
    text* t4 = text::text_substring(t3, 1, 5);  // "Hello"
    std::cout << "Substring (1,5): " << t4->to_string() << std::endl;
    
    text* t5 = text::text_substring(t3, 8, 10);  // "PostgreSQL"
    std::cout << "Substring (8,10): " << t5->to_string() << std::endl;
    
    // 测试转换为C字符串
    char* cstr = t3->text_to_cstring();
    std::cout << "As C string: " << cstr << std::endl;
    free(cstr);
    
    // 释放内存
    t1->free();
    t2->free();
    t3->free();
    t4->free();
    t5->free();
}

void test_edge_cases() {
    std::cout << "\nTesting edge cases..." << std::endl;
    
    // 空字符串
    text* t1 = text::cstring_to_text("");
    std::cout << "Empty text length: " << t1->length() << std::endl;
    std::cout << "Empty text content: '" << t1->to_string() << "'" << std::endl;
    
    // 边界情况的子串
    text* t2 = text::cstring_to_text("Hello");
    text* t3 = text::text_substring(t2, 0, 10);  // 超出范围
    std::cout << "Out of range substring: '" << t3->to_string() << "'" << std::endl;
    
    text* t4 = text::text_substring(t2, 10, 5);  // 起始位置超出范围
    std::cout << "Start position out of range: '" << t4->to_string() << "'" << std::endl;
    
    // 释放内存
    t1->free();
    t2->free();
    t3->free();
    t4->free();
}

int main() {
    std::cout << "=== PostgreSQL text类型实现演示 ===" << std::endl;
    
    test_text_creation();
    test_text_operations();
    test_edge_cases();
    
    std::cout << "\n所有测试完成！" << std::endl;
    return 0;
}
