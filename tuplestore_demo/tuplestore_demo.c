#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/**
 * TupleStore - 一个简单的元组存储实现
 * 支持内存和磁盘存储，自动溢出到磁盘
 */

/* 常量定义 */
#define TUPLESTORE_INITIAL_CAPACITY 100   /* 初始元组数组容量 */
#define TUPLESTORE_MIN_BUFFER_SIZE 10      /* 最小缓冲区大小 */
#define TUPLESTORE_BUFFER_MEMORY_RATIO 0.5 /* 缓冲区内存占比 */
#define TUPLESTORE_FLUSH_THRESHOLD 0.75    /* 缓冲区刷新阈值 */
#define TUPLESTORE_TEMP_FILE_TEMPLATE "/tmp/tuplestore_XXXXXX"

/* 缓冲区模式 */
typedef enum {
    BUFFER_MODE_READ = 0,
    BUFFER_MODE_WRITE = 1
} BufferMode;

/* 错误码 */
typedef enum {
    TUPLESTORE_SUCCESS = 0,
    TUPLESTORE_ERROR_MEMORY = -1,
    TUPLESTORE_ERROR_IO = -2,
    TUPLESTORE_ERROR_INVALID_PARAM = -3,
    TUPLESTORE_ERROR_INTERNAL = -4,
    TUPLESTORE_ERROR_CLEANUP = -5
} TupleStoreError;

/* 定义元组结构 */
typedef struct {
    int id;
    char data[100];
} Tuple;

/* 内存中的元组存储 */
typedef struct {
    Tuple **tuples;      /* 元组指针数组 */
    int capacity;        /* 数组容量 */
    int count;           /* 当前元组数量 */
    int read_pos;        /* 当前读取位置 */
    int max_memory_kb;   /* 最大内存限制(KB) */
    int current_memory;  /* 当前使用的内存(bytes) */
    FILE *temp_file;     /* 临时文件，当内存不足时使用 */
    int using_file;      /* 是否正在使用文件 */
    int file_count;      /* 文件中的元组数量 */
    char *filename;      /* 临时文件名 */
    
    /* 文件缓冲区相关 */
    Tuple *buffer;       /* 连续内存块，存储元组数据 */
    int buffer_size;     /* 缓冲区大小（元组数） */
    int buffer_start;    /* 缓冲区中第一个元组在文件中的位置（读取模式） */
    int buffer_count;    /* 缓冲区中当前的元组数量 */
    int buffer_write_mode; /* 缓冲区模式（0=读取，1=写入） */
} TupleStore;

/* 错误处理函数 */
static void tuplestore_error(const char *message) {
    fprintf(stderr, "TupleStore错误: %s (%s)\n", message, strerror(errno));
}

/* 元组分配函数 */
static Tuple* tuplestore_alloc_tuple(void) {
    Tuple *tuple = (Tuple*)malloc(sizeof(Tuple));
    if (!tuple) {
        tuplestore_error("无法为元组分配内存");
    }
    return tuple;
}

/* 创建元组存储 */
TupleStore* tuplestore_create(int max_memory_kb) {
    if (max_memory_kb <= 0) {
        tuplestore_error("无效的内存限制参数");
        return NULL;
    }
    
    TupleStore *store = (TupleStore*)malloc(sizeof(TupleStore));
    if (!store) {
        tuplestore_error("无法为TupleStore分配内存");
        return NULL;
    }
    
    // 初始化基本属性
    store->capacity = TUPLESTORE_INITIAL_CAPACITY;
    store->count = 0;
    store->read_pos = 0;
    store->max_memory_kb = max_memory_kb;
    store->current_memory = sizeof(TupleStore);
    store->temp_file = NULL;
    store->using_file = 0;
    store->file_count = 0;
    store->filename = NULL;
    
    // 分配元组数组
    store->tuples = (Tuple**)malloc(sizeof(Tuple*) * store->capacity);
    if (!store->tuples) {
        tuplestore_error("无法为元组数组分配内存");
        free(store);
        return NULL;
    }
    store->current_memory += sizeof(Tuple*) * store->capacity;
    
    // 文件缓冲区不计入内存使用
    store->buffer_size = TUPLESTORE_MIN_BUFFER_SIZE;
    
    // 分配连续内存块用于存储元组数据
    store->buffer = (Tuple*)malloc(sizeof(Tuple) * store->buffer_size);
    if (!store->buffer) {
        tuplestore_error("无法为缓冲区分配内存");
        free(store->tuples);
        free(store);
        return NULL;
    }
    
    // 初始化缓冲区
    store->buffer_start = 0;
    store->buffer_count = 0;
    store->buffer_write_mode = BUFFER_MODE_READ;  // 初始化为读取模式
    
    return store;
}

