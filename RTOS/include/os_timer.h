/**
 * @file os_timer.h
 * @author Yukikaze
 * @brief RTOS 软件定时器接口定义文件。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件声明可选的软件定时器服务接口。
 *       当前对象字段为了静态分配与过渡兼容而保持可见，
 *       但应用代码不应直接读写内部字段；
 *       字段布局不构成稳定 public contract，未来版本可能调整。
 */

#ifndef __OS_TIMER_H__
#define __OS_TIMER_H__

#include <stdint.h>
#include "os_list.h"
#include "os_types.h"

#define OS_TIMER_MAGIC 0x54494D31UL // "TIM1"，用于识别合法软件定时器对象

typedef enum {
    OS_TIMER_ONE_SHOT = 0,
    OS_TIMER_PERIODIC
} os_timer_mode_t; // 软件定时器模式定义

typedef void (*os_timer_callback_t)(void *arg); // 软件定时器回调函数类型

typedef struct os_timer {
    uint32_t            magic;               // 定时器魔数，用于识别初始化状态
    const char         *name;                // 定时器名称，仅用于诊断
    os_timer_mode_t     mode;                // one-shot 或 periodic
    os_timer_callback_t callback;            // 到期后在线程态执行的回调
    void               *arg;                 // 传递给回调的用户参数
    os_tick_t           period_ticks;        // periodic 模式的周期；one-shot 为 0
    os_tick_t           expiry_tick;         // 当前已装填的到期 tick
    uint8_t             active;              // 非 0 表示当前已在 active list 中
    uint32_t            pending_expirations; // 尚未被 daemon 消费的到期次数
    list_node_t         active_node;         // active list 挂链节点
    list_node_t         expired_node;        // expired FIFO 挂链节点
} os_timer_t; // 静态软件定时器对象定义

typedef struct os_timer_config {
    const char         *name;     // 定时器名称
    os_timer_mode_t     mode;     // 定时器模式
    os_timer_callback_t callback; // 到期回调
    void               *arg;      // 回调参数
} os_timer_config_t; // 软件定时器初始化配置

os_status_t os_timer_init(os_timer_t *timer, const os_timer_config_t *config);
os_status_t os_timer_start(os_timer_t *timer, os_tick_t timeout_ticks);
os_status_t os_timer_stop(os_timer_t *timer);

#endif /* __OS_TIMER_H__ */
