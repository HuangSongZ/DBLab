CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lm

SRCS = main.c expr_tree.c tree_evaluator.c flat_evaluator.c
OBJS = $(SRCS:.c=.o)
TARGET = expr_demo

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
