/**
 * @file os_port_cortex_m3.c
 * @author Yukikaze
 * @brief Cortex-M3 RTOS 专用移植层实现文件。
 * @version 0.1
 * @date 2026-03-26
 *
 * @copyright Copyright (c) 2026 
 *
 * @note 本文件用于放置 Cortex-M3 平台相关的上下文切换、异常入口与节拍处理代码。
 */

#include <stddef.h>
#include <stdint.h>
#include "stm32f1xx.h"
#include "os_task.h"
#include "os_port_cortex_m3.h"

extern uint32_t _estack;
/* Cortex-M 异常返回时要求 xPSR 的 T 位为 1，
 * 否则 CPU 不会按 Thumb 状态执行任务入口。 */
#define OS_PORT_INITIAL_XPSR            0x01000000UL
/* 任务栈顶在建立初始栈帧前，需要按配置要求向下对齐。 */
#define OS_PORT_STACK_ALIGN_MASK        ((uintptr_t)(OS_TASK_STACK_ALIGNMENT_BYTES - 1U))
/* EXC_RETURN = 0xFFFFFFFD 的含义是：
 * 返回到 Thread mode，使用 PSP，并按基本异常栈帧恢复现场。 */
#define OS_PORT_EXC_RETURN_THREAD_PSP   0xFFFFFFFDUL

#if defined(__GNUC__) && (defined(__arm__) || defined(__thumb__))
    /* naked 用于告诉编译器不要自动生成函数序言/结语，
     * 这样首任务启动和 PendSV 才能完全按我们写的汇编控制栈与寄存器。 */
    #define OS_PORT_NAKED __attribute__((naked))
    /* used 用于防止这些仅被内联汇编按符号名引用的静态函数
     * 在优化阶段被编译器误判为“未使用”并删掉。 */
    #define OS_PORT_USED __attribute__((used))
#else
    #define OS_PORT_NAKED
    #define OS_PORT_USED
#endif

static OS_PORT_USED void os_port_configure_pendsv_priority(void);
static OS_PORT_USED uint32_t *os_port_switch_context(uint32_t *stack_pointer);

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
 * @brief 将 PendSV 优先级设置为系统支持的最低优先级。
 */
static OS_PORT_USED void os_port_configure_pendsv_priority(void)
{
    uint32_t priority = (1UL << __NVIC_PRIO_BITS) - 1UL;

    NVIC_SetPriority(PendSV_IRQn, priority);
}

/**
 * @brief 保存当前任务软件上下文并切换到下一个任务，返回其软件保存区栈顶。
 *
 * @param stack_pointer 当前任务压入 r4-r11 后的 PSP 值。
 *
 * @return uint32_t* 下一个任务的软件栈顶；若切换失败则返回原栈指针。
 */
