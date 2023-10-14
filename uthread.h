#ifndef UTHREAD_H
#define UTHREAD_H

#include <stddef.h>
#define STACK_SIZE 4096

/// @brief 线程的状态
/// @param THREAD_INIT 初始化
/// @param THREAD_RUNNING 运行
/// @param THREAD_STOP 停止
/// @param THREAD_SUSPENDED 挂起
enum thread_state {
  THREAD_INIT,
  THREAD_RUNNING,
  THREAD_STOP,
  THREAD_SUSPENDED,
};

/// @brief 线程的上下文
struct context {
  long long rip, rsp, rbp, rbx, r12, r13, r14, r15;
  long long rdi, rsi, rdx;
};

/// @brief 线程的控制块
/// @param stack 栈
/// @param context 上下文
/// @param state 状态
/// @param name 名字
struct uthread {
  char stack[STACK_SIZE];
  struct context context;
  enum thread_state state;
  const char *name;
  void *arg;
};

/// @brief 初始化系统，在main函数开始时调用
void init_uthreads();

/// @brief 主线程陷入调度器，阻塞
void schedule();

/// @brief 创建一个线程
/// @param func 线程的执行函数
/// @param arg 线程的参数
/// @param thread_name 线程的名字
struct uthread *uthread_create(void (*func)(void *), void *arg,const char* thread_name);

/// @brief 恢复线程
/// @param 线程的控制块
void uthread_resume(struct uthread *tcb);

/// @brief 线程主动让出
long long uthread_yield();

/// @brief 销毁线程的结构体
/// @param tcb 线程的控制块q
void thread_destroy(struct uthread *tcb);

// Define the node
typedef struct QueueNode {
    struct uthread* data;
    struct QueueNode* next;
} QueueNode;

// Define the queue
typedef struct {
    QueueNode* front;
    QueueNode* rear;
} Queue;

/// @brief Create an empty queue
/// @return The pointer to the queue
Queue* createQueue();

/// @brief Enqueue an element to the queue
/// @param queue The pointer to the queue
/// @param data The data to be enqueued
void enqueue(Queue* queue, struct uthread *data);

/// @brief Dequeue an element from the queue
/// @param queue The pointer to the queue
/// @return The data dequeued from the queue
struct uthread* dequeue(Queue* queue);

/// @brief Check if the queue is empty
/// @param queue The pointer to the queue
/// @return 1 if the queue is empty, 0 otherwise
int isQueueEmpty(Queue* queue);

#endif