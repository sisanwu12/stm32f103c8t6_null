/**
 * @file os_task.c
 * @author Yukikaze
 * @brief RTOS 任务管理与调度器实现文件。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件实现任务生命周期管理、调度、延时处理与阻塞唤醒逻辑。
 */

#include <string.h>
#include "os_diag.h"
#include "internal/os_task_internal.h"
#include "os_port.h"
#include "os_mutex.h"

#define OS_IDLE_TASK_NAME "idle" // 内核自带 idle 任务名称
#define OS_TICK_COMPARE_HALF_RANGE ((os_tick_t)0x80000000UL) // 基于有符号差值比较时，允许安全比较的最大半区间

/* 全局可运行任务集合，同时包含 TASK_READY 和当前 TASK_RUNNING 任务。 */
static ready_queue_t g_task_ready_queue;
/* 全局按唤醒时间排序的 timed-wait 链表，
 * 统一承载 TASK_SLEEPING 任务和“带超时”的 TASK_BLOCKED 任务。 */
static list_t g_task_timed_wait_list;
/* 全局绝对 tick 计数，由 SysTick 路径单调递增。 */
static volatile os_tick_t g_os_tick = 0U;
/* 当前正在 CPU 上运行的任务控制块指针。 */
static tcb_t *g_current_task = NULL;
/* 调度器本次选出的下一个待运行任务控制块指针。 */
static tcb_t *g_next_task = NULL;
/* 任务系统初始化标志，非 0 表示全局调度状态已经建立。 */
static uint8_t g_task_system_initialized = 0U;

/* 内核私有 idle 任务控制块。 */
static tcb_t g_idle_task;
/* 内核私有 idle 任务栈存储区。 */
static uint32_t g_idle_task_stack[OS_IDLE_TASK_STACK_DEPTH];

static uint8_t ready_queue_priority_is_valid(uint8_t priority);
static uint8_t task_user_priority_is_valid(uint8_t priority);
static uint32_t ready_queue_priority_mask(uint8_t priority);
static os_status_t task_select_next(void);
static list_t *task_get_ready_list_by_priority(uint8_t priority);
static uint8_t task_is_known_to_scheduler(const tcb_t *task);
static uint8_t task_is_valid(const tcb_t *task);
static os_status_t task_validate_running_task(const tcb_t *task);
static os_status_t task_validate_init_config(const task_init_config_t *config, uint8_t allow_idle_priority);
static uint8_t task_normalize_time_slice(uint8_t time_slice);
static void task_idle_entry(void *param);
static os_status_t task_create_idle_task(void);
static os_status_t task_init(tcb_t *task, const task_init_config_t *config, uint8_t allow_idle_priority);
static void task_fill_stack_pattern(uint32_t *stack_base, uint32_t stack_size);
static uint8_t task_stack_sentinel_is_intact(const tcb_t *task);
static uint8_t task_is_in_runnable_queue(const tcb_t *task);
static uint8_t task_is_in_timed_wait_list(const tcb_t *task);
static uint8_t task_timeout_is_supported(os_tick_t timeout_ticks);
static uint8_t task_tick_is_due(os_tick_t current_tick, os_tick_t target_tick);
static uint8_t task_tick_deadline_is_before(os_tick_t lhs, os_tick_t rhs);
static void timed_wait_list_insert_ordered(tcb_t *task);
static void task_wait_cleanup_invoke_locked(tcb_t *task);
static os_status_t task_make_runnable_locked(tcb_t *task, task_wait_result_t wait_result);
static os_status_t task_unblock_locked(tcb_t *task, task_wait_result_t wait_result);
static os_status_t task_prepare_wait_locked(tcb_t *task,
                                            task_state_t wait_state,
                                            void *wait_obj,
                                            os_tick_t timeout_ticks,
                                            task_wait_cleanup_fn_t wait_cleanup_locked);
static os_status_t task_wake_timed_tasks_locked(void);
static os_status_t task_handle_time_slice_tick_locked(void);
static os_status_t task_detach_from_scheduler_locked(tcb_t *task);
static os_status_t task_mark_deleted_locked(tcb_t *task);

/**
 * @brief 用固定 pattern 预填整个任务栈。
 *
 * @param stack_base 任务栈低地址起点。
 * @param stack_size 任务栈深度，单位为 uint32_t。
 */
static void task_fill_stack_pattern(uint32_t *stack_base, uint32_t stack_size)
{
    uint32_t index = 0U;

    if (stack_base == NULL)
    {
        return;
    }

    for (index = 0U; index < stack_size; index++)
    {
        stack_base[index] = OS_TASK_STACK_FILL_PATTERN;
    }
}

/**
 * @brief 检查任务栈底哨兵字是否仍保持完整。
 *
 * @param task 待检查的任务对象。
 *
 * @return uint8_t 非 0 表示哨兵字完整，0 表示已被覆盖。
 */
static uint8_t task_stack_sentinel_is_intact(const tcb_t *task)
{
    if ((task_is_valid(task) == 0U) || (task->stack_base == NULL) || (task->stack_size == 0U))
    {
        return 0U;
    }

    return (uint8_t)(task->stack_base[0] == OS_TASK_STACK_FILL_PATTERN);
}

/**
 * @brief 判断任务是否已经处于全局可运行任务集合中。
 *
 * @param task 待检查的任务控制块。
 *
 * @return uint8_t 非 0 表示任务在可运行集合中，0 表示任务不在可运行集合中。
 */
static uint8_t task_is_in_runnable_queue(const tcb_t *task)
{
    /* 先确认 task 至少是一个已经完成初始化的合法 TCB；
     * 否则继续访问它的 priority 或 sched_node 都没有意义。 */
    if (task_is_valid(task) == 0U)
    {
        return 0U;
    }

    /* runnable 判定依赖“按 priority 映射到哪条 ready list”，
     * 所以 priority 越界时直接视为“不在 runnable 集合”。 */
    if (ready_queue_priority_is_valid(task->priority) == 0U)
    {
        return 0U;
    }

    /* 当前版本不靠 state 字段判断 runnable，
     * 而是直接看 sched_node 是否真的挂在对应优先级的 ready list 上。 */
    return (uint8_t)(task->sched_node.owner == &g_task_ready_queue.ready_lists[task->priority]);
}

/**
 * @brief 判断任务是否处于 timed-wait 链表中。
 *
 * @param task 待检查的任务控制块。
 *
 * @return uint8_t 非 0 表示任务当前挂在 timed-wait 链表中，0 表示不在链表中。
 */
static uint8_t task_is_in_timed_wait_list(const tcb_t *task)
{
    /* 非法 TCB 不可能是 timed-wait 链表里的合法成员。 */
    if (task_is_valid(task) == 0U)
    {
        return 0U;
    }

    /* timed-wait 的判定标准同样不是看 state，
     * 而是看 sched_node 当前是否真的归属于 timed-wait 链表。 */
    return (uint8_t)(task->sched_node.owner == &g_task_timed_wait_list);
}

/**
 * @brief 查看某条对象等待链表头部的 waiter。
 *
 * @param wait_list 待查看的对象等待链表。
 *
 * @return tcb_t* 当前最应该被对象唤醒的 waiter；若链表为空则返回 NULL。
 */
tcb_t *task_wait_list_peek_head_task(const list_t *wait_list)
{
    /* 空链表对象或空头节点都表示“当前没有 waiter 可唤醒”。 */
    if ((wait_list == NULL) || (wait_list->head == NULL))
    {
        return NULL;
    }

    /* 对象等待链表统一通过 event_node 挂接任务对象，
     * 因此队头节点可以直接反推出外层 tcb_t。 */
    return LIST_CONTAINER_OF(wait_list->head, tcb_t, event_node);
}

/**
 * @brief 按“高优先级优先、同优先级 FIFO”把任务插入对象等待链表。
 *
 * @param wait_list 目标对象等待链表。
 * @param task 当前准备进入等待态的任务。
 *
 * @return os_status_t 插入成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_wait_list_insert_priority_ordered(list_t *wait_list, tcb_t *task)
{
    list_node_t *current_node = NULL; // 当前扫描到的等待链表节点
    list_node_t *new_node     = NULL; // 当前待插入任务对应的 event_node
    tcb_t       *queued_task  = NULL; // current_node 反推得到的外层任务对象

    /* 等待链表插入要求同时拿到合法链表对象与合法任务对象。 */
    if ((wait_list == NULL) || (task_is_valid(task) == 0U))
    {
        return OS_STATUS_INVALID_STATE;
    }

    new_node = &task->event_node;

    /* event_node 只允许挂在一条对象等待链表上；
     * 若 owner 已非空，说明调用方的等待状态组织已经失配。 */
    if (new_node->owner != NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 空等待链表时，当前任务直接成为首个 waiter。 */
    if (wait_list->head == NULL)
    {
        if (list_insert_tail(wait_list, new_node) == 0U)
        {
            return OS_STATUS_INSERT_FAILED;
        }

        return OS_STATUS_OK;
    }

    /* 非空等待链表按优先级从高到低扫描：
     * 数值越小优先级越高，所以一旦发现当前任务 priority 更高，
     * 就把它插到第一个“优先级更低”的 waiter 前面。 */
    current_node = wait_list->head;
    while (current_node != NULL)
    {
        queued_task = LIST_CONTAINER_OF(current_node, tcb_t, event_node);
        if (task->priority < queued_task->priority)
        {
            /* 找到插入点后，手工修补双向链表指针，让当前任务插到 current_node 前面。 */
            new_node->prev  = current_node->prev;
            new_node->next  = current_node;
            new_node->owner = wait_list;

            if (current_node->prev != NULL)
            {
                current_node->prev->next = new_node;
            }
            else
            {
                wait_list->head = new_node;
            }

            current_node->prev = new_node;
            wait_list->item_count++;
            return OS_STATUS_OK;
        }

        /* 同优先级任务要保持 FIFO，所以这里继续向后扫描，不抢到已有同优先级 waiter 前面。 */
        current_node = current_node->next;
    }

    /* 扫到链尾仍未找到更低优先级 waiter，说明当前任务应当排在最后。 */
    if (list_insert_tail(wait_list, new_node) == 0U)
    {
        return OS_STATUS_INSERT_FAILED;
    }

    return OS_STATUS_OK;
}

/**
 * @brief 从指定对象等待链表中摘掉任务的 event_node。
 *
 * @param wait_list 目标对象等待链表。
 * @param task 待摘除的任务对象。
 */
void task_wait_list_remove_task(list_t *wait_list, tcb_t *task)
{
    /* 没有合法链表或合法任务时，不存在可摘除的等待节点。 */
    if ((wait_list == NULL) || (task_is_valid(task) == 0U))
    {
        return;
    }

    /* 只有 event_node 当前确实挂在这条等待链表上时，才执行摘链；
     * 这样可以避免误删其他对象上的 waiter。 */
    if (task->event_node.owner == wait_list)
    {
        (void)list_remove(wait_list, &task->event_node);
    }
}

/**
 * @brief 在临界区内执行 waiter 清理回调。
 *
 * @param task 当前正在因 timeout/delete 离开等待态的任务。
 */
