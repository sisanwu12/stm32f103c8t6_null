/**
 * @file os.h
 * @author Yukikaze
 * @brief RTOS 顶层公共头文件。
 * @version 0.1
 * @date 2026-03-26
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件用于汇总并对外暴露内核公共接口。
 */

#ifndef __OS_H__
#define __OS_H__

#include "os_task.h"

/**
 * @brief 启动内核并切入首个任务。
 *
 * @param cpu_clock_hz 当前 CPU 内核时钟频率，单位为 Hz。
 *
 * @return os_status_t 若当前没有 runnable task，则返回 OS_STATUS_EMPTY；
 *                     若成功完成首任务切入，本函数不应返回；
 *                     若意外返回，则返回具体错误码。
 */
os_status_t os_kernel_start(uint32_t cpu_clock_hz);

#endif /* __OS_H__ */
