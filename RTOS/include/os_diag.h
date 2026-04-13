/**
 * @file os_diag.h
 * @author Yukikaze
 * @brief RTOS 诊断与 panic 接口定义文件。
 * @version 0.1
 * @date 2026-04-13
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件用于声明内核 panic、assert 与运行时诊断相关接口。
 */

#ifndef __OS_DIAG_H__
#define __OS_DIAG_H__

#include <stdint.h>
#include "os_config.h"
#include "os_types.h"

typedef enum {
    OS_PANIC_ASSERT = 0,          // 断言失败
    OS_PANIC_TASK_STATE,          // 任务状态或生命周期状态失配
    OS_PANIC_STACK_POINTER_RANGE, // 保存或恢复的 PSP 超出任务合法栈区间
    OS_PANIC_STACK_SENTINEL,      // 任务栈底哨兵字被覆盖，判定发生栈溢出
    OS_PANIC_PORT_FAILURE         // 端口层启动/切换链路发生异常
} os_panic_reason_t; // panic 原因枚举定义

typedef struct os_panic_info {
    os_panic_reason_t   reason;       // 本次 panic 的原因
    const char         *file;         // 触发 panic 的源文件
    uint32_t            line;         // 触发 panic 的源码行号
    const struct tcb   *current_task; // panic 发生时的当前任务快照
} os_panic_info_t; // panic 现场信息定义

typedef void (*os_panic_hook_t)(const os_panic_info_t *info); // panic hook 类型定义

os_panic_hook_t os_panic_set_hook(os_panic_hook_t hook);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#endif
void os_panic(os_panic_reason_t reason, const char *file, uint32_t line);

#if (OS_ASSERT_ENABLE != 0U)
    #define OS_ASSERT(expr)                                                         \
        do                                                                          \
        {                                                                           \
            if ((expr) == 0)                                                        \
            {                                                                       \
                os_panic(OS_PANIC_ASSERT, __FILE__, (uint32_t)__LINE__);            \
            }                                                                       \
        } while (0)
#else
    #define OS_ASSERT(expr) do { (void)sizeof(expr); } while (0)
#endif

#endif /* __OS_DIAG_H__ */
