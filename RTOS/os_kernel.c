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
#include "os_port.h"

/**
 * @brief 启动内核并切入首个任务。
 *
 * @return os_status_t 若当前没有 runnable task，则返回 OS_STATUS_EMPTY；
 *                     若成功完成首任务切入，本函数不应返回；
 *                     若意外返回，则返回具体错误码。
 */
os_status_t os_kernel_start(void)
{
    os_status_t status = OS_STATUS_OK;

    /* 先让调度器选出当前应当运行的首任务。 */
    status = task_schedule();
    if (status == OS_STATUS_EMPTY)
    {
        return status;
    }

    /* 启动入口只接受“已有首任务待切入”这一种正常状态。 */
    if (status != OS_STATUS_SWITCH_REQUIRED)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 真正切入首任务的动作交给 port 层完成。 */
    os_port_start_first_task();

    /* 若 port 层意外返回，说明首任务启动链路存在异常。 */
    return OS_STATUS_INVALID_STATE;
}
