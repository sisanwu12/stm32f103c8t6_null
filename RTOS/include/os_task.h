/**
 * @file os_task.h
 * @author Yukikaze
 * @brief RTOS 任务管理与调度器接口定义文件。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件声明任务控制、调度、延时与状态切换相关接口。
 */

#ifndef __OS_TASK_H__
#define __OS_TASK_H__

#include <stdint.h>
#include "os_list.h"

#ifndef OS_MAX_PRIORITIES
    #define OS_MAX_PRIORITIES 32U
#endif

#if (OS_MAX_PRIORITIES > 32U)
    #error "OS_MAX_PRIORITIES must be less than or equal to 32 when using a 32-bit ready bitmap."
#endif

typedef void (*task_entry_t)(void *param); // 任务入口函数类型定义，接受一个 void* 参数

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DELETED
} task_state_t; // 任务状态枚举定义

typedef struct tcb {
    uint32_t *sp;              // 当前任务栈顶指针
    uint32_t *stack_base;      // 栈起始地址
    uint32_t  stack_size;      // 栈大小

    task_entry_t entry;        // 任务入口函数
    void       *param;         // 任务参数

    uint32_t wake_tick;        // 延时或超时唤醒的目标tick
    void    *wait_obj;         // 当前正在等待的对象

    uint8_t      priority;          // 优先级
    task_state_t state;             // 任务状态
    uint8_t      time_slice;        // 当前剩余时间片
    uint8_t      time_slice_reload; // 时间片初值

    list_node_t sched_node;    // 调度相关链表节点，用于ready/sleep链表
    list_node_t event_node;    // 事件等待链表节点，用于queue/semaphore等待
} tcb_t; // TCB结构定义

typedef struct ready_queue {
    list_t   ready_lists[OS_MAX_PRIORITIES]; // 每个优先级一条就绪链表
    uint32_t ready_bitmap;                   // 就绪位图，bit置位表示该优先级非空
} ready_queue_t; // 调度器就绪队列

void ready_queue_init(ready_queue_t *queue);
void ready_queue_insert_tail(ready_queue_t *queue, tcb_t *task);
void ready_queue_remove(ready_queue_t *queue, tcb_t *task);
tcb_t *ready_queue_peek_highest(ready_queue_t *queue);
uint8_t ready_queue_get_highest_priority(const ready_queue_t *queue, uint8_t *priority);
void ready_queue_rotate(ready_queue_t *queue, uint8_t priority);
uint8_t ready_queue_is_empty(const ready_queue_t *queue);



#endif /* __OS_TASK_H__ */
