CC = gcc
CFLAGS = -Wall -g

all: pg_acl_demo pg_acl_demo_column

# 基础版本
pg_acl_demo: pg_acl_demo.c
	$(CC) $(CFLAGS) -o $@ $<

# 扩展版本
pg_acl_demo_column: pg_acl_demo_column.c pg_acl_demo_column_check.c pg_acl_demo_column_utils.c pg_acl_demo_column_main.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f pg_acl_demo pg_acl_demo_column *.o

.PHONY: all clean
