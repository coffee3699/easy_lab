#include "uthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct uthread *current_thread = NULL;
static struct uthread *main_thread = NULL;
static Queue *scheduling_queue = NULL;

/// @brief 切换上下文
/// @param from 当前上下文
/// @param to 要切换到的上下文
extern void thread_switch(struct context *from, struct context *to);

/// @brief 线程的入口函数
/// @param tcb 线程的控制块
/// @param thread_func 线程的执行函数
/// @param arg 线程的参数
void _uthread_entry(struct uthread *tcb, void (*thread_func)(void *),
                    void *arg);

/// @brief 清空上下文结构体
/// @param context 上下文结构体指针
static inline void make_dummpy_context(struct context *context) {
  memset((struct context *)context, 0, sizeof(struct context));
}

struct uthread *uthread_create(void (*func)(void *), void *arg,const char* thread_name) {
  struct uthread *uthread = NULL;
  int ret;

  // 申请一块16字节对齐的内存
  ret = posix_memalign((void **)&uthread, 16, sizeof(struct uthread));
  if (0 != ret) {
    printf("error");
    exit(-1);
  }

  //         +------------------------+
  // low     |                        |
  //         |                        |
  //         |                        |
  //         |         stack          |
  //         |                        |
  //         |                        |
  //         |                        |
  //         +------------------------+
  //  high   |    fake return addr    |
  //         +------------------------+

  /*
  TODO: 在这里初始化uthread结构体。可能包括设置rip,rsp等寄存器。入口地址需要是函数_uthread_entry.
        除此以外，还需要设置uthread上的一些状态，保存参数等等。
        
        你需要注意rsp寄存器在这里要8字节对齐，否则后面从context switch进入其他函数的时候会有rsp寄存器
        不对齐的情况（表现为在printf里面Segment Fault）
  */
  // Initialize the context structure
  memset(uthread, 0, sizeof(struct uthread));
  make_dummpy_context(&uthread->context);

  // Initialize & set up the stack
  uthread->context.rdi = (long long)uthread;      // First argument
  uthread->context.rsi = (long long)func;         // Second argument
  uthread->context.rdx = (long long)arg;          // Third argument
  uthread->context.rip = (long long)_uthread_entry;

  uthread->context.rsp = ((long long)uthread->stack + STACK_SIZE) & -16L;

  uthread->context.rsp -= 8;

  // Set the status of the thread
  uthread->state = THREAD_INIT;

  // Set the argument of the thread
  uthread->arg = arg;

  // Set the thead name
  uthread->name = thread_name;

  // Enqueue the thread
  enqueue(scheduling_queue, uthread);

  return uthread;
}


void schedule() {
  /*
  TODO: 在这里写调度子线程的机制。这里需要实现一个FIFO队列。这意味着你需要一个额外的队列来保存目前活跃
        的线程。一个基本的思路是，从队列中取出线程，然后使用resume恢复函数上下文。重复这一过程。
  */
  struct uthread *next_thread = NULL;

  // Dequeue the next thread to run (if the next thread's state is THREAD_STOP,
  // destroy it, then dequeue again)
  do {
    if (isQueueEmpty(scheduling_queue)) {
      exit(0);
    }
    next_thread = dequeue(scheduling_queue);
    // printf("Dequeued thread %d.\n", (int)(intptr_t)next_thread->arg);
    if (next_thread->state == THREAD_STOP) {
      // printf("schedule--->Destroying thread %d.\n", (int)(intptr_t)next_thread->arg);
      thread_destroy(next_thread);
      continue;
    } else {
      // printf("Switching to thread %d.\n", (int)(intptr_t)next_thread->arg);
      break; // Break out of the loop if the next thread is valid and not in the
             // THREAD_STOP state
    }
  } while (1);

  // printf("Schedule--->Current thread is thread %d.\n", (int)(intptr_t)current_thread->arg);
  // Use uthread_resume to resume the next thread
  uthread_resume(next_thread);
}

long long uthread_yield() {
  /*
  TODO: 用户态线程让出控制权到调度器。由正在执行的用户态函数来调用。记得调整tcb状态。
  */
  // printf("Suspending (current) thread %d.\n", (int)(intptr_t)current_thread->arg);
  // Set the current thread's state to suspended
  current_thread->state = THREAD_SUSPENDED;

  // Enqueue the current thread
  enqueue(scheduling_queue, current_thread);

  // Call the scheduler to switch to another thread
  schedule();

  // The scheduler will switch to another thread, and when this thread is
  // resumed, it will continue from here
  return 0;
}

void uthread_resume(struct uthread *tcb) {
  /*
  TODO：调度器恢复到一个函数的上下文。
  */
  // printf("Resuming thread %d.\n", (int)(intptr_t)tcb->arg);
  
  // Set the next thread's state to running
  tcb->state = THREAD_RUNNING;

  struct uthread* prviou_thread = current_thread;

  // Set the current thread to the next thread (tcb)
  current_thread = tcb;
  // printf("Resume--->Previous thread is thread %d.\n", (int)(intptr_t)prviou_thread->arg);
  // printf("Resume--->Current thread is thread %d.\n", (int)(intptr_t)current_thread->arg);
  // Call the scheduler to switch to the next thread
  thread_switch(&prviou_thread->context, &current_thread->context);


  // printf("Current thread has been set to thread %d.\n", (int)(intptr_t)current_thread->arg);
}

void thread_destroy(struct uthread *tcb) {
  free(tcb);
}

void _uthread_entry(struct uthread *tcb, void (*thread_func)(void *),
                    void *arg) {
  /*
  TODO: 这是所有用户态线程函数开始执行的入口。在这个函数中，你需要传参数给真正调用的函数，然后设置tcb的状态。
  */

  // Set the status of the thread
  tcb->state = THREAD_RUNNING;

  // Call the user-defined function with the argument
  thread_func(arg);

  // Set the status of the thread (after the user-defined function returns)
  tcb->state = THREAD_STOP;
  // printf("Thread %d stopped.\n", (int)(intptr_t)tcb->arg);

  // Enqueue the thread (for the scheduler to destroy it)
  enqueue(scheduling_queue, tcb);

  // Yield to the scheduler
  schedule();
}

void init_uthreads() {
  main_thread = malloc(sizeof(struct uthread));
  make_dummpy_context(&main_thread->context);

  // Set the current thread to the main thread
  current_thread = main_thread;

  // Initialize the queue for thread scheduling
  scheduling_queue = createQueue();
}

Queue* createQueue() {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    if (queue == NULL) {
        fprintf(stderr, "Memory allocation error.\n");
        exit(1);
    }
    queue->front = queue->rear = NULL;
    return queue;
}

void enqueue(Queue* queue, struct uthread* data) {
    QueueNode* newNode = (QueueNode*)malloc(sizeof(QueueNode));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation error.\n");
        exit(1);
    }
    newNode->data = data;
    newNode->next = NULL;
    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
        return;
    }
    queue->rear->next = newNode;
    queue->rear = newNode;
}

struct uthread* dequeue(Queue* queue) {
    if (isQueueEmpty(queue)) {
        fprintf(stderr, "Queue is empty. Cannot dequeue.\n");
        exit(1);
    }
    struct uthread* data = queue->front->data;
    QueueNode* temp = queue->front;
    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    free(temp);
    return data;
}

int isQueueEmpty(Queue* queue) {
    return queue->front == NULL;
}