/* 将缓冲区中的元组刷新到文件（写入模式） */
int tuplestore_flush_buffer(TupleStore *store) {
    // 参数检查
    if (!store) {
        tuplestore_error("无效的TupleStore指针");
        return TUPLESTORE_ERROR_INVALID_PARAM;
    }
    
    // 如果没有需要刷新的内容，直接返回成功
    if (!store->using_file || !store->temp_file || 
        store->buffer_write_mode != BUFFER_MODE_WRITE || 
        store->buffer_count == 0) {
        return TUPLESTORE_SUCCESS;
    }
    
    // 将文件指针移动到文件末尾
    if (fseek(store->temp_file, 0, SEEK_END) != 0) {
        tuplestore_error("无法定位到文件末尾");
        return TUPLESTORE_ERROR_IO;
    }
    
    // 注意：使用连续内存块后，不需要复制元组
    
    // 直接从连续内存块中批量写入文件
    size_t tuples_written = fwrite(store->buffer, sizeof(Tuple), store->buffer_count, store->temp_file);
    
    // 检查写入是否成功
    if (tuples_written != store->buffer_count) {
        tuplestore_error("将元组写入文件失败");
        return TUPLESTORE_ERROR_IO;
    }
    
    // 更新文件中的元组数量
    store->file_count += store->buffer_count;
    store->buffer_count = 0;
    
    // 刷新文件缓冲区
    fflush(store->temp_file);
    
    // 重置缓冲区模式为读取模式
    store->buffer_write_mode = BUFFER_MODE_READ;
    
    return TUPLESTORE_SUCCESS;
}

/* 将内存中的元组转储到文件 */
int tuplestore_dump_to_file(TupleStore *store) {
    // 参数检查
    if (!store) {
        tuplestore_error("无效的TupleStore指针");
        return TUPLESTORE_ERROR_INVALID_PARAM;
    }
    
    // 如果没有元组要转储，直接返回成功
    if (store->count == 0) {
        return TUPLESTORE_SUCCESS;
    }
    
    // 如果文件尚未创建，创建一个临时文件
    if (!store->temp_file) {
        char template[sizeof(TUPLESTORE_TEMP_FILE_TEMPLATE)];
        strcpy(template, TUPLESTORE_TEMP_FILE_TEMPLATE);
        
        int fd = mkstemp(template);
        if (fd == -1) {
            tuplestore_error("无法创建临时文件");
            return TUPLESTORE_ERROR_IO;
        }
        
        store->temp_file = fdopen(fd, "w+b");
        if (!store->temp_file) {
            tuplestore_error("无法打开临时文件");
            close(fd);
            unlink(template);
            return TUPLESTORE_ERROR_IO;
        }
        
        store->filename = strdup(template);
        if (!store->filename) {
            tuplestore_error("无法复制文件名");
            fclose(store->temp_file);
            store->temp_file = NULL;
            unlink(template);
            return TUPLESTORE_ERROR_MEMORY;
        }
    }
    
    // 尝试批量写入元组
    size_t tuples_written = 0;
    int result = TUPLESTORE_SUCCESS;
    
    // 直接从内存中写入元组到文件
    for (int i = 0; i < store->count; i++) {
        if (store->tuples[i]) {
            if (fwrite(store->tuples[i], sizeof(Tuple), 1, store->temp_file) != 1) {
                tuplestore_error("将元组写入文件失败");
                result = TUPLESTORE_ERROR_IO;
                // 继续释放内存，即使写入失败
            }
            
            // 释放元组内存
            free(store->tuples[i]);
            store->tuples[i] = NULL;
            // store->current_memory -= sizeof(Tuple);
            tuples_written++;
        }
    }
    
    // 更新状态
    store->file_count += store->count;
    store->count = 0;
    store->using_file = 1;
    
    // 刷新文件缓冲区
    fflush(store->temp_file);
    
    // 注意：不重置文件指针，保持在文件末尾以便后续写入
    // 读取操作会在tuplestore_rescan或tuplestore_fill_buffer中重置文件指针
    
    return result;
}