static void task_wait_cleanup_invoke_locked(tcb_t *task)
{
    task_wait_cleanup_fn_t cleanup = NULL; // 当前任务上登记的对象侧清理回调

    /* 只有 BLOCKED 任务才存在“对象等待清理”语义。 */
    if ((task_is_valid(task) == 0U) || (task->state != TASK_BLOCKED) || (task->wait_cleanup_locked == NULL))
    {
        return;
    }

    /* 先把回调指针摘下来，防止对象侧清理过程中重复进入同一回调。 */
    cleanup = task->wait_cleanup_locked;
    task->wait_cleanup_locked = NULL;
    cleanup(task);
}

/**
 * @brief 在临界区内更新任务的当前生效优先级，并在需要时重挂相关链表。
 *
 * @param task 待更新的任务对象。
 * @param new_priority 目标生效优先级。
 *
 * @return os_status_t 更新成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_effective_priority_update_locked(tcb_t *task, uint8_t new_priority)
{
    list_t *ready_list = NULL;       // 任务当前生效优先级对应的 ready list
    list_t *wait_list  = NULL;       // 任务当前所在的对象等待链表
    uint8_t was_running = 0U;        // 非 0 表示更新前该任务正处于 RUNNING
    uint8_t was_waiting_on_object = 0U; // 非 0 表示更新前该任务正挂在对象等待链表

    /* 目标必须是一个合法 TCB，且新优先级必须落在 ready bitmap 支持范围内。 */
    if ((task_is_valid(task) == 0U) || (ready_queue_priority_is_valid(new_priority) == 0U))
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 生效优先级本就相同时，不需要做任何链表重排。 */
    if (task->priority == new_priority)
    {
        return OS_STATUS_OK;
    }

    was_running = (uint8_t)(task->state == TASK_RUNNING);
    was_waiting_on_object = (uint8_t)(task->event_node.owner != NULL);

    /* 若任务当前在 ready queue 中，就先从旧优先级链表摘掉，再按新优先级重挂。 */
    if (task_is_in_runnable_queue(task) != 0U)
    {
        ready_queue_remove(&g_task_ready_queue, task);
        if (task->sched_node.owner != NULL)
        {
            return OS_STATUS_INVALID_STATE;
        }

        task->priority = new_priority;
        ready_list = task_get_ready_list_by_priority(new_priority);
        if (ready_list == NULL)
        {
            return OS_STATUS_INVALID_STATE;
        }

        /* 当前正在运行的任务若调整优先级，仍应保持自己是新优先级链表上的当前执行者，
         * 因此这里用头插保留“继续执行直到重新调度”的语义；非 current 任务仍走尾插。 */
        if (was_running != 0U)
        {
            if (list_insert_head(ready_list, &task->sched_node) == 0U)
            {
                return OS_STATUS_INSERT_FAILED;
            }

            g_task_ready_queue.ready_bitmap |= ready_queue_priority_mask(new_priority);
            task->state = TASK_RUNNING;
        }
        else
        {
            ready_queue_insert_tail(&g_task_ready_queue, task);
            if (task->sched_node.owner != ready_list)
            {
                return OS_STATUS_INSERT_FAILED;
            }
        }

        return OS_STATUS_OK;
    }

    /* 若任务当前挂在某个对象等待链表上，则要先摘掉 event_node，
     * 更新优先级后再按新的生效优先级重插回原等待链表。 */
    if (was_waiting_on_object != 0U)
    {
        os_mutex_t *waiting_mutex = NULL; // 若任务当前阻塞在 mutex 上，这里记录它正在等待的那把锁

        wait_list = task->event_node.owner;
        if (list_remove(wait_list, &task->event_node) == 0U)
        {
            return OS_STATUS_INVALID_STATE;
        }

        /* 在真正更新生效优先级前，先记住它正在等待的对象。
         * 对 sem/queue 来说，这个对象不会参与“优先级链式传播”；
         * 但若它在等 mutex，那么它的优先级变化还要继续传到 mutex owner。 */
        if (task->wait_obj != NULL)
        {
            waiting_mutex = (os_mutex_t *)task->wait_obj;
        }

        task->priority = new_priority;

        if (task_wait_list_insert_priority_ordered(wait_list, task) != OS_STATUS_OK)
        {
            return OS_STATUS_INSERT_FAILED;
        }

        /* 关键补丁：
         * 如果这个任务自己正在等待另一把 mutex，那么它的 waiter 排序变化
         * 不应只停留在本人的 event_node 上，还必须继续刷新那把 mutex 的 owner。
         * 这里不能只做“raise”，因为链式等待里 waiter 的优先级既可能被抬升，
         * 也可能因为 timeout/delete 而下降；因此必须统一走 refresh，
         * 让上游 owner 重新按当前全部 waiter 状态计算生效优先级。 */
        if ((task->state == TASK_BLOCKED) && (waiting_mutex != NULL) && (waiting_mutex->magic == OS_MUTEX_MAGIC) && (waiting_mutex->owner != NULL))
        {
            return task_priority_inheritance_refresh_locked(waiting_mutex->owner);
        }

        return OS_STATUS_OK;
    }

    /* 若任务只在 timed-wait 链表里，timed-wait 只按 wake_tick 排序，
     * 所以这里无需重排 sched_node，只更新当前生效优先级即可。 */
    task->priority = new_priority;
    return OS_STATUS_OK;
}

/**
 * @brief 在临界区内把任务生效优先级提升到给定继承优先级。
 *
 * @param task 需要被提升优先级的任务对象。
 * @param inherited_priority 当前应当继承到的更高优先级。
 *
 * @return os_status_t 提升成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_priority_inheritance_raise_locked(tcb_t *task, uint8_t inherited_priority)
{
    /* 非法任务或非法优先级都不允许继续做继承提升。 */
    if ((task_is_valid(task) == 0U) || (ready_queue_priority_is_valid(inherited_priority) == 0U))
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 数值更小代表优先级更高；只有 waiter 真正更高时，才需要提升 owner。 */
    if (inherited_priority >= task->priority)
    {
        return OS_STATUS_OK;
    }

    return task_effective_priority_update_locked(task, inherited_priority);
}

/**
 * @brief 在临界区内按当前持有的全部 mutex 重新计算任务应有的生效优先级。
 *
 * @param task 需要重算优先级的任务对象。
 *
 * @return os_status_t 重算成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_priority_inheritance_refresh_locked(tcb_t *task)
{
    list_node_t *node = NULL;          // 当前扫描到的持锁链表节点
    os_mutex_t  *mutex = NULL;         // node 对应的外层 mutex 对象
    tcb_t       *waiter = NULL;        // 当前 mutex 上最应该被考虑的 waiter
    uint8_t      new_priority = 0U;    // 结合 base_priority 与全部 waiter 后得到的新生效优先级

    /* 非法任务不能参与优先级继承重算。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 重算的起点永远是任务自己的原始优先级。 */
    new_priority = task->base_priority;

    /* 逐把扫描当前任务持有的 mutex，收集所有 wait_list 头 waiter 的最高优先级。 */
    node = task->owned_mutex_list.head;
    while (node != NULL)
    {
        mutex = LIST_CONTAINER_OF(node, os_mutex_t, owner_node);
        waiter = task_wait_list_peek_head_task(&mutex->wait_list);

        if ((waiter != NULL) && (waiter->priority < new_priority))
        {
            new_priority = waiter->priority;
        }

        node = node->next;
    }

    return task_effective_priority_update_locked(task, new_priority);
}

/**
 * @brief 根据优先级返回对应的可运行链表指针。
 *
 * @param priority 任务优先级。
 *
 * @return list_t* 优先级合法时返回对应链表，否则返回 NULL。
 */
static list_t *task_get_ready_list_by_priority(uint8_t priority)
{
    /* 调用方若给了非法优先级，这里不能返回任何链表地址，
     * 避免上层拿着错误指针继续改 ready queue。 */
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return NULL;
    }

    /* ready queue 采用“每个优先级一条链表”的组织方式，
     * 所以合法优先级可以直接映射到对应槽位。 */
    return &g_task_ready_queue.ready_lists[priority];
}

/**
 * @brief 判断一个任务控制块地址是否已经被调度器持有。
 *
 * @param task 待检查的任务控制块地址。
 *
 * @return uint8_t 非 0 表示该任务已经被调度器持有，0 表示当前未持有。
 */
static uint8_t task_is_known_to_scheduler(const tcb_t *task)
{
    /* 调度器尚未初始化时，不可能已经正式持有任何任务对象。 */
    if ((task == NULL) || (g_task_system_initialized == 0U))
    {
        return 0U;
    }

    /* 只有已经初始化过的 TCB，才有资格谈“是否被调度器持有”。 */
    if (task_is_valid(task) == 0U)
    {
        return 0U;
    }

    /* 当前任务和下一待运行任务都属于调度器正在管理的对象，
     * 即使它们暂时没有挂在别的链表上，也必须视为“已持有”。 */
    if ((task == g_current_task) || (task == g_next_task))
    {
        return 1U;
    }

    /* sched_node 只要还挂在某条链表上，就说明这个任务还被 ready/timed-wait 管理着。 */
    if (task->sched_node.owner != NULL)
    {
        return 1U;
    }

    /* event_node 若挂在某个对象等待链表上，也说明任务仍处于内核调度体系内。 */
    if (task->event_node.owner != NULL)
    {
        return 1U;
    }

    /* 走到这里说明它既不是 current/next，也没有挂在任何内核链表上，
     * 可以认为当前没有被调度器持有。 */
    return 0U;
}

/**
 * @brief 判断任务控制块是否已经完成合法初始化。
 *
 * @param task 待检查的任务控制块。
 *
 * @return uint8_t 非 0 表示任务控制块有效，0 表示任务控制块无效。
 */
static uint8_t task_is_valid(const tcb_t *task)
{
    /* 空指针一定不可能是一个合法任务控制块。 */
    if (task == NULL)
    {
        return 0U;
    }

    /* 当前版本用 magic 作为“TCB 已完成初始化”的唯一判据。 */
    return (uint8_t)(task->magic == OS_TASK_MAGIC);
}

