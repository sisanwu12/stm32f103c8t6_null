/**
 * @file os_port_cortex_m3.h
 * @author Yukikaze
 * @brief Cortex-M3 RTOS 专用移植层头文件定义。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件声明面向 Cortex-M3 内核的专用移植层宏、类型与底层接口。
 */

#ifndef __OS_PORT_CORTEX_M3_H__
#define __OS_PORT_CORTEX_M3_H__

#include <stdint.h>
#include "os_config.h"
#include "os_types.h"

uint32_t *os_port_task_stack_init(uint32_t *stack_base, uint32_t stack_size, task_entry_t entry, void *param);
void os_port_trigger_pendsv(void);
void os_port_start_first_task(void);
os_status_t os_port_systick_init(uint32_t cpu_clock_hz, uint32_t tick_hz);
uint32_t os_port_enter_critical(void);
void os_port_exit_critical(uint32_t primask);
uint8_t os_port_is_in_interrupt(void);

#endif /* __OS_PORT_CORTEX_M3_H__ */
