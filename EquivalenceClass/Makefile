CC = gcc
CFLAGS = -Wall -g -O0

# 目标文件
TARGET = equivalence_class_demo

# 默认目标
all: $(TARGET)

# 编译规则
$(TARGET): equivalence_class_demo.c
	$(CC) $(CFLAGS) -o $@ $<

# 清理规则
clean:
	rm -f $(TARGET)

# 防止与同名文件冲突
.PHONY: all clean
