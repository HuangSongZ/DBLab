CC = gcc
CFLAGS = -Wall -g

# 目标文件
TARGET = pg_interrupt_demo

# 默认目标
all: $(TARGET)

# 编译规则
$(TARGET): pg_interrupt_demo.c
	$(CC) $(CFLAGS) -o $@ $<

# 清理规则
clean:
	rm -f $(TARGET)

# 防止与同名文件冲突
.PHONY: all clean
