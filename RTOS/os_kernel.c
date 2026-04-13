/**
 * @file os_kernel.c
 * @author Yukikaze
 * @brief RTOS 内核启动入口实现文件。
 * @version 0.1
 * @date 2026-03-26
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件负责把调度器选出的首任务正式交给 port 层切入运行。
 */

#include "os.h"
#include "internal/os_task_internal.h"
#include "os_port.h"

/**
 * @brief 启动内核并切入首个任务。
 *
 * @param cpu_clock_hz 当前 CPU 内核时钟频率，单位为 Hz。
 *
 * @return os_status_t 若当前没有 runnable task，则返回 OS_STATUS_EMPTY；
 *                     若成功完成首任务切入，本函数不应返回；
 *                     若意外返回，则返回具体错误码。
 */
os_status_t os_kernel_start(uint32_t cpu_clock_hz)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t    primask = 0U;

    /* 先让调度器选出当前应当运行的首任务。 */
    status = task_schedule();
    if ((status == OS_STATUS_EMPTY) || (status == OS_STATUS_NOT_INITIALIZED))
    {
        /* fresh boot 且还没有创建任何任务时，调度器全局状态可能尚未建立。
         * 对启动接口来说，这和“当前没有 runnable task”是同一类结果。 */
        return OS_STATUS_EMPTY;
    }

    /* 启动入口只接受“已有首任务待切入”这一种正常状态。 */
    if (status != OS_STATUS_SWITCH_REQUIRED)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 在真正切入首任务前，先关中断并配置 SysTick，
     * 避免节拍中断早于首任务启动链路生效。 */
    primask = os_port_enter_critical();

    status = os_port_systick_init(cpu_clock_hz, OS_TICK_HZ);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 真正切入首任务的动作交给 port 层完成。 */
    os_port_start_first_task();

    os_port_exit_critical(primask);

    /* 若 port 层意外返回，说明首任务启动链路存在异常。 */
    return OS_STATUS_INVALID_STATE;
}
