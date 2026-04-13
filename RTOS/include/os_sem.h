/**
 * @file os_sem.h
 * @author Yukikaze
 * @brief RTOS 二值信号量接口定义文件。
 * @version 0.1
 * @date 2026-04-12
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件声明当前阶段使用的二值信号量公共接口。
 *       当前对象字段为了静态分配与过渡兼容而保持可见，
 *       但应用代码不应直接读写内部字段；
 *       字段布局不构成稳定 public contract，未来版本可能调整。
 */

#ifndef __OS_SEM_H__
#define __OS_SEM_H__

#include <stdint.h>
#include "os_list.h"
#include "os_types.h"

#define OS_SEM_MAGIC 0x53454D31UL // "SEM1"，用于识别合法信号量对象

typedef struct os_sem {
    uint32_t magic;         // 信号量魔数，用于识别初始化状态
    uint32_t current_count; // 当前可用计数；二值语义下只允许 0 或 1
    uint32_t max_count;     // 最大计数；当前阶段固定为 1，内部仍按 counting core 存储
    list_t   wait_list;     // 对象等待链表，按“高优先级优先，同优先级 FIFO”组织
} os_sem_t; // 二值信号量对象定义

os_status_t os_sem_init(os_sem_t *sem, uint8_t initially_available);
os_status_t os_sem_take(os_sem_t *sem, os_tick_t timeout_ticks);
os_status_t os_sem_give(os_sem_t *sem);
os_status_t os_sem_give_from_isr(os_sem_t *sem);

#endif /* __OS_SEM_H__ */