/**
 * @brief 校验一个任务是否处于“当前正在运行且仍属于 runnable 集合”的状态。
 *
 * @param task 待检查的任务控制块。
 *
 * @return os_status_t 校验通过返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_validate_running_task(const tcb_t *task)
{
    /* 调度器全局状态还没建立时，任何“当前正在运行任务”的语义都不成立。 */
    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    /* 这条校验路径要求调用方必须给出明确的任务指针。 */
    if (task == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 首先要确认对象本身是一个合法 TCB，而不是未初始化内存。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 运行中的任务仍然留在 runnable 集合里，
     * 所以这里还要确认它确实挂在 ready queue 中。 */
    if (task_is_in_runnable_queue(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 最后再收紧状态字段，只有显式标记为 TASK_RUNNING 才算通过。 */
    if (task->state != TASK_RUNNING)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 三层条件都满足后，才能把该任务当作“当前正在运行的任务”继续处理。 */
    return OS_STATUS_OK;
}

/**
 * @brief 判断任务初始化配置是否完整且合法。
 *
 * @param config 待检查的任务初始化配置。
 * @param allow_idle_priority 非 0 表示允许使用保留给 idle 的最低优先级。
 *
 * @return os_status_t 配置合法返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_validate_init_config(const task_init_config_t *config, uint8_t allow_idle_priority)
{
    /* 配置结构本身不能为空。 */
    if (config == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 没有入口函数就无法构造可执行任务。 */
    if (config->entry == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 任务栈必须由调用方提供。 */
    if (config->stack_base == NULL)
    {
        return OS_STATUS_INVALID_STACK;
    }

    /* 栈深度至少要满足端口层构造初始异常栈帧的最低需求。 */
    if (config->stack_size < OS_TASK_MIN_STACK_DEPTH)
    {
        return OS_STATUS_INVALID_STACK;
    }

    /* 首先检查 priority 是否落在 ready bitmap/ready list 支持的总范围内。 */
    if (ready_queue_priority_is_valid(config->priority) == 0U)
    {
        return OS_STATUS_INVALID_PRIORITY;
    }

    /* 普通任务不允许占用保留给 idle 的最低优先级；
     * 只有内核内部创建 idle 时，才会把 allow_idle_priority 置 1。 */
    if ((allow_idle_priority == 0U) && (task_user_priority_is_valid(config->priority) == 0U))
    {
        return OS_STATUS_INVALID_PRIORITY;
    }

    /* 所有初始化前置约束都满足后，配置才算可用。 */
    return OS_STATUS_OK;
}

/**
 * @brief 规范化任务时间片配置，若用户未设置则回退到默认值。
 *
 * @param time_slice 用户配置的时间片值。
 *
 * @return uint8_t 实际写入任务控制块的时间片值。
 */
static uint8_t task_normalize_time_slice(uint8_t time_slice)
{
    /* 调用方传 0 的约定含义是“使用系统默认时间片”，
     * 而不是“关闭时间片轮转”。 */
    if (time_slice == 0U)
    {
        return (uint8_t)OS_TASK_DEFAULT_TIME_SLICE;
    }

    /* 其余非 0 值按调用方原样使用。 */
    return time_slice;
}

/**
 * @brief idle 任务入口函数。
 *
 * @param param 预留参数，当前未使用。
 */
static void task_idle_entry(void *param)
{
    /* idle 任务当前不消费任何参数，显式吞掉它以消除未使用告警。 */
    (void)param;

    /* idle 任务的职责只是保证“系统始终有一个合法 runnable 目标”，
     * 因此这里保持最简单的永久空转。 */
    while (1)
    {
    }
}

/**
 * @brief 创建内核自带的 idle 任务。
 *
 * @return os_status_t 创建成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_create_idle_task(void)
{
    os_status_t status = OS_STATUS_OK;
    /* idle 任务由内核私有 TCB 和私有栈承载，不依赖应用层提供对象。 */
    task_init_config_t idle_config = {
        .entry = task_idle_entry,
        .param = NULL,
        .stack_base = g_idle_task_stack,
        .stack_size = OS_IDLE_TASK_STACK_DEPTH,
        .name = OS_IDLE_TASK_NAME,
        .priority = OS_IDLE_TASK_PRIORITY,
        .time_slice = 0U,
    };

    /* idle 需要合法占用保留的最低优先级，所以这里放开 idle priority 校验。 */
    status = task_init(&g_idle_task, &idle_config, 1U);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 初始化完成后，把 idle 正式放进 runnable 集合，
     * 这样所有普通任务都阻塞时，调度器仍然能选到它。 */
    ready_queue_insert_tail(&g_task_ready_queue, &g_idle_task);
    if (g_idle_task.sched_node.owner != &g_task_ready_queue.ready_lists[OS_IDLE_TASK_PRIORITY])
    {
        /* 若 idle 都无法插入 ready queue，就意味着内核基础状态已损坏，
         * 这里直接把它标为 DELETED 并返回错误。 */
        g_idle_task.state = TASK_DELETED;
        return OS_STATUS_INSERT_FAILED;
    }

    /* 走到这里说明 idle 已经成为系统最底优先级的保底 runnable 任务。 */
    return OS_STATUS_OK;
}

/**
 * @brief 根据配置初始化一个任务控制块，但不将其加入全局可运行任务集合。
 *
 * @param task 待初始化的任务控制块。
 * @param config 任务初始化配置。
 * @param allow_idle_priority 非 0 表示允许使用保留给 idle 的最低优先级。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_init(tcb_t *task, const task_init_config_t *config, uint8_t allow_idle_priority)
{
    os_status_t status = OS_STATUS_OK;
    uint8_t time_slice = 0U;
    uint32_t *initial_sp = NULL;

    /* task 指针为空时，调用方甚至没有提供可写的 TCB 存储区。 */
    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 先做纯配置检查，避免把非法参数继续传给端口层。 */
    status = task_validate_init_config(config, allow_idle_priority);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 先归一化时间片配置，再让端口层按 Cortex-M3 规则伪造初始栈帧。 */
    time_slice = task_normalize_time_slice(config->time_slice);
    task_fill_stack_pattern(config->stack_base, config->stack_size);
    initial_sp = os_port_task_stack_init(config->stack_base, config->stack_size, config->entry, config->param);
    if (initial_sp == NULL)
    {
        return OS_STATUS_INVALID_STACK;
    }

    /* 用全量清零消掉旧 TCB 内容，避免历史脏数据污染本轮创建。 */
    (void)memset(task, 0, sizeof(tcb_t));

    /* 下面这组赋值定义了任务的“初始运行上下文”和“等待元数据初值”。 */
    task->sp = initial_sp;
    task->stack_base = config->stack_base;
    task->stack_size = config->stack_size;
    task->entry = config->entry;
    task->param = config->param;
    task->name = config->name;
    task->magic = OS_TASK_MAGIC;
    task->wake_tick = 0U;
    task->wait_obj = NULL;
    task->wait_result = TASK_WAIT_RESULT_NONE;
    task->wait_cleanup_locked = NULL;
    task->base_priority = config->priority;
    task->priority = config->priority;
    task->state = TASK_BLOCKED;
    task->time_slice = time_slice;
    task->time_slice_reload = time_slice;

    /* 两个链表节点都必须从“未挂链”状态起步，
     * 后续才能安全插入 ready queue、timed-wait 或事件等待链表。 */
    list_node_init(&task->sched_node);
    list_node_init(&task->event_node);
    list_init(&task->owned_mutex_list);

    /* 这里只完成“静态初始化”，并没有把任务加入 runnable 集合。 */
    return OS_STATUS_OK;
}

/**
 * @brief 初始化任务系统全局调度状态。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK。
 */
os_status_t task_system_init(void)
{
    os_status_t status = OS_STATUS_OK;

    /* 任务系统只需要初始化一次，重复调用直接返回成功即可。 */
    if (g_task_system_initialized != 0U)
    {
        return OS_STATUS_OK;
    }

    /* 先建立空的 ready queue 和 timed-wait 链表，再清空全局调度指针与时基。 */
    ready_queue_init(&g_task_ready_queue);
    list_init(&g_task_timed_wait_list);
    g_os_tick = 0U;
    g_current_task = NULL;
    g_next_task = NULL;

    /* 没有 idle 的话，所有普通任务都阻塞时系统将失去合法 runnable 目标。 */
    status = task_create_idle_task();
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 到这里为止，调度器的最小全局状态已经建立完成。 */
    g_task_system_initialized = 1U;
    return OS_STATUS_OK;
}

/**
 * @brief 初始化一个任务控制块，并将任务加入全局可运行任务集合。
 *
 * @param task 调用方提供、当前未被调度器持有的任务控制块存储区。
 * @param config 任务初始化配置。
 *
 * @return os_status_t 创建成功且无需立刻抢占时返回 OS_STATUS_OK；
 *                     若创建成功且新任务应立即抢占，则返回 OS_STATUS_SWITCH_REQUIRED；
 *                     失败时返回具体错误码。
 */
os_status_t task_create(tcb_t *task, const task_init_config_t *config)
{
    os_status_t status = OS_STATUS_OK;
    uint8_t scheduler_running = 0U;
    uint32_t primask = 0U;

    /* 创建接口要求调用方明确给出一个可写 TCB。 */
    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 首次创建普通任务时，顺手把调度器和 idle 一起初始化起来。 */
    if (g_task_system_initialized == 0U)
    {
        status = task_system_init();
        if (status != OS_STATUS_OK)
        {
            return status;
        }
    }

    /* 同一个 TCB 不允许重复交给调度器，否则会破坏 ready queue/timed-wait 结构。 */
    if (task_is_known_to_scheduler(task) != 0U)
    {
        return OS_STATUS_ALREADY_INITIALIZED;
    }

    /* 先完成 TCB 和初始栈的静态构造。 */
    status = task_init(task, config, 0U);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 运行期创建任务时，SysTick 也可能在中断态访问同一组 ready list 和 g_next_task。
     * 因此“插入 ready queue + 重新调度”这一段必须作为一个原子步骤完成。 */
    primask = os_port_enter_critical();

    /* 静态初始化成功后，再把任务正式并入 runnable 集合。 */
    ready_queue_insert_tail(&g_task_ready_queue, task);
    if (task->sched_node.owner != &g_task_ready_queue.ready_lists[task->priority])
    {
        /* 若 sched_node 没有正确挂到目标 ready list，就视为插入失败。 */
        task->state = TASK_DELETED;
        os_port_exit_critical(primask);
        return OS_STATUS_INSERT_FAILED;
    }

    /* 用 current 是否已存在，区分“系统已运行中”和“还没切入任何任务”的场景。
     * 这一步也放在临界区里，避免读取到被并发修改中的 current/next 关系。 */
    scheduler_running = (uint8_t)(g_current_task != NULL);

    /* 新任务入队后立即重新调度，确保 g_next_task 反映最新 runnable 集合。 */
    status = task_schedule();

    /* 到这里为止，ready list 指针与 g_next_task 都已经稳定，可以安全恢复中断。 */
    os_port_exit_critical(primask);

    /* 调度器明确表示“当前任务继续运行”时，创建接口按普通成功返回。 */
    if (status == OS_STATUS_NO_CHANGE)
    {
        return OS_STATUS_OK;
    }

    /* 还没启动首任务时，schedule() 返回 SWITCH_REQUIRED 是正常现象，
     * 这不应该被误报成“运行期发生抢占”。 */
    if ((status == OS_STATUS_SWITCH_REQUIRED) && (scheduler_running == 0U))
    {
        return OS_STATUS_OK;
    }

    /* 其余情况原样向上传递，尤其是运行期创建高优先级任务时的抢占需求。 */
    return status;
}

/**
 * @brief 在临界区内把目标任务从调度器当前持有的所有结构中摘掉。
 *
 * @param task 待脱离调度器持有关系的任务对象。
 *
 * @return os_status_t 摘除成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_detach_from_scheduler_locked(tcb_t *task)
{
    /* 非法 TCB 不允许继续访问链表归属信息。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* sched_node 若在 ready queue 中，就用 ready_queue_remove() 摘掉，
     * 这样 ready bitmap 也会一起被维护。 */
    if (task_is_in_runnable_queue(task) != 0U)
    {
        ready_queue_remove(&g_task_ready_queue, task);
        if (task->sched_node.owner != NULL)
        {
            return OS_STATUS_INVALID_STATE;
        }
    }
    else if (task_is_in_timed_wait_list(task) != 0U)
    {
        /* sched_node 若在 timed-wait 链表中，则直接从 timed-wait 摘掉即可。 */
        if (list_remove(&g_task_timed_wait_list, &task->sched_node) == 0U)
        {
            return OS_STATUS_INVALID_STATE;
        }
    }
    else if ((task->sched_node.owner != NULL) && (task != g_current_task))
    {
        /* 按当前设计，非 current 任务的 sched_node 只允许出现在 ready/timed-wait。
         * 若 owner 指向别的链表，说明调度器内部状态已经失配。 */
        return OS_STATUS_INVALID_STATE;
    }

    /* event_node 若当前挂在某个对象等待链表上，也必须同步摘掉，
     * 否则对象满足时仍可能错误地把已删任务重新唤醒。 */
    if (task->event_node.owner != NULL)
    {
        if (list_remove(task->event_node.owner, &task->event_node) == 0U)
        {
            return OS_STATUS_INVALID_STATE;
        }
    }

    return OS_STATUS_OK;
}

