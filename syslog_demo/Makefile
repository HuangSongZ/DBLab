CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = 

all: syslogger backend

syslogger: syslogger.c common.h
	$(CC) $(CFLAGS) -o syslogger syslogger.c $(LDFLAGS)

backend: backend.c common.h
	$(CC) $(CFLAGS) -o backend backend.c $(LDFLAGS)

clean:
	rm -f syslogger backend syslog_demo.log

.PHONY: all clean