/* 添加元组到存储中 */
int tuplestore_put(TupleStore *store, int id, const char *data) {
    // 参数检查
    if (!store || !data) {
        tuplestore_error("无效的参数");
        return TUPLESTORE_ERROR_INVALID_PARAM;
    }
    
    // 检查内存是否足够
    int tuple_size = sizeof(Tuple);
    if (store->current_memory + tuple_size > store->max_memory_kb * 1024) {
        // 内存不足，转储到文件
        int result = tuplestore_dump_to_file(store);
        if (result != TUPLESTORE_SUCCESS) {
            return result;
        }
    }
    
    // 如果不使用文件，检查并处理数组容量
    if (!store->using_file && store->count >= store->capacity) {
        // 计算扩容后的新容量
        int new_capacity = store->capacity * 2;
        int memory_increase = sizeof(Tuple*) * (new_capacity - store->capacity);
        
        // 如果扩容会超过内存限制
        if (store->current_memory + memory_increase > store->max_memory_kb * 1024) {
            // 计算在不超过内存限制的情况下可以扩容的最大容量
            int max_allowed_increase = (store->max_memory_kb * 1024) - store->current_memory;
            int max_new_capacity = store->capacity + (max_allowed_increase / sizeof(Tuple*));
            
            // 如果无法进一步扩容，则转储到文件
            if (max_new_capacity <= store->capacity) {
                int result = tuplestore_dump_to_file(store);
                if (result != TUPLESTORE_SUCCESS) {
                    return result;
                }
            } else {
                // 只扩容到允许的最大容量
                new_capacity = max_new_capacity;
                
                // 重新分配内存
                Tuple **new_tuples = (Tuple**)realloc(store->tuples, sizeof(Tuple*) * new_capacity);
                if (!new_tuples) {
                    tuplestore_error("无法为元组数组重新分配内存");
                    return TUPLESTORE_ERROR_MEMORY;
                }
                
                // 更新内存使用和容量
                store->current_memory += sizeof(Tuple*) * (new_capacity - store->capacity);
                store->capacity = new_capacity;
                store->tuples = new_tuples;
            }
        } else {
            // 正常扩容

            Tuple **new_tuples = (Tuple**)realloc(store->tuples, sizeof(Tuple*) * new_capacity);
            if (!new_tuples) {
                tuplestore_error("无法为元组数组重新分配内存");
                return TUPLESTORE_ERROR_MEMORY;
            }
            
            // 更新内存使用和容量
            store->current_memory += sizeof(Tuple*) * (new_capacity - store->capacity);
            store->capacity = new_capacity;
            store->tuples = new_tuples;
        }
    }
    
    // 分配新元组
    Tuple *tuple = tuplestore_alloc_tuple();
    if (!tuple) {
        return TUPLESTORE_ERROR_MEMORY;
    }
    
    // 设置元组数据
    tuple->id = id;
    strncpy(tuple->data, data, sizeof(tuple->data) - 1);
    tuple->data[sizeof(tuple->data) - 1] = '\0';
    
    // 如果已经在使用文件存储，添加到缓冲区
    if (store->using_file) {
        // 如果缓冲区不在写入模式，切换为写入模式
        if (store->buffer_write_mode != BUFFER_MODE_WRITE) {
            // 当使用连续内存块时，只需要重置计数器
            store->buffer_count = 0;
            store->buffer_write_mode = BUFFER_MODE_WRITE;
        }
        
        // 如果缓冲区已满，刷新到文件
        if (store->buffer_count >= store->buffer_size) {
            int result = tuplestore_flush_buffer(store);
            if (result != TUPLESTORE_SUCCESS) {
                free(tuple);
                return result;
            }
        }
        
        // 复制元组到缓冲区的连续内存块中
        Tuple *dest = &(store->buffer[store->buffer_count]);
        *dest = *tuple;
        store->buffer_count++;
        free(tuple);  // 释放原始元组，因为已经复制到缓冲区
        // store->current_memory += tuple_size;
        
        // 如果缓冲区达到刷新阈值，则刷新到文件
        if (store->buffer_count >= (int)(store->buffer_size * TUPLESTORE_FLUSH_THRESHOLD)) {
            tuplestore_flush_buffer(store);
        }
    } else {
        // 添加到内存数组
        store->tuples[store->count++] = tuple;
        // store->current_memory += tuple_size;
    }
    
    return TUPLESTORE_SUCCESS;
}

