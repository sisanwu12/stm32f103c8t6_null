/**
 * @file os_types.h
 * @author Yukikaze
 * @brief RTOS 通用类型与宏定义文件。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件集中定义内核范围内使用的基础数据类型与状态码。
 */

#ifndef __OS_TYPES_H__
#define __OS_TYPES_H__

#include <stdint.h>

typedef uint32_t os_tick_t; // RTOS 节拍计数类型
typedef uint32_t os_stack_word_t; // 任务栈中单个栈元素的类型
typedef void (*task_entry_t)(void *param); // 任务入口函数类型定义
struct tcb;
typedef void (*task_wait_cleanup_fn_t)(struct tcb *task); // waiter 因 timeout/delete 离开时触发的对象侧内部清理回调

#define OS_WAIT_FOREVER ((os_tick_t)UINT32_MAX) // 永久等待标记，不为任务配置超时 tick

typedef enum {
    OS_STATUS_OK = 0,              // 操作成功
    OS_STATUS_SWITCH_REQUIRED,     // 已得出调度结果，调用方需要触发上下文切换
    OS_STATUS_NO_CHANGE,           // 已执行检查，但当前无需切换
    OS_STATUS_INVALID_PARAM,       // 参数非法
    OS_STATUS_INVALID_PRIORITY,    // 优先级非法
    OS_STATUS_INVALID_STACK,       // 栈参数非法
    OS_STATUS_INVALID_STATE,       // 当前对象状态不允许执行该操作
    OS_STATUS_ALREADY_INITIALIZED, // 对象已经完成初始化
    OS_STATUS_NOT_INITIALIZED,     // 模块尚未初始化
    OS_STATUS_EMPTY,               // 容器为空
    OS_STATUS_INSERT_FAILED,       // 插入或挂链操作失败
    OS_STATUS_TIMEOUT,             // 等待超时或“立即失败”语义下未能获得对象
    OS_STATUS_NOT_OWNER,           // 当前调用方并不是该互斥锁的 owner
    OS_STATUS_RECURSIVE_LOCK       // non-recursive mutex 上重复 lock 同一把锁
} os_status_t; // RTOS 状态码定义

#endif /* __OS_TYPES_H__ */
