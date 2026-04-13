/**
 * @file os_kernel.h
 * @author Yukikaze
 * @brief RTOS 内核生命周期与全局时基接口定义文件。
 * @version 0.1
 * @date 2026-04-13
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件声明稳定的内核级 public API。
 *       这里承诺的是函数名、调用语义与头文件分层；
 *       不涉及任何对象字段布局承诺。
 */

#ifndef __OS_KERNEL_H__
#define __OS_KERNEL_H__

#include <stdint.h>
#include "os_types.h"

os_status_t os_kernel_start(uint32_t cpu_clock_hz);
os_tick_t os_kernel_tick_get(void);

/*
 * Compatibility API:
 * 旧名字保留一轮过渡兼容，新代码应优先使用 os_kernel_tick_get()。
 */
os_tick_t os_tick_get(void);

#endif /* __OS_KERNEL_H__ */