/**
 * @brief 在临界区内把任务状态推进为 DELETED，并清理等待元数据。
 *
 * @param task 待标记为删除态的任务对象。
 *
 * @return os_status_t 标记成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_mark_deleted_locked(tcb_t *task)
{
    os_status_t status = OS_STATUS_OK;

    /* 只有合法 TCB 才允许继续走删除态清理。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 先把任务从 ready/timed-wait/对象等待链表里统一摘掉。 */
    status = task_detach_from_scheduler_locked(task);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 若被删任务原本正在等待某个对象，且对象登记了 waiter cleanup hook，
     * 这里要在“event_node 已摘掉”之后立刻回调，让对象有机会刷新 owner 继承优先级。 */
    task_wait_cleanup_invoke_locked(task);

    /* 删除后不清 magic，让同一 TCB 在未来可被重新 task_create() 复用；
     * 但调度器持有关系和等待元数据都必须彻底清空。 */
    task->state = TASK_DELETED;
    task->wait_obj = NULL;
    task->wake_tick = 0U;
    task->wait_result = TASK_WAIT_RESULT_NONE;
    task->wait_cleanup_locked = NULL;
    task->time_slice = task->time_slice_reload;

    return OS_STATUS_OK;
}

/**
 * @brief 删除一个任务对象，并在需要时触发重新调度。
 *
 * @param task 待删除的任务对象。
 *
 * @return os_status_t 删除成功返回 OS_STATUS_OK；失败时返回具体错误码。
 */
