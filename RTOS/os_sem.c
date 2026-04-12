/**
 * @file os_sem.c
 * @author Yukikaze
 * @brief RTOS 二值信号量实现文件。
 * @version 0.1
 * @date 2026-04-12
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件实现当前阶段使用的二值信号量，以及对象等待链表的优先级排序规则。
 */

#include <string.h>
#include "os_port.h"
#include "os_sem.h"
#include "os_task.h"

#define OS_SEM_BINARY_MAX_COUNT 1U // 当前阶段的 public API 只暴露二值语义，因此最大计数固定为 1

static uint8_t os_sem_is_valid(const os_sem_t *sem);
static os_status_t os_sem_give_common(os_sem_t *sem);

/**
 * @brief 校验一个信号量对象是否已完成合法初始化。
 *
 * @param sem 待检查的信号量对象指针。
 *
 * @return uint8_t 非 0 表示对象有效，0 表示对象无效。
 */
static uint8_t os_sem_is_valid(const os_sem_t *sem)
{
    /* 空指针一定不可能是一个合法信号量对象。 */
    if (sem == NULL)
    {
        return 0U;
    }

    /* magic 负责识别“该对象是否已经完成过 os_sem_init()”。 */
    if (sem->magic != OS_SEM_MAGIC)
    {
        return 0U;
    }

    /* 当前阶段虽然内部按 counting core 存储，但 public 语义仍然是二值，
     * 所以 max_count 必须稳定等于 1。 */
    if (sem->max_count != OS_SEM_BINARY_MAX_COUNT)
    {
        return 0U;
    }

    /* current_count 不能超过 max_count，否则说明对象状态已经被破坏。 */
    if (sem->current_count > sem->max_count)
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief 初始化一个二值信号量对象。
 *
 * @param sem 调用方提供的信号量对象存储区。
 * @param initially_available 非 0 表示初始为“可用一次”，0 表示初始不可用；
 *                            所有非 0 输入都会被归一化成 1。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t os_sem_init(os_sem_t *sem, uint8_t initially_available)
{
    /* 信号量对象本身必须由调用方明确提供。 */
    if (sem == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 已初始化对象再次 init 会破坏已有等待链表，因此直接拒绝。 */
    if (sem->magic == OS_SEM_MAGIC)
    {
        return OS_STATUS_ALREADY_INITIALIZED;
    }

    /* 先清空整个对象，避免旧内容污染 wait_list 指针和计数字段。 */
    (void)memset(sem, 0, sizeof(os_sem_t));

    /* 当前阶段 public API 只暴露二值语义，因此 max_count 固定写成 1。
     * 这里把任何非 0 的 initially_available 都统一归一化成“可用一次”，
     * 以符合接口文档里“非 0 表示可用”的公开约定。 */
    sem->magic = OS_SEM_MAGIC;
    sem->current_count = (initially_available != 0U) ? 1U : 0U;
    sem->max_count = OS_SEM_BINARY_MAX_COUNT;

    /* 对象等待链表必须初始化为空，后续 take 阻塞时才能安全挂 waiter。 */
    list_init(&sem->wait_list);
    return OS_STATUS_OK;
}

/**
 * @brief 获取一个二值信号量；必要时进入阻塞等待。
 *
 * @param sem 目标信号量对象。
 * @param timeout_ticks 等待超时 tick 数；传 OS_WAIT_FOREVER 表示永久等待。
 *
 * @return os_status_t 获得信号量返回 OS_STATUS_OK；
 *                     若超时或立即失败，则返回 OS_STATUS_TIMEOUT；
 *                     失败时返回其他具体错误码。
 */
os_status_t os_sem_take(os_sem_t *sem, os_tick_t timeout_ticks)
{
    os_status_t status = OS_STATUS_OK; // 当前 take 路径得到的状态码
    uint32_t    primask = 0U;          // 外层临界区保存的 PRIMASK 原值
    tcb_t      *current_task = NULL;   // 当前发起 take 的线程对象

    /* take 只允许在线程态调用；
     * ISR 若需要同步，应当改走 give-from-isr 或更高层的中断到线程交接逻辑。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 先确认 semaphore 本身已经初始化完成。 */
    if (os_sem_is_valid(sem) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 当前任务指针必须存在，否则说明还没有真正运行在线程上下文里。 */
    current_task = task_get_current();
    if (current_task == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 快速路径和慢路径都会读取 current_count 与 wait_list，
     * 所以这里先进入外层临界区，保证“检查对象状态 + 决定是否阻塞”原子完成。 */
    primask = os_port_enter_critical();

    /* 若当前已经存在可用计数，就直接消耗掉它并成功返回。 */
    if (sem->current_count > 0U)
    {
        sem->current_count--;
        os_port_exit_critical(primask);
        return OS_STATUS_OK;
    }

    /* timeout=0 表示“只试一次，不等待”，对象当前不可用时直接按 TIMEOUT 返回。 */
    if (timeout_ticks == 0U)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_TIMEOUT;
    }

    /* 慢路径先把当前任务按对象等待规则挂进 semaphore wait_list，
     * 再调用任务层阻塞接口让它离开 runnable 集合。 */
    status = task_wait_list_insert_priority_ordered(&sem->wait_list, current_task);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 当前实现要求“对象等待链表挂链”和“task_block_current() 提交等待态”
     * 紧挨着发生，避免中间窗口里对象给出信号却找不到合法 waiter。 */
    status = task_block_current(sem, timeout_ticks, NULL);
    if (status != OS_STATUS_OK)
    {
        /* 若在真正阻塞前就失败，必须把刚刚挂进去的 event_node 立即摘掉，
         * 否则 wait_list 里会残留一个并未真正进入等待态的伪 waiter。 */
        task_wait_list_remove_task(&sem->wait_list, current_task);
        os_port_exit_critical(primask);
        return status;
    }

    /* 外层临界区必须在这里立即退出：
     * 1. 若 task_block_current() 已经 pend 了 PendSV，退出临界区后切换才会真正发生；
     * 2. 当前任务未来恢复运行时，也会从这里继续往下执行。 */
    os_port_exit_critical(primask);

    /* 任务恢复后，通过 wait_result 区分是“对象满足唤醒”还是“超时返回”。 */
    current_task = task_get_current();
    if (current_task == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (current_task->wait_result == TASK_WAIT_RESULT_OBJECT)
    {
        return OS_STATUS_OK;
    }

    if (current_task->wait_result == TASK_WAIT_RESULT_TIMEOUT)
    {
        return OS_STATUS_TIMEOUT;
    }

    /* 理论上不会走到这里；若走到这里，说明任务等待结果没有被正确写入。 */
    return OS_STATUS_INVALID_STATE;
}

/**
 * @brief 二值信号量 give 的公共实现；线程态与 ISR 态共用。
 *
 * @param sem 目标信号量对象。
 *
 * @return os_status_t give 成功返回 OS_STATUS_OK；失败时返回具体错误码。
 */
static os_status_t os_sem_give_common(os_sem_t *sem)
{
    os_status_t status = OS_STATUS_OK; // 唤醒 waiter 后得到的调度结果
    uint32_t    primask = 0U;          // 当前 give 路径外层临界区保存的 PRIMASK 原值
    tcb_t      *waiter = NULL;         // 当前最应该被唤醒的等待任务

    /* give 会读取 wait_list、修改 current_count，并且可能唤醒 waiter，
     * 所以整条路径必须放进临界区里完成。 */
    primask = os_port_enter_critical();

    if (os_sem_is_valid(sem) == 0U)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_INVALID_STATE;
    }

    /* wait_list 头节点就是当前最应该被唤醒的任务。 */
    waiter = task_wait_list_peek_head_task(&sem->wait_list);
    if (waiter != NULL)
    {
        /* 只要有 waiter，就把这次 give 直接转交给它；
         * 不先累加 current_count，避免“同一份可用资源既记到账上又交给 waiter”的重复结算。 */
        sem->current_count = 0U;

        status = task_unblock(waiter, TASK_WAIT_RESULT_OBJECT);
        if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
        {
            os_port_exit_critical(primask);
            return status;
        }

        /* 若唤醒后调度器判断应当切到更高优先级线程，就在退出临界区前提交 PendSV。
         * 在线程态里这是“先 pend 再开中断”，在 ISR 态里则会在异常尾部生效。 */
        if (status == OS_STATUS_SWITCH_REQUIRED)
        {
            os_port_trigger_pendsv();
        }

        os_port_exit_critical(primask);
        return OS_STATUS_OK;
    }

    /* 没有 waiter 时，give 只是把信号量恢复成“可用一次”。
     * 若它本来就已经是 1，则保持饱和，不把重复 give 视为错误。 */
    sem->current_count = sem->max_count;
    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 在线程态 give 一个二值信号量。
 *
 * @param sem 目标信号量对象。
 *
 * @return os_status_t give 成功返回 OS_STATUS_OK；失败时返回具体错误码。
 */
os_status_t os_sem_give(os_sem_t *sem)
{
    /* 线程态 give 不允许在 ISR 中被直接调用，避免调用语义混乱。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    return os_sem_give_common(sem);
}

/**
 * @brief 在 ISR 中 give 一个二值信号量。
 *
 * @param sem 目标信号量对象。
 *
 * @return os_status_t give 成功返回 OS_STATUS_OK；失败时返回具体错误码。
 */
os_status_t os_sem_give_from_isr(os_sem_t *sem)
{
    /* from-isr 版本只允许在异常/中断上下文里使用。 */
    if (os_port_is_in_interrupt() == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    return os_sem_give_common(sem);
}
