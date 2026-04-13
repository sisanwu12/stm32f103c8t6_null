/**
 * @file os_mutex.c
 * @author Yukikaze
 * @brief RTOS non-recursive mutex 实现文件。
 * @version 0.1
 * @date 2026-04-12
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件实现带 owner 语义与优先级继承的互斥锁。
 */

#include <string.h>
#include "internal/os_task_internal.h"
#include "os_mutex.h"
#include "os_port.h"

static uint8_t os_mutex_is_valid(const os_mutex_t *mutex);
static os_status_t os_mutex_owner_link_locked(os_mutex_t *mutex, tcb_t *owner);
static void os_mutex_owner_unlink_locked(os_mutex_t *mutex);
static void os_mutex_wait_cleanup_locked(tcb_t *task);

/**
 * @brief 校验一个互斥锁对象是否已完成合法初始化。
 *
 * @param mutex 待检查的互斥锁对象指针。
 *
 * @return uint8_t 非 0 表示对象有效，0 表示对象无效。
 */
static uint8_t os_mutex_is_valid(const os_mutex_t *mutex)
{
    /* 空指针一定不可能是一个合法 mutex 对象。 */
    if (mutex == NULL)
    {
        return 0U;
    }

    /* magic 负责识别“该对象是否已经完成过 os_mutex_init()”。 */
    if (mutex->magic != OS_MUTEX_MAGIC)
    {
        return 0U;
    }

    /* owner 为空时，owner_node 不应继续残留在任何持锁链表里。 */
    if ((mutex->owner == NULL) && (mutex->owner_node.owner != NULL))
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief 在临界区内把 mutex 挂进 owner 的持锁链表，并提交 owner 指针。
 *
 * @param mutex 待挂接的互斥锁对象。
 * @param owner 即将成为 owner 的任务对象。
 *
 * @return os_status_t 挂接成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t os_mutex_owner_link_locked(os_mutex_t *mutex, tcb_t *owner)
{
    /* 链接 owner 前，mutex 和 owner 都必须有效。 */
    if ((os_mutex_is_valid(mutex) == 0U) || (owner == NULL))
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 一个 mutex 同一时刻只能属于一个 owner，owner_node 也不能重复挂链。 */
    if ((mutex->owner != NULL) || (mutex->owner_node.owner != NULL))
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (list_insert_tail(&owner->owned_mutex_list, &mutex->owner_node) == 0U)
    {
        return OS_STATUS_INSERT_FAILED;
    }

    mutex->owner = owner;
    return OS_STATUS_OK;
}

/**
 * @brief 在临界区内把 mutex 从旧 owner 的持锁链表上摘掉。
 *
 * @param mutex 待摘除 owner 关系的互斥锁对象。
 */
static void os_mutex_owner_unlink_locked(os_mutex_t *mutex)
{
    /* 无效 mutex 或当前本就没有 owner 时，都没有可摘除的 owner 关系。 */
    if ((os_mutex_is_valid(mutex) == 0U) || (mutex->owner == NULL))
    {
        return;
    }

    /* owner_node 当前若确实挂在 owner 的持锁链表上，就先摘掉。 */
    if (mutex->owner_node.owner == &mutex->owner->owned_mutex_list)
    {
        (void)list_remove(&mutex->owner->owned_mutex_list, &mutex->owner_node);
    }

    mutex->owner = NULL;
}

/**
 * @brief waiter 因 timeout/delete 离开 mutex wait_list 时触发的清理回调。
 *
 * @param task 当前离开 mutex 等待队列的任务对象。
 */
static void os_mutex_wait_cleanup_locked(tcb_t *task)
{
    os_mutex_t *mutex = NULL; // 当前 waiter 原本等待的 mutex 对象

    /* cleanup hook 只对“合法 waiter + 合法 wait_obj”有意义。 */
    if ((task == NULL) || (task->wait_obj == NULL))
    {
        return;
    }

    mutex = (os_mutex_t *)task->wait_obj;
    if (os_mutex_is_valid(mutex) == 0U)
    {
        return;
    }

    /* waiter 离开等待队列后，old owner 的继承优先级可能需要下降，
     * 所以这里立刻按其剩余持锁集合重新计算一次生效优先级。 */
    if (mutex->owner != NULL)
    {
        (void)task_priority_inheritance_refresh_locked(mutex->owner);
    }
}

/**
 * @brief 初始化一个 non-recursive mutex 对象。
 *
 * @param mutex 调用方提供的互斥锁对象存储区。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t os_mutex_init(os_mutex_t *mutex)
{
    /* mutex 对象本身必须由调用方明确提供。 */
    if (mutex == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 已初始化对象再次 init 会破坏已有 owner/waiter 关系，因此直接拒绝。 */
    if (mutex->magic == OS_MUTEX_MAGIC)
    {
        return OS_STATUS_ALREADY_INITIALIZED;
    }

    /* 先清空整个对象，消掉历史 owner、wait_list 与 owner_node 残留。 */
    (void)memset(mutex, 0, sizeof(os_mutex_t));

    mutex->magic = OS_MUTEX_MAGIC;
    mutex->owner = NULL;
    list_init(&mutex->wait_list);
    list_node_init(&mutex->owner_node);
    return OS_STATUS_OK;
}

/**
 * @brief 在线程态获取一把 non-recursive mutex。
 *
 * @param mutex 目标互斥锁对象。
 * @param timeout_ticks 互斥锁被持有时的等待超时 tick 数；传 OS_WAIT_FOREVER 表示永久等待。
 *
 * @return os_status_t 获取成功返回 OS_STATUS_OK；
 *                     若被别人持有且立即失败或等待超时，则返回 OS_STATUS_TIMEOUT；
 *                     其他违规则返回具体错误码。
 */
os_status_t os_mutex_lock(os_mutex_t *mutex, os_tick_t timeout_ticks)
{
    os_status_t status = OS_STATUS_OK; // 当前 lock 路径得到的状态码
    uint32_t    primask = 0U;          // 当前外层临界区保存的 PRIMASK 原值
    tcb_t      *current_task = NULL;   // 当前发起 lock 的线程对象
    tcb_t      *owner = NULL;          // lock 失败时当前持锁 owner

    /* mutex lock 只允许在线程态调用。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (os_mutex_is_valid(mutex) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    current_task = task_get_current();
    if (current_task == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    primask = os_port_enter_critical();

    /* 快速路径：当前没有 owner 时，直接拿锁。 */
    if (mutex->owner == NULL)
    {
        status = os_mutex_owner_link_locked(mutex, current_task);
        os_port_exit_critical(primask);
        return status;
    }

    owner = mutex->owner;

    /* non-recursive mutex 不允许 owner 自己再次 lock 同一把锁。 */
    if (owner == current_task)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_RECURSIVE_LOCK;
    }

    /* lock(timeout=0) 表示立即失败，不进入等待。 */
    if (timeout_ticks == 0U)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_TIMEOUT;
    }

    /* 慢路径先把当前任务挂进 mutex wait_list。 */
    status = task_wait_list_insert_priority_ordered(&mutex->wait_list, current_task);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 若 waiter 比当前 owner 优先级更高，就立即对 owner 执行优先级继承。 */
    status = task_priority_inheritance_raise_locked(owner, current_task->priority);
    if (status != OS_STATUS_OK)
    {
        task_wait_list_remove_task(&mutex->wait_list, current_task);
        os_port_exit_critical(primask);
        return status;
    }

    /* waiter cleanup hook 专门给 timeout/delete 离开 wait_list 时刷新 owner 继承优先级。 */
    status = task_block_current(mutex, timeout_ticks, os_mutex_wait_cleanup_locked);
    if (status != OS_STATUS_OK)
    {
        /* 若在真正阻塞前失败，要把 event_node 摘掉，并把 owner 优先级恢复到当前应有值。 */
        task_wait_list_remove_task(&mutex->wait_list, current_task);
        (void)task_priority_inheritance_refresh_locked(owner);
        os_port_exit_critical(primask);
        return status;
    }

    /* 阻塞路径已经把 PendSV 与等待态提交好；退出临界区后，当前线程可能立刻切走。 */
    os_port_exit_critical(primask);

    /* waiter 被对象满足唤醒后，ownership 已经在 unlock 临界区内原子转交给它；
     * 因此恢复后不需要重试循环，直接按 wait_result 区分 OK/TIMEOUT 即可。 */
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

    return OS_STATUS_INVALID_STATE;
}

