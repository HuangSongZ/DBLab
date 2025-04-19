#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define BUFFER_SIZE 4096

// 模拟管道的环形缓冲区结构
typedef struct {
    char buffer[BUFFER_SIZE];  // 缓冲区
    int read_pos;              // 读指针位置
    int write_pos;             // 写指针位置
    int data_size;             // 当前数据量
} PipeBuffer;

// 初始化管道缓冲区
void init_pipe_buffer(PipeBuffer *pb) {
    pb->read_pos = 0;
    pb->write_pos = 0;
    pb->data_size = 0;
    memset(pb->buffer, 0, BUFFER_SIZE);
}

// 写入数据到管道
int pipe_write(PipeBuffer *pb, const char *data, int len) {
    int i;
    int bytes_written = 0;
    
    printf("尝试写入 %d 字节数据\n", len);
    
    // 检查可用空间
    if (pb->data_size >= BUFFER_SIZE) {
        printf("管道已满，写入被阻塞\n");
        return 0;  // 管道已满，阻塞
    }
    
    // 写入数据
    for (i = 0; i < len && pb->data_size < BUFFER_SIZE; i++) {
        pb->buffer[pb->write_pos] = data[i];
        pb->write_pos = (pb->write_pos + 1) % BUFFER_SIZE;  // 循环队列
        pb->data_size++;
        bytes_written++;
    }
    
    printf("成功写入 %d 字节数据，管道中现有 %d 字节\n", 
           bytes_written, pb->data_size);
    
    return bytes_written;
}

// 从管道读取数据
int pipe_read(PipeBuffer *pb, char *data, int max_len) {
    int i;
    int bytes_read = 0;
    
    printf("尝试读取最多 %d 字节数据\n", max_len);
    
    // 检查是否有数据可读
    if (pb->data_size == 0) {
        printf("管道为空，读取被阻塞\n");
        return 0;  // 管道为空，阻塞
    }
    
    // 读取数据
    for (i = 0; i < max_len && pb->data_size > 0; i++) {
        data[i] = pb->buffer[pb->read_pos];
        pb->read_pos = (pb->read_pos + 1) % BUFFER_SIZE;  // 循环队列
        pb->data_size--;
        bytes_read++;
    }
    
    data[bytes_read] = '\0';  // 确保字符串结束
    
    printf("成功读取 %d 字节数据，管道中剩余 %d 字节\n", 
           bytes_read, pb->data_size);
    
    return bytes_read;
}

// 打印管道状态
void print_pipe_status(PipeBuffer *pb) {
    printf("管道状态：读指针=%d, 写指针=%d, 数据量=%d/%d\n", 
           pb->read_pos, pb->write_pos, pb->data_size, BUFFER_SIZE);
}

// 模拟阻塞行为
void simulate_blocking() {
    PipeBuffer pipe;
    init_pipe_buffer(&pipe);
    
    printf("\n=== 模拟管道阻塞行为 ===\n\n");
    
    // 准备测试数据
    char write_buffer[BUFFER_SIZE * 2];
    char read_buffer[BUFFER_SIZE];
    
    for (int i = 0; i < BUFFER_SIZE * 2; i++) {
        write_buffer[i] = 'A' + (i % 26);
    }
    
    // 场景1：写满管道
    printf("场景1：写满管道\n");
    int total_written = 0;
    while (total_written < BUFFER_SIZE) {
        int chunk_size = 1024;  // 每次写入1KB
        int written = pipe_write(&pipe, write_buffer + total_written, chunk_size);
        total_written += written;
        print_pipe_status(&pipe);
        
        if (written < chunk_size) {
            printf("管道已满，无法继续写入\n");
            break;
        }
    }
    
    // 场景2：尝试写入已满的管道
    printf("\n场景2：尝试写入已满的管道\n");
    int result = pipe_write(&pipe, "这些数据将无法写入", 20);
    if (result == 0) {
        printf("写入被阻塞，实际写入了 %d 字节\n", result);
    }
    
    // 场景3：读取部分数据，然后继续写入
    printf("\n场景3：读取部分数据，然后继续写入\n");
    pipe_read(&pipe, read_buffer, 2048);
    print_pipe_status(&pipe);
    
    result = pipe_write(&pipe, "现在可以写入一些数据了", 30);
    printf("写入了 %d 字节\n", result);
    print_pipe_status(&pipe);
}

// 模拟实际管道的FIFO行为
void simulate_fifo() {
    int pipefd[2];
    pid_t pid;
    char buf[1024];
    
    printf("\n=== 模拟实际管道的FIFO行为 ===\n\n");
    
    // 创建管道
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    // 创建子进程
    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {  // 子进程：读取端
        close(pipefd[1]);  // 关闭写端
        
        // 循环读取数据
        for (int i = 0; i < 5; i++) {
            sleep(1);  // 故意延迟，模拟处理时间
            
            ssize_t numRead = read(pipefd[0], buf, sizeof(buf));
            if (numRead > 0) {
                buf[numRead] = '\0';
                printf("子进程读取: %s\n", buf);
            } else if (numRead == 0) {
                printf("子进程: 管道已关闭\n");
                break;
            } else {
                perror("read");
                exit(EXIT_FAILURE);
            }
        }
        
        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    } else {  // 父进程：写入端
        close(pipefd[0]);  // 关闭读端
        
        // 写入一些消息
        const char *messages[] = {
            "第一条消息",
            "第二条消息",
            "第三条消息",
            "第四条消息",
            "第五条消息"
        };
        
        for (int i = 0; i < 5; i++) {
            printf("父进程写入: %s\n", messages[i]);
            write(pipefd[1], messages[i], strlen(messages[i]) + 1);
            usleep(500000);  // 0.5秒延迟
        }
        
        close(pipefd[1]);
        wait(NULL);  // 等待子进程结束
    }
}

int main() {
    // 模拟管道的环形缓冲区和阻塞行为
    simulate_blocking();
    
    // 模拟实际管道的FIFO行为
    simulate_fifo();
    
    return 0;
}