os_status_t task_delete(tcb_t *task)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t primask = 0U;
    uint8_t scheduler_running = 0U;

    /* 删除接口要求调用方明确给出目标任务对象。 */
    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 任务系统尚未初始化时，不存在可删除的内核任务对象。 */
    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    /* 删除路径只允许在线程态调用，ISR 中不负责管理任务生命周期。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 非法 TCB 无法安全参与删除流程。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* idle 是系统最后的保底 runnable 目标，任何路径都不允许删除。 */
    if (task == &g_idle_task)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 第一版 mutex 明确禁止“持锁任务被删除”，否则 owner、waiter 和继承优先级恢复都会失配。 */
    if (list_is_empty(&task->owned_mutex_list) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 已经是 DELETED 的对象不允许再次删除。 */
    if (task->state == TASK_DELETED)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 只有当前仍被调度器持有的对象，才允许进入删除流程。 */
    if (task_is_known_to_scheduler(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 当前任务自删语义固定为 no-return，由专门 helper 负责完成。 */
    if (task == g_current_task)
    {
        task_exit_current();

        /* 正常情况下不会走到这里；若 helper 意外返回，就按错误兜底。 */
        return OS_STATUS_INVALID_STATE;
    }

    primask = os_port_enter_critical();

    /* 删除 non-current 任务时，先统一做脱链和删除态清理。 */
    status = task_mark_deleted_locked(task);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 若当前已经有 current task，说明这是运行期删除；后续 schedule() 若要求切换，
     * 需要在恢复中断前先提交 PendSV。 */
    scheduler_running = (uint8_t)(g_current_task != NULL);

    status = task_schedule();
    if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 只有运行期删除 non-current 任务且调度器明确要求切换时，
     * 才在恢复中断前先 pend PendSV。 */
    if ((status == OS_STATUS_SWITCH_REQUIRED) && (scheduler_running != 0U))
    {
        os_port_trigger_pendsv();
    }

    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 从全局可运行任务集合中挑选下一个应运行的任务。
 *
 * @return os_status_t 选择成功返回 OS_STATUS_OK；若无可运行任务则返回 OS_STATUS_EMPTY。
 */
static os_status_t task_select_next(void)
{
    const tcb_t *selected_task = NULL;

    /* 调度器全局状态尚未建立时，没有 ready queue 可供选择 next task。 */
    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    /* 最高优先级 ready list 的头节点就是当前应当运行的任务。 */
    selected_task = ready_queue_peek_highest(&g_task_ready_queue);
    if (selected_task == NULL)
    {
        /* runnable 集合为空时，要同步清掉 g_next_task，避免残留旧指针。 */
        g_next_task = NULL;
        return OS_STATUS_EMPTY;
    }

    /* 注意：这里只负责“选出谁该跑”，不负责真正提交 current task。 */
    g_next_task = (tcb_t *)selected_task;
    return OS_STATUS_OK;
}

/**
 * @brief 执行一次调度决策，给出当前是否需要触发上下文切换。
 *
 * @return os_status_t 需要切换返回 OS_STATUS_SWITCH_REQUIRED；无需切换返回
 *                     OS_STATUS_NO_CHANGE；失败时返回具体错误码。
 */
os_status_t task_schedule(void)
{
    os_status_t status = OS_STATUS_OK;

    /* 每次 schedule 都先刷新 g_next_task，让它反映最新 runnable 集合。 */
    status = task_select_next();
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* current 为空表示系统还没真正切进任何线程，但 next 已经选出来了；
     * 这正是“需要启动首任务”的典型状态。 */
    if (g_current_task == NULL)
    {
        return OS_STATUS_SWITCH_REQUIRED;
    }

    /* 只要 next 和 current 不同，就说明这次调度需要真正发生上下文切换。 */
    if (g_next_task != g_current_task)
    {
        return OS_STATUS_SWITCH_REQUIRED;
    }

    /* 走到这里说明当前任务继续运行即可，不需要切换。 */
    return OS_STATUS_NO_CHANGE;
}

/**
 * @brief 让当前任务主动让出 CPU，供同优先级任务轮转运行。
 *
 * @return os_status_t 返回本次让出后得到的调度决策结果。
 */
os_status_t task_yield(void)
{
    os_status_t status = OS_STATUS_OK;
    list_t *ready_list = NULL;
    uint32_t primask = 0U;

    /* yield 只允许线程态当前任务主动调用，ISR 没有“当前线程自愿让出 CPU”的语义。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 后续会同时读取 current task、ready list 并可能做 rotate，因此先进入临界区。 */
    primask = os_port_enter_critical();

    /* 先确认当前任务确实还是一个合法 RUNNING 任务。 */
    status = task_validate_running_task(g_current_task);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 轮转只会发生在“当前任务所属优先级”的那条 ready list 内。 */
    ready_list = task_get_ready_list_by_priority(g_current_task->priority);
    if (ready_list == NULL)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_INVALID_STATE;
    }

    /* 主动 yield 等价于放弃本轮剩余时间片，所以这里先恢复到 reload 值。 */
    g_current_task->time_slice = g_current_task->time_slice_reload;

    /* 只有当前任务确实位于本优先级队头，且同优先级还有其他任务时，
     * 才需要执行一次“头移到尾”的轮转。 */
    if ((ready_list->item_count > 1U) && (ready_list->head == &g_current_task->sched_node))
    {
        ready_queue_rotate(&g_task_ready_queue, g_current_task->priority);
    }

    /* 轮转完成后重新做调度决策，决定是否需要切到同优先级下一个任务。 */
    status = task_schedule();
    os_port_exit_critical(primask);

    return status;
}

/**
 * @brief 判断一个相对超时值是否落在“半个 tick 空间”内。
 *
 * @param timeout_ticks 调用方传入的相对超时 tick 数。
 *
 * @return uint8_t 非 0 表示该超时值可安全参与当前的回绕比较逻辑，
 *                 0 表示该值会破坏有符号差值比较的前提。
 */
static uint8_t task_timeout_is_supported(os_tick_t timeout_ticks)
{
    /* OS_WAIT_FOREVER 是一个单独语义，不参与 deadline 的数值比较，
     * 因此这里直接视为“允许”。 */
    if (timeout_ticks == OS_WAIT_FOREVER)
    {
        return 1U;
    }

    /* 当前 timed-wait 链表和“是否到期”的判断，都依赖 int32_t 差值比较。
     * 这套比较只在“当前 tick 与目标 deadline 的距离严格小于 0x80000000”
     * 时才成立，所以必须拒绝 0x80000000 及以上的超时值。 */
    return (uint8_t)((timeout_ticks > 0U) && (timeout_ticks < OS_TICK_COMPARE_HALF_RANGE));
}

/**
 * @brief 判断一个 deadline 是否已经到期。
 *
 * @param current_tick 当前绝对 tick。
 * @param target_tick 目标到期 tick。
 *
 * @return uint8_t 非 0 表示 deadline 已到期，0 表示尚未到期。
 */
static uint8_t task_tick_is_due(os_tick_t current_tick, os_tick_t target_tick)
{
    /* 这里故意使用 int32_t 差值比较来获得“32-bit tick 回绕安全”的到期判断。
     * 前提是 current_tick 与 target_tick 的距离始终落在半个 tick 空间内，
     * 调用方必须保证 timeout_ticks < 0x80000000。 */
    return (uint8_t)(((int32_t)(current_tick - target_tick)) >= 0);
}

/**
 * @brief 判断两个未来 deadline 的先后顺序。
 *
 * @param lhs 待比较的左操作数 deadline。
 * @param rhs 待比较的右操作数 deadline。
 *
 * @return uint8_t 非 0 表示 lhs 早于 rhs，0 表示 lhs 不早于 rhs。
 */
static uint8_t task_tick_deadline_is_before(os_tick_t lhs, os_tick_t rhs)
{
    /* timed-wait 链表按 deadline 升序维护时，也复用同一套半区间比较规则。
     * 只要所有插入的 deadline 都来自“受限的相对超时值”，这里的排序就稳定。 */
    return (uint8_t)(((int32_t)(lhs - rhs)) < 0);
}

/**
 * @brief 将任务按 wake_tick 升序插入 timed-wait 链表。
 *
 * @param task 待插入的任务控制块。
 */
static void timed_wait_list_insert_ordered(tcb_t *task)
{
    list_node_t *current = NULL;
    tcb_t *current_task = NULL;
    list_node_t *node = NULL;

    /* 空任务指针说明调用方没有提供合法对象，直接拒绝插入。 */
    if (task == NULL)
    {
        return;
    }

    node = &task->sched_node;

    /* timed-wait 插入要求 sched_node 目前必须是“未挂链”状态；
     * 若 owner 非空，说明这个任务还停留在其他链表中，不能重复挂入。 */
    if (node->owner != NULL)
    {
        return;
    }

    /* 空链表是最简单的情况，首个等待任务直接成为新的队头和队尾。 */
    if (g_task_timed_wait_list.head == NULL)
    {
        (void)list_insert_tail(&g_task_timed_wait_list, node);
        return;
    }

    /* 非空链表按 wake_tick 升序扫描，找到第一个 deadline 晚于当前任务的位置，
     * 这样可以保证“最早到期任务”始终待在队头。 */
    current = g_task_timed_wait_list.head;
    while (current != NULL)
    {
        current_task = LIST_CONTAINER_OF(current, tcb_t, sched_node);
        if (task_tick_deadline_is_before(task->wake_tick, current_task->wake_tick) != 0U)
        {
            /* 找到插入点后，手工完成双向链表指针修补：
             * 当前任务插在 current 前面，并把 owner 标记成 timed-wait 链表。 */
            node->prev = current->prev;
            node->next = current;
            node->owner = &g_task_timed_wait_list;

            /* 若 current 不是原队头，就只需要把“前驱 -> 新节点”接起来；
             * 否则当前任务会成为新的队头。 */
            if (current->prev != NULL)
            {
                current->prev->next = node;
            }
            else
            {
                g_task_timed_wait_list.head = node;
            }

            /* 最后把 current 的前驱改成新节点，并补齐 item_count。 */
            current->prev = node;
            g_task_timed_wait_list.item_count++;
            return;
        }

        /* 当前位置 deadline 不晚于新任务，继续向后找插入点。 */
        current = current->next;
    }

    /* 扫到链尾仍未找到更晚的 deadline，说明新任务是“当前最晚到期”的，
     * 直接尾插即可，同时还能保持同一 wake_tick 下的 FIFO 语义。 */
    (void)list_insert_tail(&g_task_timed_wait_list, node);
}

/**
 * @brief 在临界区内把一个等待中的任务重新放回 runnable 集合。
 *
 * @param task 待恢复运行的任务控制块。
 * @param wait_result 本次恢复运行应写入的等待结果。
 *
 * @return os_status_t 恢复成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_make_runnable_locked(tcb_t *task, task_wait_result_t wait_result)
{
    /* 被唤醒的对象首先必须是合法 TCB。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 只有真正处于等待态的任务，才允许走“恢复 runnable”路径。 */
    if ((task->state != TASK_BLOCKED) && (task->state != TASK_SLEEPING))
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 先从 timed-wait 链表摘掉 sched_node。
     * 对纯事件唤醒且没有 timeout 的任务，这一步会自然跳过。 */
    if (task_is_in_timed_wait_list(task) != 0U)
    {
        (void)list_remove(&g_task_timed_wait_list, &task->sched_node);
    }

    /* 再从对象等待链表摘掉 event_node。
     * 这样无论是“事件先到”还是“超时先到”，都不会留下重复唤醒入口。 */
    if (task->event_node.owner != NULL)
    {
        (void)list_remove(task->event_node.owner, &task->event_node);
    }

    /* timeout 唤醒 blocked 任务时，要在 event_node 已摘掉之后调用对象清理回调，
     * 让 mutex 之类的对象及时刷新 owner 的继承优先级。 */
    if ((task->state == TASK_BLOCKED) && (wait_result == TASK_WAIT_RESULT_TIMEOUT))
    {
        task_wait_cleanup_invoke_locked(task);
    }

    /* 等待结束后，把等待相关元数据全部清理掉，
     * 并把时间片恢复到初始值，等待再次运行时重新消费。 */
    task->wake_tick = 0U;
    task->wait_obj = NULL;
    task->wait_result = wait_result;
    task->wait_cleanup_locked = NULL;
    task->time_slice = task->time_slice_reload;

    /* 最后按普通 runnable 任务重新尾插回 ready queue，
     * 这样同优先级下不会破坏原有 FIFO/时间片轮转顺序。 */
    ready_queue_insert_tail(&g_task_ready_queue, task);
    if (task->sched_node.owner != &g_task_ready_queue.ready_lists[task->priority])
    {
        task->state = TASK_DELETED;
        return OS_STATUS_INSERT_FAILED;
    }

    return OS_STATUS_OK;
}

/**
 * @brief 在临界区内唤醒一个等待中的任务，并给出新的调度决策。
 *
 * @param task 待唤醒的任务控制块。
 * @param wait_result 本次唤醒应写入的等待结果。
 *
 * @return os_status_t 返回唤醒后得到的调度决策结果。
 */
static os_status_t task_unblock_locked(tcb_t *task, task_wait_result_t wait_result)
{
    os_status_t status = OS_STATUS_OK;

    status = task_make_runnable_locked(task, wait_result);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    return task_schedule();
}

/**
 * @brief 在临界区内把当前运行任务迁移到 sleeping/blocking 等待状态。
 *
 * @param task 当前待迁移的任务控制块。
 * @param wait_state 目标等待状态，只允许 TASK_SLEEPING 或 TASK_BLOCKED。
 * @param wait_obj 当前等待的对象指针。
 * @param timeout_ticks 若需要超时唤醒，给出相对 tick；传 OS_WAIT_FOREVER 表示不挂超时。
 *
 * @return os_status_t 返回迁移后的调度决策结果。
 */
static os_status_t task_prepare_wait_locked(tcb_t *task,
                                            task_state_t wait_state,
                                            void *wait_obj,
                                            os_tick_t timeout_ticks,
                                            task_wait_cleanup_fn_t wait_cleanup_locked)
{
    os_status_t status = OS_STATUS_OK;

    /* 进入等待路径前，先确认调用方确实是一个仍在 runnable 集合里的
     * 当前运行任务；否则后续“从 ready queue 摘链”的前提根本不成立。 */
    status = task_validate_running_task(task);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* idle 任务是整个系统在“无普通任务 runnable”时的最后兜底，
     * 绝不能把它自己送进 sleeping/blocking。 */
    if (task == &g_idle_task)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 当前内部等待迁移逻辑只支持两种非 runnable 状态：
     * 主动延时进入 TASK_SLEEPING，或等待对象进入 TASK_BLOCKED。 */
    if ((wait_state != TASK_SLEEPING) && (wait_state != TASK_BLOCKED))
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* TASK_SLEEPING 必须对应一个有限的相对延时值；
     * 若把 OS_WAIT_FOREVER 传进来，任务会离开 runnable 集合后再也没有唤醒来源。 */
    if ((wait_state == TASK_SLEEPING) && (timeout_ticks == OS_WAIT_FOREVER))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 这里显式收紧 timeout 的取值范围：
     * 1. OS_WAIT_FOREVER 允许存在，但它不参与 deadline 比较；
     * 2. 其余 timeout 必须是正数，且严格小于 0x80000000，
     *    否则会破坏后续 int32_t 差值比较的正确性。 */
    if (task_timeout_is_supported(timeout_ticks) == 0U)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 只要走 TASK_BLOCKED 路径，任务就必须已经挂进某个对象等待链表。
     * 当前实现里 wait_obj 本身并不会被用来反查 waiter，所以若 event_node
     * 还没真正挂链，那么这个阻塞最终只可能靠 timeout 返回，无法被对象满足路径唤醒。
     * 这种“看起来像在等对象，实际上只能超时”的状态必须在入口直接拒绝。 */
    if ((wait_state == TASK_BLOCKED) && (task->event_node.owner == NULL))
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 真正进入等待态的第一步，是把当前任务从 runnable 集合摘掉。
     * 从这一刻开始，它就不应再被 task_schedule() 选为 next task。 */
    ready_queue_remove(&g_task_ready_queue, task);
    if (task->sched_node.owner != NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 摘链成功后，马上把任务元数据切到等待语义：
     * state 表示等待类型，wait_obj 指向等待对象，wait_result 清空为“尚无结果”，
     * time_slice 复位到 reload，供任务将来再次被调度时重新消费完整时间片。 */
    task->state = wait_state;
    task->wait_obj = wait_obj;
    task->wait_result = TASK_WAIT_RESULT_NONE;
    task->wait_cleanup_locked = wait_cleanup_locked;
    task->time_slice = task->time_slice_reload;

    if (timeout_ticks != OS_WAIT_FOREVER)
    {
        /* 带 timeout 的等待，需要先把相对超时换算成绝对 wake_tick，
         * 再按 deadline 顺序插进 timed-wait 链表。 */
        task->wake_tick = (os_tick_t)(g_os_tick + timeout_ticks);
        timed_wait_list_insert_ordered(task);

        /* 若插入后 owner 没有落到 timed-wait 链表，说明挂链失败；
         * 这时必须立即返回错误，避免任务处于“既不 runnable、也不在 wait list”的丢失状态。 */
        if (task->sched_node.owner != &g_task_timed_wait_list)
        {
            return OS_STATUS_INSERT_FAILED;
        }
    }
    else
    {
        /* 永久等待不参与 timed-wait 排序，因此 wake_tick 明确清零，
         * 后续只能依赖对象满足路径来调用 task_unblock()。 */
        task->wake_tick = 0U;
    }

    /* 最后重新跑一次调度，让 g_next_task 反映“当前任务已离开 runnable 集合”后的结果。 */
    return task_schedule();
}

/**
 * @brief 在临界区内批量唤醒所有已经到期的 timed-wait 任务。
 *
 * @return os_status_t 若唤醒后需要切换，则返回 OS_STATUS_SWITCH_REQUIRED；
 *                     若无需切换，则返回 OS_STATUS_NO_CHANGE；
 *                     出错时返回具体错误码。
 */
static os_status_t task_wake_timed_tasks_locked(void)
{
    list_node_t *node = NULL;
    tcb_t *task = NULL;
    uint8_t woke_any_task = 0U;
    os_status_t status = OS_STATUS_OK;

    /* timed-wait 链表始终按 wake_tick 升序维护，所以只要队头还没到期，
     * 后面的任务也一定不应该在本次 tick 被唤醒。 */
    node = g_task_timed_wait_list.head;
    while (node != NULL)
    {
        task = LIST_CONTAINER_OF(node, tcb_t, sched_node);

        /* 队头 deadline 尚未到期时，当前 tick 的超时处理可以立即结束。 */
        if (task_tick_is_due(g_os_tick, task->wake_tick) == 0U)
        {
            break;
        }

        /* 已到期任务统一走“重新放回 runnable 集合”的公共路径。
         * 其中 BLOCKED 任务写入 TIMEOUT 结果，SLEEPING 任务保持 NONE 即可。 */
        status = task_make_runnable_locked(task,
                                           (task->state == TASK_BLOCKED) ? TASK_WAIT_RESULT_TIMEOUT
                                                                         : TASK_WAIT_RESULT_NONE);
        if (status != OS_STATUS_OK)
        {
            return status;
        }

        /* 只要本轮至少唤醒过一个任务，最后就要重新跑一次全局调度；
         * 另外由于刚刚移除了原队头，所以这里重新从新队头开始检查。 */
        woke_any_task = 1U;
        node = g_task_timed_wait_list.head;
    }

    /* 一个任务都没唤醒时，说明本次 tick 没有因为 timeout 改变 runnable 集合。 */
    if (woke_any_task == 0U)
    {
        return OS_STATUS_NO_CHANGE;
    }

    /* 有任务超时返回时，必须重新计算 next task，
     * 这样更高优先级任务才能在本次 tick 后立即触发抢占。 */
    return task_schedule();
}

/**
 * @brief 在临界区内推进当前运行任务的时间片计数，并在量子耗尽时执行同优先级轮转。
 *
 * @return os_status_t 返回本次时间片处理得到的调度决策结果。
 */
static os_status_t task_handle_time_slice_tick_locked(void)
{
    os_status_t status = OS_STATUS_OK;
    list_t *ready_list = NULL;
    tcb_t *current_task = g_current_task;

    /* 若调度器尚未建立，时间片逻辑没有语义基础。 */
    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    /* 尚未切入任何任务时，本次 tick 不需要做时间片处理。 */
    if (current_task == NULL)
    {
        return OS_STATUS_NO_CHANGE;
    }

    /* 时间片只能由真实 RUNNING 且仍在 runnable 集合中的 current task 消费。 */
    status = task_validate_running_task(current_task);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 先拿到当前优先级对应的 ready list，后面同优先级轮转只会作用在这一条链表上。 */
    ready_list = task_get_ready_list_by_priority(current_task->priority);
    if (ready_list == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 当前优先级只有自己一个 runnable 任务时，时间片轮转没有意义；
     * 这里直接把剩余时间片恢复到 reload，避免单任务场景一直耗尽到 0。 */
    if (ready_list->item_count <= 1U)
    {
        current_task->time_slice = current_task->time_slice_reload;
        /* 即使本优先级没有可轮转对象，也仍要在本 tick 末尾重新计算全局调度结果，
         * 这样刚刚被唤醒的更高优先级任务才能在本次 SysTick 后立刻抢占。 */
        return task_schedule();
    }

    /* 只有时间片还大于 0 时才递减，防止无符号计数下溢。 */
    if (current_task->time_slice > 0U)
    {
        current_task->time_slice--;
    }

    /* 时间片尚未用完时，当前任务本优先级内不需要 rotate；
     * 但本 tick 仍可能因为更高优先级任务刚刚醒来而需要切换。 */
    if (current_task->time_slice > 0U)
    {
        return task_schedule();
    }

    /* 时间片耗尽后，先恢复 reload 值，供该任务下一轮再次被调度时使用。 */
    current_task->time_slice = current_task->time_slice_reload;

    /* 轮转动作只在“当前任务仍是本优先级队头”时执行；
     * 这样可以确保 rotate 的确是把当前运行者移到队尾。 */
    if (ready_list->head == &current_task->sched_node)
    {
        ready_queue_rotate(&g_task_ready_queue, current_task->priority);
    }

    /* 轮转完成后再做一次调度，综合决定：
     * 1. 是否切到刚醒来的更高优先级任务；
     * 2. 若无更高优先级任务，是否切到同优先级下一个时间片接班者。 */
    return task_schedule();
}

/**
 * @brief 对外暴露的 RTOS 节拍推进入口。
 *
 * @return os_status_t 返回本次 tick 处理得到的调度决策结果。
 */
os_status_t task_system_tick(void)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t primask = 0U;

    /* 节拍处理依赖 ready queue、timed-wait 链表和 current task 等全局状态，
     * 所以调度器未初始化时不能进入这条路径。 */
    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    /* SysTick 和线程态 API 都会改动同一批任务链表，这里先进入最小临界区，
     * 保证“tick 递增 + timeout 唤醒 + 时间片决策”作为一个原子步骤完成。 */
    primask = os_port_enter_critical();

    /* 先推进绝对时基，再基于新的 current tick 处理到期任务。 */
    g_os_tick++;

    /* 先处理 timeout，让所有“本 tick 到期”的任务回到 runnable 集合。
     * 但这里先不急着返回 SWITCH_REQUIRED，因为当前正在运行的任务
     * 也必须先为“已经跑完的这个 tick”结算时间片。 */
    status = task_wake_timed_tasks_locked();
    if ((status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
    {
        os_port_exit_critical(primask);
        return status;
    }

#if (OS_TASK_STACK_CHECK_ENABLE != 0U)
    if ((g_current_task != NULL) && (task_stack_sentinel_is_intact(g_current_task) == 0U))
    {
        os_port_exit_critical(primask);
        os_panic(OS_PANIC_STACK_SENTINEL, __FILE__, (uint32_t)__LINE__);
    }
#endif

    /* 无论 timeout 是否已经让更高优先级任务变成 runnable，
     * 当前任务都已经完整消耗了这一个 tick，所以这里仍要继续结算时间片。 */
    status = task_handle_time_slice_tick_locked();
    os_port_exit_critical(primask);

    return status;
}

/**
 * @brief 获取当前绝对 tick 值。
 *
 * @return os_tick_t 当前全局 tick 值。
 */
os_tick_t os_tick_get(void)
{
    return g_os_tick;
}

/**
 * @brief 以稳定 public 语义名字创建一个任务。
 *
 * @param task 调用方提供的静态任务对象。
 * @param config 任务初始化配置。
 *
 * @return os_status_t 创建结果与 task_create() 保持一致。
 */
os_status_t os_task_create(os_task_t *task, const os_task_config_t *config)
{
    return task_create(task, config);
}

/**
 * @brief 以稳定 public 语义名字删除一个任务。
 *
 * @param task 待删除的任务对象。
 *
 * @return os_status_t 删除结果与 task_delete() 保持一致。
 */
os_status_t os_task_delete(os_task_t *task)
{
    return task_delete(task);
}

/**
 * @brief 以稳定 public 语义名字主动让出 CPU。
 *
 * @return os_status_t 让出结果与 task_yield() 保持一致。
 */
os_status_t os_task_yield(void)
{
    return task_yield();
}

/**
 * @brief 以稳定 public 语义名字让当前任务延时。
 *
 * @param delay_ticks 相对延时 tick 数。
 *
 * @return os_status_t 延时结果与 task_delay() 保持一致。
 */
os_status_t os_task_delay(os_tick_t delay_ticks)
{
    return task_delay(delay_ticks);
}

/**
 * @brief 以稳定 public 语义名字执行周期性 delay-until。
 *
 * @param previous_wake_tick 上一次唤醒基准 tick。
 * @param period_ticks 周期长度。
 *
 * @return os_status_t 结果与 task_delay_until() 保持一致。
 */
os_status_t os_task_delay_until(os_tick_t *previous_wake_tick, os_tick_t period_ticks)
{
    return task_delay_until(previous_wake_tick, period_ticks);
}

/**
 * @brief 以稳定 public 语义名字获取当前任务对象。
 *
 * @return os_task_t* 当前任务对象；若尚未切入任务则返回 NULL。
 */
os_task_t *os_task_current_get(void)
{
    return task_get_current();
}

/**
 * @brief 以稳定 public 语义名字获取任务当前生效优先级。
 *
 * @param task 目标任务对象。
 * @param priority 输出当前生效优先级。
 *
 * @return os_status_t 查询结果与 task_priority_get() 保持一致。
 */
os_status_t os_task_priority_get(const os_task_t *task, uint8_t *priority)
{
    return task_priority_get(task, priority);
}

/**
 * @brief 以稳定 public 语义名字获取任务当前 base priority。
 *
 * @param task 目标任务对象。
 * @param priority 输出当前 base priority。
 *
 * @return os_status_t 查询结果与 task_base_priority_get() 保持一致。
 */
os_status_t os_task_base_priority_get(const os_task_t *task, uint8_t *priority)
{
    return task_base_priority_get(task, priority);
}

/**
 * @brief 以稳定 public 语义名字修改任务当前 base priority。
 *
 * @param task 目标任务对象。
 * @param priority 新的 base priority。
 *
 * @return os_status_t 修改结果与 task_base_priority_set() 保持一致。
 */
os_status_t os_task_base_priority_set(os_task_t *task, uint8_t priority)
{
    return task_base_priority_set(task, priority);
}

/**
 * @brief 以稳定 public 语义名字获取任务状态。
 *
 * @param task 目标任务对象。
 * @param state 输出任务状态。
 *
 * @return os_status_t 查询结果与 task_state_get() 保持一致。
 */
os_status_t os_task_state_get(const os_task_t *task, os_task_state_t *state)
{
    return task_state_get(task, state);
}

/**
 * @brief 以稳定 public 语义名字获取任务名称。
 *
 * @param task 目标任务对象。
 * @param name 输出名称指针。
 *
 * @return os_status_t 查询结果与 task_name_get() 保持一致。
 */
os_status_t os_task_name_get(const os_task_t *task, const char **name)
{
    return task_name_get(task, name);
}

/**
 * @brief 以稳定 public 语义名字查询任务栈剩余水位。
 *
 * @param task 目标任务对象。
 * @param unused_words 输出未使用栈空间，单位为 words。
 *
 * @return os_status_t 查询结果与 task_stack_high_water_mark_get() 保持一致。
 */
os_status_t os_task_stack_high_water_mark_get(const os_task_t *task, uint32_t *unused_words)
{
    return task_stack_high_water_mark_get(task, unused_words);
}

/**
 * @brief 获取任务当前生效优先级。
 *
 * @param task 目标任务对象。
 * @param priority 输出当前生效优先级。
 *
 * @return os_status_t 查询成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_priority_get(const tcb_t *task, uint8_t *priority)
{
    uint32_t primask = 0U; // 当前只读查询路径的临界区快照

    if ((task == NULL) || (priority == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    primask = os_port_enter_critical();
    *priority = task->priority;
    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 获取任务当前配置的 base priority。
 *
 * @param task 目标任务对象。
 * @param priority 输出当前 base priority。
 *
 * @return os_status_t 查询成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_base_priority_get(const tcb_t *task, uint8_t *priority)
{
    uint32_t primask = 0U; // 当前只读查询路径的临界区快照

    if ((task == NULL) || (priority == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    primask = os_port_enter_critical();
    *priority = task->base_priority;
    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 修改任务的 base priority，并按继承规则重算当前生效优先级。
 *
 * @param task 目标任务对象。
 * @param priority 新的 base priority。
 *
 * @return os_status_t 修改成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_base_priority_set(tcb_t *task, uint8_t priority)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t    primask = 0U;          // 当前 set 路径的外层临界区
    uint8_t     scheduler_running = 0U; // 非 0 表示当前已经切入线程运行期

    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (task == &g_idle_task)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (task->state == TASK_DELETED)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (task_is_known_to_scheduler(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (task_user_priority_is_valid(priority) == 0U)
    {
        return OS_STATUS_INVALID_PRIORITY;
    }

    primask = os_port_enter_critical();
    task->base_priority = priority;

    status = task_priority_inheritance_refresh_locked(task);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    scheduler_running = (uint8_t)(g_current_task != NULL);
    status = task_schedule();
    if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
    {
        os_port_exit_critical(primask);
        return status;
    }

    if ((scheduler_running != 0U) && (status == OS_STATUS_SWITCH_REQUIRED))
    {
        os_port_trigger_pendsv();
    }

    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 获取任务当前状态。
 *
 * @param task 目标任务对象。
 * @param state 输出当前任务状态。
 *
 * @return os_status_t 查询成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_state_get(const tcb_t *task, task_state_t *state)
{
    uint32_t primask = 0U; // 当前只读查询路径的临界区快照

    if ((task == NULL) || (state == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    primask = os_port_enter_critical();
    *state = task->state;
    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 获取任务名称指针。
 *
 * @param task 目标任务对象。
 * @param name 输出任务名称指针。
 *
 * @return os_status_t 查询成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_name_get(const tcb_t *task, const char **name)
{
    uint32_t primask = 0U; // 当前只读查询路径的临界区快照

    if ((task == NULL) || (name == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    primask = os_port_enter_critical();
    *name = task->name;
    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 统计任务栈当前仍保持 fill pattern 的未使用空间。
 *
 * @param task 待查询的任务对象。
 * @param unused_words 输出当前未使用栈空间，单位为 uint32_t words。
 *
 * @return os_status_t 查询成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_stack_high_water_mark_get(const tcb_t *task, uint32_t *unused_words)
{
    uint32_t count = 0U; // 当前仍保持 fill pattern 的低地址连续 word 数

    if ((task == NULL) || (unused_words == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if ((task_is_valid(task) == 0U) || (task->stack_base == NULL))
    {
        return OS_STATUS_INVALID_STATE;
    }

    while ((count < task->stack_size) && (task->stack_base[count] == OS_TASK_STACK_FILL_PATTERN))
    {
        count++;
    }

    *unused_words = count;
    return OS_STATUS_OK;
}

/**
 * @brief 以固定周期推进当前任务的下一次唤醒时刻。
 *
 * @param previous_wake_tick 上一次唤醒基准 tick。
 * @param period_ticks 任务期望的周期长度，单位为 tick。
 *
 * @return os_status_t 成功进入延时或本次无需延时时返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_delay_until(os_tick_t *previous_wake_tick, os_tick_t period_ticks)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t    primask = 0U;          // 当前 delay-until 路径的外层临界区
    os_tick_t   next_wake_tick = 0U;   // 本轮计算得到的绝对目标唤醒 tick
    os_tick_t   current_tick = 0U;     // 进入临界区后读取到的当前绝对 tick
    os_tick_t   remaining_ticks = 0U;  // 当前距离目标唤醒点还需要等待多少 tick

    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (previous_wake_tick == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if ((period_ticks == 0U) || (period_ticks == OS_WAIT_FOREVER) || (task_timeout_is_supported(period_ticks) == 0U))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    primask = os_port_enter_critical();
    status = task_validate_running_task(g_current_task);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    next_wake_tick = (os_tick_t)(*previous_wake_tick + period_ticks);
    current_tick = g_os_tick;
    *previous_wake_tick = next_wake_tick;

    if (task_tick_is_due(current_tick, next_wake_tick) != 0U)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_OK;
    }

    remaining_ticks = (os_tick_t)(next_wake_tick - current_tick);
    status = task_prepare_wait_locked(g_current_task, TASK_SLEEPING, NULL, remaining_ticks, NULL);

    if (status == OS_STATUS_SWITCH_REQUIRED)
    {
        os_port_trigger_pendsv();
        os_port_exit_critical(primask);
        return OS_STATUS_OK;
    }

    os_port_exit_critical(primask);
    return status;
}

/**
 * @brief 让当前任务进入延时睡眠状态。
 *
 * @param delay_ticks 相对延时 tick 数；传 0 表示立即返回且不进入睡眠。
 *
 * @return os_status_t 成功进入延时路径并最终恢复返回时返回 OS_STATUS_OK，
 *                     否则返回具体错误码。
 */
os_status_t task_delay(os_tick_t delay_ticks)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t primask = 0U;

    /* delay 只能由线程态代码发起；
     * ISR 若要延后工作，应该通过对象唤醒线程，而不是把自己送进睡眠。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* delay(0) 明确定义为 no-op，不进入睡眠，也不等价于 yield。 */
    if (delay_ticks == 0U)
    {
        return OS_STATUS_OK;
    }

    /* delay 必须表达一个“有限时长的睡眠”。
     * OS_WAIT_FOREVER 只保留给“等待对象且永久阻塞”的语义，不能拿来做 task_delay。 */
    if (delay_ticks == OS_WAIT_FOREVER)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 从这里开始，当前任务会离开 runnable 集合；
     * 因此整个“摘链 + 选出 next task + 提交 PendSV”必须放在同一个临界区内。 */
    primask = os_port_enter_critical();
    status = task_prepare_wait_locked(g_current_task, TASK_SLEEPING, NULL, delay_ticks, NULL);

    if (status == OS_STATUS_SWITCH_REQUIRED)
    {
        /* 审查点修复：
         * 当前任务已经不在 ready queue 中了，所以必须先把 PendSV 置 pending，
         * 再恢复中断。这样任何 SysTick 或对象 IRQ 都不可能在“任务已阻塞、
         * 但线程还继续向前跑”的窗口里抢先把它重新唤醒。 */
        os_port_trigger_pendsv();
        os_port_exit_critical(primask);
        return OS_STATUS_OK;
    }

    /* 只有在“不需要切换”或“返回错误”的路径上，才按常规顺序退出临界区。 */
    os_port_exit_critical(primask);
    return status;
}

/**
 * @brief 让当前任务进入对象等待阻塞状态。
 *
 * @param wait_obj 当前等待的对象指针。
 * @param timeout_ticks 等待超时 tick 数；传 OS_WAIT_FOREVER 表示永久等待。
 *
 * @return os_status_t 成功进入阻塞路径并最终恢复返回时返回 OS_STATUS_OK，
 *                     否则返回具体错误码。
 */
os_status_t task_block_current(void *wait_obj, os_tick_t timeout_ticks, task_wait_cleanup_fn_t wait_cleanup_locked)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t primask = 0U;

    /* block 接口同样只允许在线程态使用；
     * ISR 不应把自己挂起，而应唤醒或解锁别的线程。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* timeout=0 的语义是“立即超时返回”，所以这里不做任何摘链动作，
     * 只把当前等待结果记成 TIMEOUT，供上层对象代码按同步失败路径处理。 */
    if (timeout_ticks == 0U)
    {
        status = task_validate_running_task(g_current_task);
        if (status != OS_STATUS_OK)
        {
            return status;
        }

        if (g_current_task != NULL)
        {
            g_current_task->wait_result = TASK_WAIT_RESULT_TIMEOUT;
            g_current_task->wait_cleanup_locked = NULL;
        }

        return OS_STATUS_OK;
    }

    /* 进入真正的阻塞路径后，当前任务可能马上离开 runnable 集合；
     * 因此和 task_delay() 一样，要把“摘链 + 调度 + PendSV 置 pending”
     * 绑在同一个临界区里完成。 */
    primask = os_port_enter_critical();
    status = task_prepare_wait_locked(g_current_task, TASK_BLOCKED, wait_obj, timeout_ticks, wait_cleanup_locked);

    if (status == OS_STATUS_SWITCH_REQUIRED)
    {
        /* 审查点修复：
         * 先提交 PendSV，再恢复 PRIMASK，避免对象 IRQ / SysTick 抢在真正切走前
         * 改写同一个任务的等待状态，从而把阻塞错误地退化成 no-op。 */
        os_port_trigger_pendsv();
        os_port_exit_critical(primask);
        return OS_STATUS_OK;
    }

    /* 不需要切换或发生错误时，再按普通路径退出临界区。 */
    os_port_exit_critical(primask);
    return status;
}

/**
 * @brief 唤醒一个等待中的任务，并返回新的调度决策结果。
 *
 * @param task 待唤醒的任务控制块。
 * @param wait_result 本次唤醒应写入的等待结果。
 *
 * @return os_status_t 返回唤醒后的调度决策结果。
 */
os_status_t task_unblock(tcb_t *task, task_wait_result_t wait_result)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t primask = 0U;

    /* unblock 会同时改动 timed-wait、事件等待链表和 ready queue，
     * 所以这里统一放进最小临界区内执行。 */
    primask = os_port_enter_critical();
    status = task_unblock_locked(task, wait_result);
    os_port_exit_critical(primask);

    /* 返回值保留“唤醒后是否需要切换”的调度信息，交由上层决定是否触发 PendSV。 */
    return status;
}

/**
 * @brief 删除当前运行任务并切走，正常情况下不返回。
 */
void task_exit_current(void)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t primask = 0U;
    tcb_t *current_task = g_current_task;

    /* 这条 helper 只允许在线程态删除一个真实 RUNNING 的当前任务。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        os_panic(OS_PANIC_TASK_STATE, __FILE__, (uint32_t)__LINE__);
    }

    status = task_validate_running_task(current_task);
    if (status != OS_STATUS_OK)
    {
        os_panic(OS_PANIC_TASK_STATE, __FILE__, (uint32_t)__LINE__);
    }

    /* idle 不能删除；若真的走到这里，说明调用链已经严重失配，直接停住。 */
    if (current_task == &g_idle_task)
    {
        os_panic(OS_PANIC_TASK_STATE, __FILE__, (uint32_t)__LINE__);
    }

    /* 第一版 mutex 明确禁止“持锁任务直接 return 自删”；
     * 若真的发生，直接停机暴露错误，不做隐式释放。 */
    if (list_is_empty(&current_task->owned_mutex_list) == 0U)
    {
        os_panic(OS_PANIC_TASK_STATE, __FILE__, (uint32_t)__LINE__);
    }

    primask = os_port_enter_critical();

    /* 先把当前任务从调度器持有结构中摘掉，并标记成 DELETED。 */
    status = task_mark_deleted_locked(current_task);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        os_panic(OS_PANIC_TASK_STATE, __FILE__, (uint32_t)__LINE__);
    }

    /* 当前任务删掉后必须重新调度，保证 g_next_task 指向一个新的合法目标。 */
    status = task_schedule();
    if (status != OS_STATUS_SWITCH_REQUIRED)
    {
        /* 正常情况下自删后一定需要切走到别的 runnable 任务；
         * 若不是，说明调度器状态已经异常。 */
        os_port_exit_critical(primask);
        os_panic(OS_PANIC_TASK_STATE, __FILE__, (uint32_t)__LINE__);
    }

    /* 在恢复中断前先 pend PendSV，保证当前已删除任务不会继续向前执行用户代码。 */
    os_port_trigger_pendsv();
    os_port_exit_critical(primask);

    /* 正常情况下恢复中断后，PendSV 会立刻把当前已删除任务切走；
     * 若链路异常返回到这里，就永远停住，避免带着已删除任务继续执行。 */
    os_panic(OS_PANIC_TASK_STATE, __FILE__, (uint32_t)__LINE__);
}

