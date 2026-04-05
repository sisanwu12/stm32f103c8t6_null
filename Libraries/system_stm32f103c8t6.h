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

u32 SystemInit();
u32 SystemCoreClockUpdate(void);

#endif // SYSTEM_STM32F103C8T6_H