/**
 * @brief 在线程态释放一把由当前任务持有的 mutex。
 *
 * @param mutex 目标互斥锁对象。
 *
 * @return os_status_t 释放成功返回 OS_STATUS_OK；失败时返回具体错误码。
 */
os_status_t os_mutex_unlock(os_mutex_t *mutex)
{
    os_status_t status = OS_STATUS_OK; // 当前 unlock 路径得到的状态码
    uint32_t    primask = 0U;          // 当前外层临界区保存的 PRIMASK 原值
    tcb_t      *current_task = NULL;   // 当前执行 unlock 的线程对象
    tcb_t      *waiter = NULL;         // 当前最应该接班 ownership 的 waiter
    tcb_t      *old_owner = NULL;      // 当前被释放 mutex 的旧 owner

    /* mutex unlock 只允许在线程态调用。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (os_mutex_is_valid(mutex) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    current_task = task_get_current();
    if (current_task == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 非 owner 解锁直接报错，不允许像 semaphore 一样“任何人都能 give”。 */
    if (mutex->owner != current_task)
    {
        return OS_STATUS_NOT_OWNER;
    }

    primask = os_port_enter_critical();
    old_owner = mutex->owner;

    /* wait_list 为空时，只需释放 ownership 并把 old owner 优先级恢复。 */
    waiter = task_wait_list_peek_head_task(&mutex->wait_list);
    if (waiter == NULL)
    {
        os_mutex_owner_unlink_locked(mutex);

        status = task_priority_inheritance_refresh_locked(old_owner);
        if (status != OS_STATUS_OK)
        {
            os_port_exit_critical(primask);
            return status;
        }

        /* owner 优先级恢复后，可能需要让出 CPU，因此要重新调度。 */
        status = task_schedule();
        if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
        {
            os_port_exit_critical(primask);
            return status;
        }

        if (status == OS_STATUS_SWITCH_REQUIRED)
        {
            os_port_trigger_pendsv();
        }

        os_port_exit_critical(primask);
        return OS_STATUS_OK;
    }

    /* wait_list 非空时，当前版本采用“直接 owner 交接”：
     * 先把队头 waiter 从等待链表摘掉，再把 ownership 原子转给它。 */
    task_wait_list_remove_task(&mutex->wait_list, waiter);
    os_mutex_owner_unlink_locked(mutex);

    status = os_mutex_owner_link_locked(mutex, waiter);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 新 owner 接手 mutex 后，若其后面仍有人在等同一把锁，
     * 它也可能需要立即继承更高优先级。 */
    status = task_priority_inheritance_refresh_locked(waiter);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* old owner 放锁后，再按它剩余持有的其他 mutex 重算生效优先级。 */
    status = task_priority_inheritance_refresh_locked(old_owner);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* ownership 交接完成后，再把新 owner 对应的 waiter 真正唤醒。 */
    status = task_unblock(waiter, TASK_WAIT_RESULT_OBJECT);
    if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
    {
        os_port_exit_critical(primask);
        return status;
    }

    if (status == OS_STATUS_SWITCH_REQUIRED)
    {
        os_port_trigger_pendsv();
    }

    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}