/**
 * @brief 获取当前任务指针。
 *
 * @return tcb_t* 当前任务控制块指针；若尚未切入任务则返回 NULL。
 */
tcb_t *task_get_current(void)
{
    /* 这里直接返回 current task 快照，不附带任何额外状态变化。 */
    return g_current_task;
}

/**
 * @brief 获取下一个待运行任务指针。
 *
 * @return tcb_t* 下一个任务控制块指针；若尚未选出则返回 NULL。
 */
tcb_t *task_get_next(void)
{
    /* g_next_task 由最近一次 schedule() 决策维护，这里只读暴露其当前值。 */
    return g_next_task;
}

/**
 * @brief 获取全局可运行任务集合对象。
 *
 * @return ready_queue_t* 全局可运行任务集合指针。
 */
ready_queue_t *task_get_ready_queue(void)
{
    /* 返回的是内核内部全局 ready queue 本体，调用方拿到的是共享对象地址。 */
    return &g_task_ready_queue;
}

/**
 * @brief 提交当前调度结果，把已经选中的 g_next_task 设为当前运行任务。
 *
 * @param task 新的当前任务控制块，必须等于当前 g_next_task。
 *
 * @return os_status_t 设置成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_set_current(tcb_t *task)
{
    /* 提交 current task 时，必须给出明确的目标任务。 */
    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 目标首先得是一个已经初始化完成的合法 TCB。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* port 层不能绕过调度器任意挑任务，
     * 这里只允许把当前 g_next_task 提交成 current task。 */
    if (task != g_next_task)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 被切入的任务必须仍然留在 runnable 集合里，
     * 否则说明 ready queue / task state 已经失配。 */
    if (task_is_in_runnable_queue(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 若目标不是当前任务本身，那么它在提交前必须明确处于 READY，
     * 不能把 BLOCKED/SLEEPING/DELETED 任务直接提升成 RUNNING。 */
    if ((task != g_current_task) && (task->state != TASK_READY))
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 真正切走旧任务前，先把它从 RUNNING 降回 READY；
     * 它仍留在 runnable 集合中，只是暂时不占用 CPU。 */
    if ((g_current_task != NULL) && (g_current_task != task) && (g_current_task->state == TASK_RUNNING))
    {
        g_current_task->state = TASK_READY;
    }

    /* 直到这里才真正提交新的 current，并把状态推进为 RUNNING。 */
    g_current_task = task;
    g_current_task->state = TASK_RUNNING;
    return OS_STATUS_OK;
}

/**
 * @brief 判断优先级是否落在当前就绪队列支持的范围内。
 *
 * @param priority 待检查的任务优先级。
 *
 * @return uint8_t 非 0 表示优先级有效，0 表示优先级越界。
 */
static uint8_t ready_queue_priority_is_valid(uint8_t priority)
{
    /* 当前实现用 32-bit 位图管理优先级，所以合法范围就是 0 ~ OS_MAX_PRIORITIES-1。 */
    return (uint8_t)(priority < OS_MAX_PRIORITIES);
}

/**
 * @brief 判断一个优先级是否允许给普通用户任务使用。
 *
 * @param priority 待检查的任务优先级。
 *
 * @return uint8_t 非 0 表示优先级允许给普通任务使用，0 表示该优先级被保留。
 */
static uint8_t task_user_priority_is_valid(uint8_t priority)
{
    /* 先确认 priority 至少在 ready queue 的总范围内。 */
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return 0U;
    }

    /* 普通任务不能占用保留给 idle 的最低优先级槽位。 */
    return (uint8_t)(priority < OS_IDLE_TASK_PRIORITY);
}

