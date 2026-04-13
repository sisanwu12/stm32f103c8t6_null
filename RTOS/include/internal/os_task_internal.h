/**
 * @file os_task_internal.h
 * @author Yukikaze
 * @brief RTOS 任务层内部接口定义文件。
 * @version 0.1
 * @date 2026-04-13
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件只给内核内部模块使用，不对应用代码公开。
 */

#ifndef __OS_TASK_INTERNAL_H__
#define __OS_TASK_INTERNAL_H__

#include "os_task.h"

typedef struct ready_queue {
    list_t   ready_lists[OS_MAX_PRIORITIES]; // 每个优先级一条可运行链表
    uint32_t ready_bitmap;                   // 可运行位图，bit置位表示该优先级非空
} ready_queue_t; // 调度器可运行任务集合

os_status_t task_system_init(void);
os_status_t task_schedule(void);
os_status_t task_system_tick(void);
tcb_t *task_get_next(void);
ready_queue_t *task_get_ready_queue(void);
os_status_t task_set_current(tcb_t *task);

os_status_t task_block_current(void *wait_obj, os_tick_t timeout_ticks, task_wait_cleanup_fn_t wait_cleanup_locked);
os_status_t task_unblock(tcb_t *task, task_wait_result_t wait_result);
void task_exit_current(void);
tcb_t *task_wait_list_peek_head_task(const list_t *wait_list);
os_status_t task_effective_priority_update_locked(tcb_t *task, uint8_t new_priority);
os_status_t task_priority_inheritance_raise_locked(tcb_t *task, uint8_t inherited_priority);
os_status_t task_priority_inheritance_refresh_locked(tcb_t *task);
os_status_t task_wait_list_insert_priority_ordered(list_t *wait_list, tcb_t *task);
void task_wait_list_remove_task(list_t *wait_list, tcb_t *task);

void ready_queue_init(ready_queue_t *queue);
void ready_queue_insert_tail(ready_queue_t *queue, tcb_t *task);
void ready_queue_remove(ready_queue_t *queue, tcb_t *task);
const tcb_t *ready_queue_peek_highest(const ready_queue_t *queue);
uint8_t ready_queue_get_highest_priority(const ready_queue_t *queue, uint8_t *priority);
void ready_queue_rotate(ready_queue_t *queue, uint8_t priority);
uint8_t ready_queue_is_empty(const ready_queue_t *queue);

#endif /* __OS_TASK_INTERNAL_H__ */
