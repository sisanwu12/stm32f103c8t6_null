/**
 * @file os_diag.c
 * @author Yukikaze
 * @brief RTOS 诊断与 panic 实现文件。
 * @version 0.1
 * @date 2026-04-13
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件实现 panic hook 注册与 panic 停机路径。
 */

#include "os_diag.h"
#include "os_port.h"
#include "os_task.h"

static volatile uint8_t g_os_panic_active = 0U; // 非 0 表示当前已经进入 panic 路径
static os_panic_hook_t  g_os_panic_hook = NULL; // 当前注册的 panic hook

/**
 * @brief 注册或替换全局 panic hook。
 *
 * @param hook 新的 panic hook；传 NULL 表示清空。
 *
 * @return os_panic_hook_t 返回替换前的旧 hook。
 */
os_panic_hook_t os_panic_hook_set(os_panic_hook_t hook)
{
    os_panic_hook_t old_hook = NULL;
    uint32_t        primask = 0U;

    primask = os_port_enter_critical();
    old_hook = g_os_panic_hook;
    g_os_panic_hook = hook;
    os_port_exit_critical(primask);
    return old_hook;
}

/**
 * @brief 兼容旧名字的 panic hook 注册接口。
 *
 * @param hook 新的 panic hook；传 NULL 表示清空。
 *
 * @return os_panic_hook_t 返回替换前的旧 hook。
 */
os_panic_hook_t os_panic_set_hook(os_panic_hook_t hook)
{
    return os_panic_hook_set(hook);
}

/**
 * @brief 进入内核 panic 路径，调用 hook 后永久停机。
 *
 * @param reason 本次 panic 的原因。
 * @param file 触发 panic 的源文件。
 * @param line 触发 panic 的行号。
 */
void os_panic(os_panic_reason_t reason, const char *file, uint32_t line)
{
    os_panic_info_t info = {
        .reason = reason,
        .file = file,
        .line = line,
        .current_task = NULL,
    };
    os_panic_hook_t hook = NULL;

    (void)os_port_enter_critical();

    if (g_os_panic_active != 0U)
    {
        while (1)
        {
        }
    }

    g_os_panic_active = 1U;
    info.current_task = task_get_current();
    hook = g_os_panic_hook;

    if (hook != NULL)
    {
        hook(&info);
    }

    while (1)
    {
    }
}