/**
 * @brief 根据优先级生成对应的就绪位图掩码。
 *
 * @param priority 任务优先级，数值越小优先级越高。
 *
 * @return uint32_t 对应优先级的位图掩码。
 */
static uint32_t ready_queue_priority_mask(uint8_t priority)
{
    /* 越界 priority 没有对应的 bitmap 位，直接返回 0 掩码。 */
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return 0U;
    }

    /* 每个优先级在 ready_bitmap 中占一个 bit。 */
    return (uint32_t)(1UL << priority);
}

/**
 * @brief 初始化可运行任务集合。
 *
 * @param queue 待初始化的可运行任务集合。
 */
void ready_queue_init(ready_queue_t *queue)
{
    uint32_t priority = 0U;

    /* 空指针保护，避免初始化阶段误写非法内存。 */
    if (queue == NULL)
    {
        return;
    }

    /* 初始时没有任何 runnable 任务，所以位图先清零。 */
    queue->ready_bitmap = 0U;

    /* 把每个优先级槽位都初始化成空链表，后续 insert/remove 才有定义。 */
    for (priority = 0U; priority < OS_MAX_PRIORITIES; priority++)
    {
        list_init(&queue->ready_lists[priority]);
    }
}

/**
 * @brief 将任务按尾插方式加入对应优先级的可运行链表。
 *
 * @param queue 目标可运行任务集合。
 * @param task 待加入的任务控制块。
 */
