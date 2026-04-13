/**
 * @file os_timer_internal.h
 * @author Yukikaze
 * @brief RTOS 软件定时器内部接口定义文件。
 * @version 0.1
 * @date 2026-04-13
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件只给内核内部使用，不对应用代码公开。
 */

#ifndef __OS_TIMER_INTERNAL_H__
#define __OS_TIMER_INTERNAL_H__

#include "os_timer.h"

os_status_t os_timer_system_tick(void);

#endif /* __OS_TIMER_INTERNAL_H__ */
