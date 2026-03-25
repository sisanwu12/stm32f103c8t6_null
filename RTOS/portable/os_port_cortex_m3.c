/**
 * @file os_port_cortex_m3.c
 * @author Yukikaze
 * @brief Cortex-M3 RTOS 专用移植层实现文件。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件用于放置 Cortex-M3 平台相关的上下文切换、异常入口与节拍处理代码。
 */

#include <stdint.h>
#include <stddef.h>
#include "os_port_cortex_m3.h"

#define OS_PORT_INITIAL_XPSR        0x01000000UL
#define OS_PORT_STACK_ALIGN_MASK    ((uintptr_t)(OS_TASK_STACK_ALIGNMENT_BYTES - 1U))

/**
 * @brief 当任务函数意外返回时进入死循环，便于调试定位错误任务。
 */
static void os_port_task_exit_error(void)
{
    while (1)
    {
    }
}

/**
 * @brief 按 Cortex-M3 的异常返回格式初始化任务初始栈帧。
 *
 * @param stack_base 任务栈内存低地址起点。
 * @param stack_size 栈深度，单位为 uint32_t。
 * @param entry 任务入口函数。
 * @param param 传递给任务入口函数的参数。
 *
 * @return uint32_t* 初始化完成后的任务栈顶指针；参数非法时返回 NULL。
 */
uint32_t *os_port_task_stack_init(uint32_t *stack_base, uint32_t stack_size, task_entry_t entry, void *param)
{
    uintptr_t  top_address = 0U;
    uint32_t  *sp          = NULL;

    if ((stack_base == NULL) || (entry == NULL) || (stack_size < OS_TASK_MIN_STACK_DEPTH))
    {
        return NULL;
    }

    top_address = (uintptr_t)(stack_base + stack_size);
    top_address &= ~OS_PORT_STACK_ALIGN_MASK;
    sp = (uint32_t *)top_address;

    *(--sp) = OS_PORT_INITIAL_XPSR;              // xPSR
    *(--sp) = ((uint32_t)entry & 0xFFFFFFFEUL);  // PC
    *(--sp) = (uint32_t)os_port_task_exit_error; // LR
    *(--sp) = 0x12121212UL;                      // R12
    *(--sp) = 0x03030303UL;                      // R3
    *(--sp) = 0x02020202UL;                      // R2
    *(--sp) = 0x01010101UL;                      // R1
    *(--sp) = (uint32_t)param;                   // R0

    *(--sp) = 0x11111111UL; // R11
    *(--sp) = 0x10101010UL; // R10
    *(--sp) = 0x09090909UL; // R9
    *(--sp) = 0x08080808UL; // R8
    *(--sp) = 0x07070707UL; // R7
    *(--sp) = 0x06060606UL; // R6
    *(--sp) = 0x05050505UL; // R5
    *(--sp) = 0x04040404UL; // R4

    return sp;
}