void ready_queue_insert_tail(ready_queue_t *queue, tcb_t *task)
{
    uint8_t priority = 0U;

    /* ready queue 和 task 对象缺一不可。 */
    if ((queue == NULL) || (task == NULL))
    {
        return;
    }

    /* 任务自己的 priority 决定它应当进入哪条 ready list。 */
    priority = task->priority;
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    /* 同优先级任务统一尾插，天然兼容 FIFO 与时间片轮转语义。 */
    if (list_insert_tail(&queue->ready_lists[priority], &task->sched_node) == 0U)
    {
        return;
    }

    /* 插入成功后，同步置位 ready bitmap，并把任务状态推进成 READY。 */
    queue->ready_bitmap |= ready_queue_priority_mask(priority);
    task->state = TASK_READY;
}

/**
 * @brief 将任务从对应优先级的可运行链表中移除。
 *
 * @note 本函数只负责摘链和位图维护，不负责修改任务状态。
 *       调用方必须在更高层显式把任务状态切换为
 *       TASK_BLOCKED、TASK_SLEEPING 或 TASK_DELETED。
 *
 * @param queue 目标可运行任务集合。
 * @param task 待移除的任务控制块。
 */
void ready_queue_remove(ready_queue_t *queue, tcb_t *task)
{
    uint8_t priority = 0U;
    list_t *list = NULL;

    /* 参数任一为空都无法完成摘链。 */
    if ((queue == NULL) || (task == NULL))
    {
        return;
    }

    /* 根据任务自己的 priority 找到它当前应当所在的 ready list。 */
    priority = task->priority;
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    /* 先定位链表，再尝试把任务的 sched_node 摘下来。 */
    list = &queue->ready_lists[priority];
    if (list_remove(list, &task->sched_node) == 0U)
    {
        return;
    }

    /* 该优先级链表若已被摘空，要同步清掉 ready bitmap 上对应的 bit。 */
    if (list_is_empty(list) != 0U)
    {
        queue->ready_bitmap &= ~ready_queue_priority_mask(priority);
    }
}

/**
 * @brief 获取当前最高优先级的非空可运行链表优先级值。
 *
 * @param queue 待查询的可运行任务集合。
 * @param priority 输出的最高优先级指针。
 *
 * @return uint8_t 非 0 表示查找成功，0 表示当前没有可运行任务。
 */
uint8_t ready_queue_get_highest_priority(const ready_queue_t *queue, uint8_t *priority)
{
    uint8_t current = 0U;

    /* 空队列或空指针都表示“当前没有任何 runnable 优先级”。 */
    if ((queue == NULL) || (queue->ready_bitmap == 0U))
    {
        return 0U;
    }

    /* 数值越小优先级越高，因此从 0 开始扫描 ready bitmap。 */
    for (current = 0U; current < OS_MAX_PRIORITIES; current++)
    {
        if ((queue->ready_bitmap & ready_queue_priority_mask(current)) != 0U)
        {
            /* 调用方如果给了输出参数，就顺手把找到的最高优先级写回。 */
            if (priority != NULL)
            {
                *priority = current;
            }

            /* 第一个命中的 bit 就是当前最高优先级，可以立刻返回。 */
            return 1U;
        }
    }

    /* 理论上不应走到这里；若走到这里，说明 ready_bitmap 与扫描结果不一致。 */
    return 0U;
}

/**
 * @brief 查看当前最高优先级的可运行任务，但不将其移出可运行集合。
 *
 * @param queue 待查询的可运行任务集合。
 *
 * @return const tcb_t* 当前最高优先级的可运行任务控制块只读指针；若集合为空则返回 NULL。
 */
const tcb_t *ready_queue_peek_highest(const ready_queue_t *queue)
{
    uint8_t priority = 0U;
    const list_t *ready_list = NULL;

    /* 先找出哪一个优先级当前最高且非空。 */
    if (ready_queue_get_highest_priority(queue, &priority) == 0U)
    {
        return NULL;
    }

    /* 再拿到该优先级对应的 ready list。 */
    ready_list = &queue->ready_lists[priority];
    if (ready_list->head == NULL)
    {
        /* 位图和链表理论上应保持一致，这里保留空头保护，避免非法解引用。 */
        return NULL;
    }

    /* 最高优先级链表的头节点，就是当前最应该运行的任务。 */
    return LIST_CONTAINER_OF(ready_list->head, tcb_t, sched_node);
}

/**
 * @brief 对指定优先级的可运行链表执行一次时间片轮转。
 *
 * @param queue 目标可运行任务集合。
 * @param priority 需要轮转的优先级。
 */
void ready_queue_rotate(ready_queue_t *queue, uint8_t priority)
{
    list_t *ready_list = NULL;
    list_node_t *node = NULL;

    /* 空队列对象没有可轮转的目标。 */
    if (queue == NULL)
    {
        return;
    }

    /* 越界 priority 不能映射到合法 ready list。 */
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    /* 轮转只作用于某个固定优先级的 ready list。 */
    ready_list = &queue->ready_lists[priority];
    if (ready_list->item_count <= 1U)
    {
        /* 少于两个节点时，头尾移动没有任何意义。 */
        return;
    }

    /* 轮转的本质就是：摘下头节点，再把它重新挂到尾部。 */
    node = list_remove_head(ready_list);
    if (node != NULL)
    {
        (void)list_insert_tail(ready_list, node);
    }
}

/**
 * @brief 判断整个可运行任务集合是否为空。
 *
 * @param queue 待判断的可运行任务集合。
 *
 * @return uint8_t 非 0 表示可运行任务集合为空，0 表示至少存在一个可运行任务。
 */
uint8_t ready_queue_is_empty(const ready_queue_t *queue)
{
    /* 约定空指针按“空队列”处理，这样调用方可以少写一层空值判断。 */
    if (queue == NULL)
    {
        return 1U;
    }

    /* 当前版本以 ready_bitmap 是否为 0 作为整个 runnable 集合是否为空的唯一依据。 */
    return (uint8_t)(queue->ready_bitmap == 0U);
}
