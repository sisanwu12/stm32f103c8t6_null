/**
 * @file os_mutex.h
 * @author Yukikaze
 * @brief RTOS 互斥锁接口定义文件。
 * @version 0.1
 * @date 2026-04-12
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件声明当前阶段使用的 non-recursive mutex 公共接口。
 *       当前对象字段为了静态分配与过渡兼容而保持可见，
 *       但应用代码不应直接读写内部字段；
 *       字段布局不构成稳定 public contract，未来版本可能调整。
 */

#ifndef __OS_MUTEX_H__
#define __OS_MUTEX_H__

#include <stdint.h>
#include "os_list.h"
#include "os_types.h"

#define OS_MUTEX_MAGIC 0x4D545831UL // "MTX1"，用于识别合法互斥锁对象

typedef struct tcb tcb_t;

typedef struct os_mutex {
    uint32_t    magic;      // 互斥锁魔数，用于识别初始化状态
    tcb_t      *owner;      // 当前持锁任务；NULL 表示当前未被持有
    list_t      wait_list;  // 等待获取互斥锁的任务链表
    list_node_t owner_node; // 把当前 mutex 挂进 owner->owned_mutex_list 的链表节点
} os_mutex_t; // non-recursive mutex 对象定义

os_status_t os_mutex_init(os_mutex_t *mutex);
os_status_t os_mutex_lock(os_mutex_t *mutex, os_tick_t timeout_ticks);
os_status_t os_mutex_unlock(os_mutex_t *mutex);

#endif /* __OS_MUTEX_H__ */