/* 从文件中读取数据到缓冲区 */
int tuplestore_fill_buffer(TupleStore *store) {
    // 参数检查
    if (!store) {
        tuplestore_error("无效的TupleStore指针");
        return TUPLESTORE_ERROR_INVALID_PARAM;
    }
    
    // 检查是否可以从文件读取
    if (!store->using_file || !store->temp_file) {
        tuplestore_error("没有可用的文件进行读取");
        return TUPLESTORE_ERROR_INVALID_PARAM;
    }
    
    // 如果缓冲区处于写入模式，先刷新到文件
    if (store->buffer_write_mode == BUFFER_MODE_WRITE) {
        if (store->buffer_count > 0) {
            int result = tuplestore_flush_buffer(store);
            if (result != TUPLESTORE_SUCCESS) {
                return result;
            }
            // 刷新后已经切换到读取模式
        } else {
            // 如果缓冲区处于写入模式但没有数据，直接切换到读取模式
            store->buffer_write_mode = BUFFER_MODE_READ;
        }
    }
    
    // 计算要读取的起始位置
    store->buffer_start = store->read_pos;
    
    // 检查读取位置是否有效
    if (store->buffer_start >= store->file_count) {
        // 已超出文件范围，没有数据可读
        return 0;
    }
    
    // 定位到正确的文件位置
    long offset = (long)store->buffer_start * sizeof(Tuple);
    if (fseek(store->temp_file, offset, SEEK_SET) != 0) {
        tuplestore_error("无法定位到缓冲区起始位置");
        return TUPLESTORE_ERROR_IO;
    }
    
    // 重置缓冲区计数
    // 注意：使用连续内存块后，不需要释放单个元组
    store->buffer_count = 0;
    
    // 计算要读取的元组数量
    int to_read = store->buffer_size;
    if (store->buffer_start + to_read > store->file_count) {
        to_read = store->file_count - store->buffer_start;
    }
    
    // 重置缓冲区计数
    store->buffer_count = 0;
    
    // 直接将数据读取到连续内存块中
    size_t tuples_read = fread(store->buffer, sizeof(Tuple), to_read, store->temp_file);
    
    if (tuples_read > 0) {
        // 更新缓冲区计数
        store->buffer_count = tuples_read;
        
        // 内存使用量已经在创建时计算，这里不需要再次计算
    } else if (ferror(store->temp_file)) {
        // 读取错误
        tuplestore_error("从文件读取元组失败");
        return TUPLESTORE_ERROR_IO;
    }
    
    return store->buffer_count;
}

/* 从存储中获取下一个元组 */
Tuple* tuplestore_get_next(TupleStore *store) {
    // 参数检查
    if (!store) {
        tuplestore_error("无效的TupleStore指针");
        return NULL;
    }
    
    if (store->using_file) {
        // 从文件读取
        if (store->read_pos >= store->file_count) {
            return NULL;  // 已读取完所有元组
        }
        
        // 检查当前元组是否在缓冲区中
        int buffer_index = store->read_pos - store->buffer_start;
        
        // 如果元组不在缓冲区中，需要重新填充缓冲区
        if (buffer_index < 0 || buffer_index >= store->buffer_count) {
            int result = tuplestore_fill_buffer(store);
            if (result <= 0) {  // 返回值小于等于0表示错误或没有数据
                // 填充缓冲区失败或没有数据
                return NULL;
            }
            buffer_index = 0; // 现在读取的是缓冲区的第一个元组
        }
        
        // 从缓冲区中获取元组
        // 直接从连续内存块中获取元组
        Tuple *result = tuplestore_alloc_tuple();
        if (!result) {
            tuplestore_error("无法为结果元组分配内存");
            store->read_pos++;
            return NULL;
        }
        
        // 复制元组数据
        Tuple *src = &(store->buffer[buffer_index]);
        *result = *src;
        
        // 移动读取位置
        store->read_pos++;
        
        return result;
    } else {
        // 从内存读取
        if (store->read_pos >= store->count) {
            return NULL;  // 已读取完所有元组
        }
        
        Tuple *mem_tuple = store->tuples[store->read_pos];
        if (!mem_tuple) {
            store->read_pos++;
            return NULL;
        }
        
        // 创建返回元组的副本，保持一致的接口行为
        // 这样无论是从文件还是内存读取，调用者都需要释放返回的元组
        Tuple *result = tuplestore_alloc_tuple();
        if (!result) {
            tuplestore_error("无法为结果元组分配内存");
            store->read_pos++;
            return NULL;
        }
        
        // 复制元组数据
        *result = *mem_tuple;
        
        // 移动读取位置
        store->read_pos++;
        
        return result;
    }
}

/* 重置读取位置 */
int tuplestore_rescan(TupleStore *store) {
    // 参数检查
    if (!store) {
        tuplestore_error("无效的TupleStore指针");
        return TUPLESTORE_ERROR_INVALID_PARAM;
    }
    
    // 如果缓冲区处于写入模式且有数据，先刷新
    if (store->using_file && store->buffer_write_mode == BUFFER_MODE_WRITE && store->buffer_count > 0) {
        int result = tuplestore_flush_buffer(store);
        if (result != TUPLESTORE_SUCCESS) {
            tuplestore_error("重置前刷新缓冲区失败");
            return result;
        }
    }
    
    // 重置读取位置
    store->read_pos = 0;
    
    // 如果使用文件，重置文件指针
    if (store->using_file && store->temp_file) {
        if (fseek(store->temp_file, 0, SEEK_SET) != 0) {
            tuplestore_error("重置文件指针失败");
            return TUPLESTORE_ERROR_IO;
        }
    }
    
    return TUPLESTORE_SUCCESS;
}