static OS_PORT_USED uint32_t *os_port_switch_context(uint32_t *stack_pointer)
{
    tcb_t *current_task = NULL;
    tcb_t *next_task = NULL;
    os_status_t status = OS_STATUS_OK;

    /* 当前任务非空时，说明这不是首任务启动，而是一次普通的 PendSV 切换。
     * 这里先把压入 r4-r11 之后得到的新 PSP 保存回 current task 的 TCB。 */
    current_task = task_get_current();
    if (current_task != NULL)
    {
        current_task->sp = stack_pointer;
    }

    /* g_next_task 由调度器提前选好；port 层这里只负责消费这个结果。 */
    next_task = task_get_next();
    if (next_task == NULL)
    {
        /* 没有 next task 时，保持当前上下文不变。
         * 若当前任务存在，就返回它刚刚保存下来的栈顶；若连 current 都没有，
         * 说明首任务启动链路本身就不成立，返回 NULL 让上层停在兜底路径。 */
        return (current_task != NULL) ? current_task->sp : NULL;
    }

    /* 把调度器选中的 next task 正式提交成 current task。
     * 这一步会同步更新 g_current_task 和任务状态。 */
    status = task_set_current(next_task);
    if (status != OS_STATUS_OK)
    {
        /* 若提交失败，也不能随便切走；这里退回到原 current 的栈顶，
         * 让异常返回继续回到原任务。首任务路径下则返回 NULL。 */
        return (current_task != NULL) ? current_task->sp : NULL;
    }

    /* 切换提交成功后，返回新 current task 的软件保存区栈顶，
     * PendSV 后半段会据此恢复 r4-r11，并继续走异常返回。 */
    return next_task->sp;
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

/**
 * @brief 触发一次 PendSV 异常，请求在异常返回路径上完成上下文切换。
 */
void os_port_trigger_pendsv(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

/**
 * @brief 启动首任务，切换到 PSP 并通过异常返回进入任务上下文。
 */
OS_PORT_NAKED void os_port_start_first_task(void)
{
#if defined(__GNUC__) && (defined(__arm__) || defined(__thumb__))
    __asm volatile(
        /* 先在当前启动栈上完成 PendSV 优先级配置。 */
        "bl os_port_configure_pendsv_priority \n"
        /* 关中断，避免在重置 MSP 的过渡窗口被别的异常打断。 */
        "cpsid i                            \n"
        /* 直接把 MSP 恢复到链接脚本给出的初始栈顶，
         * 这样 main -> os_kernel_start -> os_port_start_first_task
         * 这条调用链就不会继续占用中断栈空间。 */
        "ldr r0, =_estack                  \n"
        "msr msp, r0                       \n"
        /* 置位 PendSV，请求由异常返回路径切入首任务。 */
        "bl os_port_trigger_pendsv         \n"
        /* 重新开中断后，PendSV 或更高优先级异常都会在“已恢复的 MSP”上运行。 */
        "cpsie i                           \n"
        /* 若首任务切入正常，这里不会继续向前推进；留本地死循环兜底。 */
        "1:                                \n"
        "b 1b                              \n"
    );
#else
    while (1)
    {
    }
#endif
}

/**
 * @brief PendSV 异常处理函数，负责执行任务软件上下文切换。
 *
 * @note 汇编流程分成三段：
 *       1. 先判断当前是否已经存在 current task。
 *          - 若存在，说明这是一次普通任务切换，需要先从 PSP 中取出当前线程栈，
 *            并把软件保存寄存器 r4-r11 压回当前任务栈；
 *          - 若不存在，说明这是首任务启动路径，此时还没有旧任务可保存，
 *            直接把 r0 置 0 传给 os_port_switch_context()。
 *       2. 调用 os_port_switch_context()：
 *          - 若有旧任务，则把刚保存后的 PSP 写回旧任务 TCB；
 *          - 再把 g_next_task 提交成新的 g_current_task；
 *          - 返回新任务的软件保存区栈顶，供后半段恢复 r4-r11。
 *       3. 若返回的栈顶非空，则恢复新任务的 r4-r11，写回 PSP，
 *          再把 CONTROL.SPSEL 置为 1，确保 Thread mode 使用 PSP，
 *          最后通过 EXC_RETURN=0xFFFFFFFD 异常返回到任务上下文。
 *          若返回空指针，则停在本地死循环，避免带着坏栈继续运行。
 */
OS_PORT_NAKED void PendSV_Handler(void)
{
#if defined(__GNUC__) && (defined(__arm__) || defined(__thumb__))
    __asm volatile(
        /* 先问调度器：当前是否已经存在 current task。
         * 首任务启动时这里会返回 NULL；普通任务切换时会返回旧任务指针。 */
        "bl task_get_current                \n"
        /* current == NULL 表示还没有旧任务可保存，直接跳到标签 1。 */
        "cbz r0, 1f                         \n"
        /* 普通切换路径：读取当前线程栈 PSP。 */
        "mrs r0, psp                        \n"
        /* 把软件保存寄存器 r4-r11 压回当前任务栈。
         * Cortex-M 异常硬件只会自动保存 r0-r3、r12、lr、pc、xpsr，
         * 所以 r4-r11 需要 RTOS 自己保存。 */
        "stmdb r0!, {r4-r11}                \n"
        /* 保存完成后跳过“首任务路径”的 r0=0 处理。 */
        "b 2f                               \n"
        /* 首任务路径：没有旧任务上下文要保存，给 C 辅助函数传 NULL。 */
        "1:                                 \n"
        "movs r0, #0                        \n"
        /* r0 现在统一表示：
         * - 普通切换时：旧任务保存完 r4-r11 后的 PSP
         * - 首任务启动时：NULL */
        "2:                                 \n"
        /* 进入 C 辅助函数，完成“保存旧任务栈顶 + 提交 next 为 current +
         * 取出新任务栈顶”这组调度器相关动作。 */
        "bl os_port_switch_context          \n"
        /* 若返回 NULL，说明没有可切入的新任务或提交 current 失败，
         * 直接跳到本地死循环兜底。 */
        "cbz r0, 3f                         \n"
        /* 切换成功后，从新任务栈中恢复 r4-r11。 */
        "ldmia r0!, {r4-r11}                \n"
        /* 恢复后的 r0 指向的是“硬件异常栈帧”的起始位置，把它写回 PSP。 */
        "msr psp, r0                        \n"
        /* CONTROL[1]=1 表示 Thread mode 使用 PSP。
         * 这里保持特权线程模式，只切换栈指针来源。 */
        "movs r0, #2                        \n"
        "msr control, r0                    \n"
        /* 指令同步屏障，确保 CONTROL 的修改立刻生效。 */
        "isb                                \n"
        /* 构造 EXC_RETURN=0xFFFFFFFD：
         * 异常返回到 Thread mode，使用 PSP，并按基本异常栈帧恢复现场。 */
        "ldr lr, =0xFFFFFFFD                \n"
        /* 走异常返回，CPU 会自动从 PSP 中恢复硬件栈帧：
         * r0-r3、r12、lr、pc、xpsr。 */
        "bx lr                              \n"
        /* 失败兜底：停在原地，避免带着损坏的上下文继续执行。 */
        "3:                                 \n"
        "b 3b                               \n"
    );
#else
    while (1)
    {
    }
#endif
}
