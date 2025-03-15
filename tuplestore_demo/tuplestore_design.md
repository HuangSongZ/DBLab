# TupleStore 设计文档

## 1. 概述

TupleStore是一个轻量级的元组存储系统，用于临时存储数据元组。它支持在内存中存储元组，并在内存不足时自动溢出到磁盘。这种设计使其特别适合于处理大量数据的场景，如数据库系统中的中间结果存储、排序和连接操作等。

## 2. 主要特性

- **内存和磁盘混合存储**：根据配置的内存限制，自动决定是否将数据溢出到磁盘
- **透明的数据访问**：无论数据存储在内存还是磁盘，都提供统一的访问接口
- **高效的读取**：使用缓冲区机制减少磁盘I/O操作
- **支持重新扫描**：可以重置读取位置，多次遍历存储的元组
- **资源自动管理**：在不再需要时自动释放内存和临时文件

## 3. 数据结构

### 3.1 元组结构 (Tuple)

```c
typedef struct {
    int id;
    char data[100];
} Tuple;
```

这是一个简单的元组结构，包含一个整数ID和一个固定长度的数据字段。在实际应用中，可以根据需要扩展此结构。

### 3.2 元组存储结构 (TupleStore)

```c
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
    
    /* 缓冲区相关 */
    Tuple **buffer;      /* 文件读取缓冲区 */
    int buffer_size;     /* 缓冲区大小（元组数） */
    int buffer_start;    /* 缓冲区中第一个元组在文件中的位置 */
    int buffer_count;    /* 缓冲区中当前的元组数量 */
} TupleStore;
```

TupleStore结构包含了管理元组存储所需的所有信息，包括内存管理、文件操作和缓冲区控制。

## 4. 核心功能

### 4.1 创建元组存储 (tuplestore_create)

```c
TupleStore* tuplestore_create(int max_memory_kb);
```

创建一个新的元组存储，指定最大内存使用限制（KB）。此函数会初始化内存结构并设置缓冲区。

### 4.2 添加元组 (tuplestore_put)

```c
int tuplestore_put(TupleStore *store, int id, const char *data);
```

向存储中添加一个新元组。如果内存不足，会自动将现有元组溢出到磁盘。

### 4.3 获取下一个元组 (tuplestore_get_next)

```c
Tuple* tuplestore_get_next(TupleStore *store);
```

获取下一个元组。此函数会根据当前状态从内存或磁盘读取元组。如果从磁盘读取，会使用缓冲区减少I/O操作。

### 4.4 重置读取位置 (tuplestore_rescan)

```c
void tuplestore_rescan(TupleStore *store);
```

重置读取位置到开始，允许重新遍历所有元组。

### 4.5 释放元组存储 (tuplestore_free)

```c
void tuplestore_free(TupleStore *store);
```

释放元组存储使用的所有资源，包括内存和临时文件。

## 5. 内部实现细节

### 5.1 内存管理

TupleStore使用一个简单的内存跟踪机制，记录当前使用的内存量。当添加新元组时，如果内存使用超过限制，会触发将现有元组转储到磁盘的操作。

### 5.2 磁盘存储

当内存不足时，TupleStore会创建一个临时文件并将所有内存中的元组写入该文件。之后，新的元组会直接写入文件，而不是存储在内存中。

### 5.3 缓冲区机制

为了提高从磁盘读取元组的效率，TupleStore实现了一个缓冲区机制。它一次从磁盘读取多个元组到缓冲区，减少磁盘I/O操作的次数。

### 5.4 元组拷贝

从TupleStore获取元组时，根据存储位置有不同的行为：
- 从内存读取：返回元组的指针，不创建副本
- 从磁盘读取：创建元组的副本并返回，调用者需要负责释放内存

## 6. 使用示例

```c
// 创建一个TupleStore，内存限制为1MB
TupleStore *store = tuplestore_create(1024);

// 添加元组
for (int i = 0; i < 1000; i++) {
    char data[100];
    snprintf(data, sizeof(data), "这是元组数据 #%d", i);
    tuplestore_put(store, i, data);
}

// 读取所有元组
tuplestore_rescan(store);
Tuple *tuple;
while ((tuple = tuplestore_get_next(store)) != NULL) {
    printf("元组: id=%d, data=%s\n", tuple->id, tuple->data);
    
    // 如果是从文件读取的元组，需要释放
    if (store->using_file) {
        free(tuple);
    }
}

// 释放资源
tuplestore_free(store);
```

## 7. 性能考虑

### 7.1 内存使用

TupleStore的内存使用由`max_memory_kb`参数控制。较大的内存限制可以减少磁盘I/O，但会增加内存压力。

### 7.2 缓冲区大小

缓冲区大小会影响从磁盘读取数据的效率。较大的缓冲区可以减少磁盘I/O次数，但会占用更多内存。当前实现使用一半的可用内存作为缓冲区。

### 7.3 临时文件管理

TupleStore使用标准C库的文件操作函数管理临时文件。在高并发环境中，可能需要考虑更复杂的文件管理策略。

## 8. 潜在改进

1. **支持不同类型的元组**：当前实现假设所有元组具有相同的结构，可以扩展为支持变长或不同类型的元组
2. **并行处理**：添加对并行读写的支持
3. **压缩存储**：为磁盘存储添加压缩功能，减少I/O和存储需求
4. **索引支持**：添加简单的索引结构，支持按键查找元组
5. **内存策略优化**：实现更复杂的内存管理策略，如LRU缓存

## 9. 结论

TupleStore提供了一个简单而高效的解决方案，用于临时存储和访问大量元组数据。它的混合存储策略使其适用于各种数据处理场景，特别是在处理超出可用内存的大型数据集时。