/* 释放元组存储 */
int tuplestore_free(TupleStore *store) {
    // 参数检查
    if (!store) {
        tuplestore_error("无效的TupleStore指针");
        return TUPLESTORE_ERROR_INVALID_PARAM;
    }
    
    int error_occurred = 0;
    
    // 如果缓冲区处于写入模式且有数据，尝试刷新到文件
    if (store->using_file && store->buffer_write_mode == BUFFER_MODE_WRITE && store->buffer_count > 0) {
        if (tuplestore_flush_buffer(store) != TUPLESTORE_SUCCESS) {
            tuplestore_error("释放前刷新缓冲区失败");
            error_occurred = 1;
            // 继续释放内存，即使刷新失败
        }
    }
    
    // 释放内存中的元组
    if (store->tuples) {
        for (int i = 0; i < store->count; i++) {
            if (store->tuples[i]) {
                free(store->tuples[i]);
                store->tuples[i] = NULL;
            }
        }
        free(store->tuples);
        store->tuples = NULL;
        store->count = 0;
        store->capacity = 0;
    }
    
    // 释放缓冲区中的元组
    if (store->buffer) {
        // 释放缓冲区连续内存块
        free(store->buffer);
        store->buffer = NULL;
        store->buffer_count = 0;
        store->buffer_size = 0;
    }
    
    // 关闭临时文件
    if (store->temp_file) {
        if (fclose(store->temp_file) != 0) {
            tuplestore_error("关闭临时文件失败");
            error_occurred = 1;
        }
        store->temp_file = NULL;
    }
    
    // 删除临时文件
    if (store->filename) {
        if (unlink(store->filename) != 0 && errno != ENOENT) {
            // ENOENT表示文件不存在，这不是错误
            tuplestore_error("删除临时文件失败");
            error_occurred = 1;
        }
        free(store->filename);
        store->filename = NULL;
    }
    
    // 重置内存计数器
    store->current_memory = 0;
    store->max_memory_kb = 0;
    
    // 释放存储结构
    free(store);
    
    return error_occurred ? TUPLESTORE_ERROR_CLEANUP : TUPLESTORE_SUCCESS;
}

/**
 * 示例主函数 - 展示如何使用TupleStore
 */
int main() {
    printf("===== TupleStore 示例 =====\n\n");
    
    // 创建一个TupleStore，内存限制为6KB
    TupleStore *store = tuplestore_create(6);
    if (!store) {
        printf("创建TupleStore失败\n");
        return 1;
    }
    
    printf("添加元组到TupleStore...\n");
    // 添加一些元组
    for (int i = 0; i < 1000; i++) {
        char data[100];
        snprintf(data, sizeof(data), "这是元组数据 #%d", i);
        int result = tuplestore_put(store, i, data);
        if (result != TUPLESTORE_SUCCESS) {
            printf("添加元组失败，错误代码: %d\n", result);
            tuplestore_free(store);
            return 1;
        }
    }
    
    printf("\n读取所有元组...\n");
    // 读取所有元组
    int rescan_result = tuplestore_rescan(store);
    if (rescan_result != TUPLESTORE_SUCCESS) {
        printf("重置读取位置失败，错误代码: %d\n", rescan_result);
        tuplestore_free(store);
        return 1;
    }
    
    Tuple *tuple;
    int count = 0;
    while ((tuple = tuplestore_get_next(store)) != NULL) {
        printf("元组 #%d: id=%d, data=%s\n", count++, tuple->id, tuple->data);
        
        // 现在不管是从文件还是内存读取，都需要释放元组
        free(tuple);
    }
    
    printf("\n总共读取了 %d 个元组\n", count);
    printf("当前内存使用: %.2f KB\n", store->current_memory / 1024.0);
    printf("是否使用文件: %s\n", store->using_file ? "是" : "否");
    
    // 释放资源
    int free_result = tuplestore_free(store);
    if (free_result != TUPLESTORE_SUCCESS) {
        printf("警告: 释放 TupleStore 时发生错误，错误代码: %d\n", free_result);
    }
    
    printf("\n===== 示例完成 =====\n");
    return 0;
}
