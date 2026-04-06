/**
 * @file system_stm32f103c8t6.h
 * @author sisanwu12
 * @brief
 * @version 0.1
 * @date 2026-04-04
 *
 */

#ifndef SYSTEM_STM32F103C8T6_H
#define SYSTEM_STM32F103C8T6_H

#include "data_type.h"

#define HSE_VALUE   ((u32)8000000) // 外部晶振频率，单位为 Hz
#define HSI_VALUE   ((u32)8000000) // 内部振荡器频率，单位为 Hz
#define LSI_VALUE   ((u32)40000)   // 内部低速振荡器频率，单位为 Hz
#define LSE_VALUE   ((u32)32768)   // 外部低速振荡器频率，单位为 Hz
#define SYSCLK_FREQ HSE_VALUE      // 系统时钟频率，单位为 Hz

typedef enum
{
    SYSTEM_INIT_SUCCESS = 0, // 系统初始化成功
    SYSTEM_INIT_TIMEOUT = 1, // 系统初始化超时
    SYSTEM_INIT_ERROR   = 2  // 系统初始化错误
} system_init_status_t;

typedef enum
{
    CLOCK_SOURCE_HSE = 0, // 使用外部晶振作为系统时钟源
    CLOCK_SOURCE_HSI = 1, // 使用内部振荡器作为系统时钟源
    CLOCK_SOURCE_LSE = 2, // 使用外部低速振荡器作为系统时钟源
    CLOCK_SOURCE_LSI = 3  // 使用内部低速振荡器作为系统时钟源
} system_clock_source_t;

/* ========== 外部接口 ==========*/

/* 系统初始化函数 */
system_init_status_t SystemInit();
/* 更新系统核心时钟源 */
system_clock_source_t SystemCoreClockUpdate(void);

#endif // SYSTEM_STM32F103C8T6_H