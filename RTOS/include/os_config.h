/**
 * @file os_config.h
 * @author Yukikaze
 * @brief RTOS 编译期配置定义文件。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件用于存放内核范围内的配置宏与构建选项。
 */

#ifndef __OS_CONFIG_H__
#define __OS_CONFIG_H__

/* RTOS 支持的最大优先级数量。
 * 约定数值越小优先级越高，当前实现同时要求该值不超过 32（0-31）。 */
#ifndef OS_MAX_PRIORITIES
    #define OS_MAX_PRIORITIES 32U
#endif

/* RTOS 系统节拍频率，单位为 Hz。
 * 当前 delay/block/tick 闭环按 1000Hz（1ms 一个 tick）设计。 */
#ifndef OS_TICK_HZ
    #define OS_TICK_HZ 1000U
#endif

/* 任务默认时间片长度。
 * 当创建任务时未显式指定时间片，系统使用该值初始化 time_slice。 */
#ifndef OS_TASK_DEFAULT_TIME_SLICE
    #define OS_TASK_DEFAULT_TIME_SLICE 10U
#endif

/* 任务允许的最小栈深度，单位为 uint32_t。
 * 小于该值的任务栈不会通过创建参数检查。 */
#ifndef OS_TASK_MIN_STACK_DEPTH
    #define OS_TASK_MIN_STACK_DEPTH 32U
#endif

/* 任务栈地址对齐要求，单位为字节。
 * Cortex-M3 任务初始栈帧在建立时会按该值向下对齐。 */
#ifndef OS_TASK_STACK_ALIGNMENT_BYTES
    #define OS_TASK_STACK_ALIGNMENT_BYTES 8U
#endif

/* idle 任务栈深度，单位为 uint32_t。
 * idle 任务只做最低优先级兜底运行，因此默认使用最小栈深度即可。 */
#ifndef OS_IDLE_TASK_STACK_DEPTH
    #define OS_IDLE_TASK_STACK_DEPTH OS_TASK_MIN_STACK_DEPTH
#endif

#endif /* __OS_CONFIG_H__ */
