CXX = g++
CXXFLAGS = -std=c++11 -Wall -pthread
TARGET = pg_cache_inval_demo
SRCS = pg_cache_inval_demo.cpp
HEADERS = invalidation_message.h shared_inval_queue.h cache.h transaction.h backend.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
