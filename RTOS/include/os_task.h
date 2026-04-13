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
#include "os_config.h"
#include "os_list.h"
#include "os_types.h"

#if (OS_MAX_PRIORITIES > 32U)
    #error "OS_MAX_PRIORITIES must be less than or equal to 32 when using a 32-bit ready bitmap."
#endif

#if (OS_MAX_PRIORITIES < 2U)
    #error "OS_MAX_PRIORITIES must be at least 2 when the lowest priority is reserved for the idle task."
#endif

#define OS_TASK_MAGIC 0x54434231UL // "TCB1"，用于识别合法任务控制块
#define OS_IDLE_TASK_PRIORITY ((uint8_t)(OS_MAX_PRIORITIES - 1U)) // idle 任务保留的最低优先级

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DELETED
} task_state_t; // 任务状态枚举定义

typedef enum {
    TASK_WAIT_RESULT_NONE = 0, // 当前等待路径没有附带额外结果
    TASK_WAIT_RESULT_TIMEOUT,  // 当前等待由超时路径唤醒
    TASK_WAIT_RESULT_OBJECT    // 当前等待由对象满足路径唤醒
} task_wait_result_t; // 任务等待结果枚举定义

typedef struct tcb {
    uint32_t *sp;              // 当前任务栈顶指针
    uint32_t *stack_base;      // 栈内存低地址起点
    uint32_t  stack_size;      // 栈深度，单位为uint32_t

    task_entry_t entry;        // 任务入口函数
    void       *param;         // 任务参数
    const char *name;          // 任务名称，仅用于调试和诊断

    uint32_t magic;            // 任务控制块魔数，用于识别初始化状态
    os_tick_t wake_tick;       // 延时或超时唤醒的目标tick
    void    *wait_obj;         // 当前正在等待的对象
    task_wait_result_t wait_result; // 当前等待恢复运行后携带的结果
    task_wait_cleanup_fn_t wait_cleanup_locked; // waiter 因 timeout/delete 离开时，供对象侧做清理的内部回调

    uint8_t      base_priority;     // 任务静态原始优先级
    uint8_t      priority;          // 当前生效优先级；调度与 waiter 排序都使用这个值
    task_state_t state;             // 任务状态
    uint8_t      time_slice;        // 当前剩余时间片
    uint8_t      time_slice_reload; // 时间片初值

    list_node_t sched_node;    // 调度相关链表节点，用于ready/timed-wait链表
    list_node_t event_node;    // 事件等待链表节点，用于queue/semaphore等待
    list_t      owned_mutex_list; // 当前任务持有的互斥锁链表，用于优先级继承重算
} tcb_t; // TCB结构定义

typedef struct task_init_config {
    task_entry_t entry;   // 任务入口函数
    void        *param;   // 任务入口参数
    uint32_t    *stack_base; // 任务栈内存低地址起点
    uint32_t     stack_size; // 栈深度，单位为uint32_t
    const char  *name;    // 任务名称
    uint8_t      priority; // 任务优先级，数值越小优先级越高
    uint8_t      time_slice; // 时间片长度，传 0 使用默认值
} task_init_config_t; // 任务初始化配置

os_status_t task_create(tcb_t *task, const task_init_config_t *config);
os_status_t task_delete(tcb_t *task);
os_status_t task_yield(void);
os_status_t task_delay(os_tick_t delay_ticks);
os_tick_t os_tick_get(void);
tcb_t *task_get_current(void);
os_status_t task_stack_high_water_mark_get(const tcb_t *task, uint32_t *unused_words);

#endif /* __OS_TASK_H__ */
