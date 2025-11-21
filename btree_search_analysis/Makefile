# Makefile for B+tree search demo

CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -std=c99
TARGET = btree_search_demo

# 源文件
SRCS = btree_search_demo.c
OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 编译可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build complete: $(TARGET)"

# 编译目标文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 运行程序
run: $(TARGET)
	@echo "Running $(TARGET)..."
	@echo ""
	./$(TARGET)

# 清理
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Clean complete"

# 重新编译
rebuild: clean all

.PHONY: all run clean rebuild
