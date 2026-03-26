/**
 * @file os_task.c
 * @author Yukikaze
 * @brief RTOS 任务管理与调度器实现文件。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件实现任务生命周期管理、调度与延时处理逻辑。
 */

#include <string.h>
#include "os_port.h"
#include "os_task.h"

/* 全局可运行任务集合，同时包含 TASK_READY 和当前 TASK_RUNNING 任务。 */
static ready_queue_t g_task_ready_queue;
/* 当前正在 CPU 上运行的任务控制块指针。 */
static tcb_t        *g_current_task = NULL;
/* 调度器本次选出的下一个待运行任务控制块指针。 */
static tcb_t        *g_next_task = NULL;
/* 任务系统初始化标志，非 0 表示全局调度状态已经建立。 */
static uint8_t       g_task_system_initialized = 0U;

static uint8_t ready_queue_priority_is_valid(uint8_t priority);
static uint32_t ready_queue_priority_mask(uint8_t priority);
static os_status_t task_select_next(void);
static list_t *task_get_ready_list_by_priority(uint8_t priority);
static uint8_t task_is_known_to_scheduler(const tcb_t *task);
static uint8_t task_is_valid(const tcb_t *task);
static os_status_t task_validate_running_task(const tcb_t *task);

/**
 * @brief 判断任务是否已经处于全局可运行任务集合中。
 *
 * @param task 待检查的任务控制块。
 *
 * @return uint8_t 非 0 表示任务在可运行集合中，0 表示任务不在可运行集合中。
 */
static uint8_t task_is_in_runnable_queue(const tcb_t *task)
{
    /* 先确认这是一个已经完成初始化的合法 TCB，
     * 避免后面直接读取 priority 或链表归属信息时访问脏数据。 */
    if (task_is_valid(task) == 0U)
    {
        return 0U;
    }

    /* 只有合法优先级才可能映射到 ready queue 的某一条链表。 */
    if (ready_queue_priority_is_valid(task->priority) == 0U)
    {
        return 0U;
    }

    /* runnable 集合的判定标准不是看 state 字段，
     * 而是看调度节点当前是否真的挂在该优先级的 ready list 上。 */
    return (uint8_t)(task->sched_node.owner == &g_task_ready_queue.ready_lists[task->priority]);
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
    /* 优先级越界时不能返回链表地址，避免调用方拿到非法指针。 */
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return NULL;
    }

    /* 当前 ready queue 采用“每个优先级一条链表”的组织方式。 */
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
    uint8_t      priority = 0U;
    const list_t *ready_list = NULL;
    list_node_t *node     = NULL;

    /* 调度器未初始化时，全局 ready queue 还没有建立，
     * 此时直接判定“尚未被调度器持有”。 */
    if ((task == NULL) || (g_task_system_initialized == 0U))
    {
        return 0U;
    }

    for (priority = 0U; priority < OS_MAX_PRIORITIES; priority++)
    {
        /* 位图未置位说明该优先级当前没有 runnable 任务，
         * 无需进入链表逐个比对。 */
        if ((g_task_ready_queue.ready_bitmap & ready_queue_priority_mask(priority)) == 0U)
        {
            continue;
        }

        ready_list = &g_task_ready_queue.ready_lists[priority];
        node = ready_list->head;

        while (node != NULL)
        {
            /* 这里按 TCB 地址判重，目的是防止调用方把同一个 TCB
             * 再次传给 task_create()，从而破坏已有链表结构。 */
            if (LIST_CONTAINER_OF(node, tcb_t, sched_node) == task)
            {
                return 1U;
            }

            node = node->next;
        }
    }

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
    /* 空指针一定不可能是合法任务。 */
    if (task == NULL)
    {
        return 0U;
    }

    /* 当前版本用 magic 判断 TCB 是否已完成初始化。 */
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
    /* 调度器全局状态尚未建立时，任何“当前运行任务”语义都不成立。 */
    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    /* 当前路径要求必须已经有明确的 current task。 */
    if (task == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 任务本身必须是一个已经初始化过的合法 TCB。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* TASK_RUNNING 的任务仍然留在 runnable 集合中，
     * 所以这里还要确认它确实还挂在 ready queue 里。 */
    if (task_is_in_runnable_queue(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 只有 state 明确为 TASK_RUNNING，才能走 yield/tick 这类
     * 以“当前正在占用 CPU 的任务”为前提的逻辑。 */
    if (task->state != TASK_RUNNING)
    {
        return OS_STATUS_INVALID_STATE;
    }

    return OS_STATUS_OK;
}

/**
 * @brief 判断任务初始化配置是否完整且合法。
 *
 * @param config 待检查的任务初始化配置。
 *
 * @return os_status_t 配置合法返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_validate_init_config(const task_init_config_t *config)
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

    /* 栈深度至少要能容纳当前端口约定的最小初始栈帧。 */
    if (config->stack_size < OS_TASK_MIN_STACK_DEPTH)
    {
        return OS_STATUS_INVALID_STACK;
    }

    /* 优先级必须落在 ready bitmap 和 ready_lists 支持的范围内。 */
    if (ready_queue_priority_is_valid(config->priority) == 0U)
    {
        return OS_STATUS_INVALID_PRIORITY;
    }

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
    /* 用户传 0 表示“使用系统默认时间片”，不是“关闭时间片”。 */
    if (time_slice == 0U)
    {
        return (uint8_t)OS_TASK_DEFAULT_TIME_SLICE;
    }

    return time_slice;
}

/**
 * @brief 初始化任务系统全局调度状态。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK。
 */
os_status_t task_system_init(void)
{
    /* 建立空的 runnable 集合。 */
    ready_queue_init(&g_task_ready_queue);
    /* 启动前还没有 current/next task。 */
    g_current_task = NULL;
    g_next_task = NULL;
    /* 标记调度器全局状态已准备就绪。 */
    g_task_system_initialized = 1U;

    return OS_STATUS_OK;
}

/**
 * @brief 根据配置初始化一个任务控制块，但不将其加入全局可运行任务集合。
 *
 * @param task 待初始化的任务控制块。
 * @param config 任务初始化配置。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t task_init(tcb_t *task, const task_init_config_t *config)
{
    os_status_t status = OS_STATUS_OK;
    uint8_t     time_slice = 0U;
    uint32_t   *initial_sp = NULL;

    /* 创建入口要求调用方提供一个可写的 TCB 存储区。 */
    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 先做纯参数检查，避免把非法配置继续传入端口层。 */
    status = task_validate_init_config(config);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 先归一化时间片，再交给端口层伪造初始栈帧。 */
    time_slice = task_normalize_time_slice(config->time_slice);
    initial_sp = os_port_task_stack_init(config->stack_base, config->stack_size, config->entry, config->param);

    /* 端口层若无法构造初始栈，说明当前栈参数不满足要求。 */
    if (initial_sp == NULL)
    {
        return OS_STATUS_INVALID_STACK;
    }

    /* 从零清空整个 TCB，确保旧内容不会污染新任务。 */
    (void)memset(task, 0, sizeof(tcb_t));

    /* 以下字段都是任务创建后的基础运行上下文。 */
    task->sp               = initial_sp;
    task->stack_base       = config->stack_base;
    task->stack_size       = config->stack_size;
    task->entry            = config->entry;
    task->param            = config->param;
    task->name             = config->name;
    task->magic            = OS_TASK_MAGIC;
    task->wake_tick        = 0U;
    task->wait_obj         = NULL;
    task->priority         = config->priority;
    /* 任务刚初始化完成时还未进入 runnable 集合，
     * 所以先放在非运行状态，后续由 ready_queue_insert_tail() 改成 READY。 */
    task->state            = TASK_BLOCKED;
    task->time_slice       = time_slice;
    task->time_slice_reload = time_slice;

    /* 调度链表节点和事件链表节点都必须从“未挂链”状态开始。 */
    list_node_init(&task->sched_node);
    list_node_init(&task->event_node);

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
    uint8_t     scheduler_running = 0U;

    /* 创建接口不接受空 TCB 指针。 */
    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 首次创建任务时，顺手建立调度器全局状态。 */
    if (g_task_system_initialized == 0U)
    {
        (void)task_system_init();
    }

    /* 若同一个 TCB 已经被调度器持有，再次创建会破坏 ready queue，
     * 所以这里必须直接拒绝。 */
    if (task_is_known_to_scheduler(task) != 0U)
    {
        return OS_STATUS_ALREADY_INITIALIZED;
    }

    /* 先完成 TCB 和初始栈的静态初始化。 */
    status = task_init(task, config);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 创建成功后，再把任务正式放入 runnable 集合。 */
    ready_queue_insert_tail(&g_task_ready_queue, task);

    /* owner 没有正确落到目标 ready list 上，说明插入失败。 */
    if (task->sched_node.owner != &g_task_ready_queue.ready_lists[task->priority])
    {
        task->state = TASK_DELETED;
        return OS_STATUS_INSERT_FAILED;
    }

    /* 用 current_task 是否存在，区分“系统已在运行”和“还没启动首任务”。 */
    scheduler_running = (uint8_t)(g_current_task != NULL);
    /* 插入新任务后立即重跑一次调度，让 g_next_task 与当前 runnable 集合保持一致。 */
    status = task_schedule();

    /* 调度器明确判断“无需切换”时，对创建接口统一按成功返回。 */
    if (status == OS_STATUS_NO_CHANGE)
    {
        return OS_STATUS_OK;
    }

    /* 首任务启动前，schedule() 会因为还没有 current task 而返回
     * SWITCH_REQUIRED，但这里不能把它误报成“运行期抢占”。 */
    if ((status == OS_STATUS_SWITCH_REQUIRED) && (scheduler_running == 0U))
    {
        return OS_STATUS_OK;
    }

    /* 其余情况原样向上传递，尤其是运行期创建高优先级任务时的
     * OS_STATUS_SWITCH_REQUIRED。 */
    return status;
}

/**
 * @brief 从全局可运行任务集合中挑选下一个应运行的任务。
 *
 * @return os_status_t 选择成功返回 OS_STATUS_OK；若无可运行任务则返回 OS_STATUS_EMPTY。
 */
static os_status_t task_select_next(void)
{
    const tcb_t *selected_task = NULL;

    /* 全局调度器都还没建立时，无法选择 next task。 */
    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    /* 最高优先级 ready list 的头节点就是当前应运行的任务。 */
    selected_task = ready_queue_peek_highest(&g_task_ready_queue);
    if (selected_task == NULL)
    {
        /* 没有 runnable 任务时，要同步清空 g_next_task。 */
        g_next_task = NULL;
        return OS_STATUS_EMPTY;
    }

    /* 注意：这里只负责“选出谁”，不负责真正切换。 */
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

    /* 先刷新 g_next_task，让它始终反映当前 runnable 集合的最高优先级结果。 */
    status = task_select_next();
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* current 为空表示系统还没真正切入任何任务，
     * 但 next 已经选出来了，后续应启动首任务。 */
    if (g_current_task == NULL)
    {
        return OS_STATUS_SWITCH_REQUIRED;
    }

    /* 只要 next 和 current 不是同一个任务，就说明需要做上下文切换。 */
    if (g_next_task != g_current_task)
    {
        return OS_STATUS_SWITCH_REQUIRED;
    }

    /* 走到这里说明“当前任务继续运行”即可。 */
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
    list_t     *ready_list = NULL;

    /* yield 只能由当前正在运行的任务发起。 */
    status = task_validate_running_task(g_current_task);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 取出当前优先级对应的 ready list，后续轮转只在这一条链表内进行。 */
    ready_list = task_get_ready_list_by_priority(g_current_task->priority);
    if (ready_list == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 主动让出 CPU 时，等价于放弃本轮剩余时间片，
     * 因此把时间片恢复到 reload 值，留给下次再次运行时使用。 */
    g_current_task->time_slice = g_current_task->time_slice_reload;

    /* 只有当前任务正好在本优先级队头，且确实存在同优先级其他任务时，
     * 才需要执行“头移到尾”的轮转。 */
    if ((ready_list->item_count > 1U) && (ready_list->head == &g_current_task->sched_node))
    {
        ready_queue_rotate(&g_task_ready_queue, g_current_task->priority);
    }

    /* 轮转完成后，重新做一次完整调度决策。 */
    return task_schedule();
}

/**
 * @brief 推进当前运行任务的时间片计数，并在量子耗尽时执行同优先级轮转。
 *
 * @return os_status_t 返回本次 tick 处理得到的调度决策结果。
 */
os_status_t task_time_slice_tick(void)
{
    os_status_t status = OS_STATUS_OK;
    list_t     *ready_list = NULL;
    tcb_t      *current_task = g_current_task;

    /* 未初始化时，时间片逻辑没有语义基础。 */
    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    /* 尚未切入首任务时，tick 不需要做任何时间片处理。 */
    if (current_task == NULL)
    {
        return OS_STATUS_NO_CHANGE;
    }

    /* 只有真实处于 RUNNING 的 current task 才能消费时间片。 */
    status = task_validate_running_task(current_task);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    /* 这一条链表代表“当前任务所属优先级”的 runnable 任务集合。 */
    ready_list = task_get_ready_list_by_priority(current_task->priority);
    if (ready_list == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 先做一次全局调度检查，优先捕获“更高优先级任务已经 runnable”
     * 的情况。这样就不会因为后面只看同优先级链表而漏报抢占。 */
    status = task_schedule();
    if (status == OS_STATUS_SWITCH_REQUIRED)
    {
        return status;
    }

    /* 对 tick 路径来说，除了“无需切换”之外的其他返回都说明状态异常，
     * 直接向上返回即可。 */
    if (status != OS_STATUS_NO_CHANGE)
    {
        return status;
    }

    /* 当前优先级只有自己一个 runnable 任务时，不需要做时间片轮转，
     * 但仍把剩余片维持在 reload 值，避免单任务场景不断耗尽到 0。 */
    if (ready_list->item_count <= 1U)
    {
        current_task->time_slice = current_task->time_slice_reload;
        return OS_STATUS_NO_CHANGE;
    }

    /* 只有时间片大于 0 时才递减，防止无符号下溢。 */
    if (current_task->time_slice > 0U)
    {
        current_task->time_slice--;
    }

    /* 时间片还没用完，则当前任务继续运行。 */
    if (current_task->time_slice > 0U)
    {
        return OS_STATUS_NO_CHANGE;
    }

    /* 时间片耗尽后，先恢复初始时间片，供该任务下一轮再次被调度时使用。 */
    current_task->time_slice = current_task->time_slice_reload;

    /* 时间片轮转的核心动作是把本优先级队头移到队尾。 */
    if (ready_list->head == &current_task->sched_node)
    {
        ready_queue_rotate(&g_task_ready_queue, current_task->priority);
    }

    /* 轮转后再做一次调度，决定是否切到同优先级下一个任务。 */
    return task_schedule();
}

/**
 * @brief 获取当前任务指针。
 *
 * @return tcb_t* 当前任务控制块指针；若尚未切入任务则返回 NULL。
 */
tcb_t *task_get_current(void)
{
    return g_current_task;
}

/**
 * @brief 获取下一个待运行任务指针。
 *
 * @return tcb_t* 下一个任务控制块指针；若尚未选出则返回 NULL。
 */
tcb_t *task_get_next(void)
{
    return g_next_task;
}

/**
 * @brief 获取全局可运行任务集合对象。
 *
 * @return ready_queue_t* 全局可运行任务集合指针。
 */
ready_queue_t *task_get_ready_queue(void)
{
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

    /* 目标必须是一个已经初始化好的合法 TCB。 */
    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 这里的边界被刻意收紧：只能把调度器刚刚选出来的 g_next_task
     * 提升为当前任务，不能绕过调度器直接指定其他 READY 任务。 */
    if (task != g_next_task)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 被切入的任务必须仍然处于 runnable 集合中。 */
    if (task_is_in_runnable_queue(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 若目标不是当前任务本身，则它在提交前应该处于 READY，
     * 不允许把 BLOCKED/SLEEPING/DELETED 任务直接切成 RUNNING。 */
    if ((task != g_current_task) && (task->state != TASK_READY))
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 旧的 current task 若确实在运行，且这次要切到别的任务，
     * 则先把它降回 READY。注意：它仍然留在 runnable 集合中。 */
    if ((g_current_task != NULL) && (g_current_task != task) && (g_current_task->state == TASK_RUNNING))
    {
        g_current_task->state = TASK_READY;
    }

    /* 真正提交 current task 的时刻在这里。 */
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
    /* 当前实现使用 32 位位图，因此合法优先级范围是 0 ~ OS_MAX_PRIORITIES - 1。 */
    return (uint8_t)(priority < OS_MAX_PRIORITIES);
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
    /* 越界优先级不应生成位图掩码。 */
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

    /* 空指针保护，避免初始化阶段误写非法地址。 */
    if (queue == NULL)
    {
        return;
    }

    /* 初始时没有任何 runnable 优先级。 */
    queue->ready_bitmap = 0U;

    /* 每个优先级的 ready list 都要单独初始化为空链表。 */
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

    /* ready queue 和任务对象都必须存在。 */
    if ((queue == NULL) || (task == NULL))
    {
        return;
    }

    /* 任务的 priority 决定它应插入哪一条 ready list。 */
    priority = task->priority;

    /* 非法优先级无法映射到 ready list。 */
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    /* 同优先级任务采用尾插，这样可以直接支撑时间片轮转语义。 */
    if (list_insert_tail(&queue->ready_lists[priority], &task->sched_node) == 0U)
    {
        return;
    }

    /* 插入成功后，更新位图并把任务状态推进为 READY。 */
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
    list_t *list     = NULL;

    /* 参数任一为空都无法完成摘链。 */
    if ((queue == NULL) || (task == NULL))
    {
        return;
    }

    /* 按任务自己的 priority 找到它应该所在的 ready list。 */
    priority = task->priority;

    /* 非法优先级不允许继续访问 ready_lists。 */
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    list = &queue->ready_lists[priority];

    /* 节点不在该链表里时，list_remove() 会失败；这里直接返回即可。 */
    if (list_remove(list, &task->sched_node) == 0U)
    {
        return;
    }

    /* 该优先级链表变空后，要同步清掉 ready_bitmap 上对应的 bit。 */
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

    /* 空队列或空指针都表示“当前没有可运行优先级”。 */
    if ((queue == NULL) || (queue->ready_bitmap == 0U))
    {
        return 0U;
    }

    /* 数值越小优先级越高，所以从 0 开始向后扫描位图。 */
    for (current = 0U; current < OS_MAX_PRIORITIES; current++)
    {
        if ((queue->ready_bitmap & ready_queue_priority_mask(current)) != 0U)
        {
            /* 调用方若提供输出参数，则把找到的最高优先级写回去。 */
            if (priority != NULL)
            {
                *priority = current;
            }
            return 1U;
        }
    }

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
    uint8_t priority   = 0U;
    const list_t *ready_list = NULL;

    /* 先找出最高优先级对应的是哪一条 ready list。 */
    if (ready_queue_get_highest_priority(queue, &priority) == 0U)
    {
        return NULL;
    }

    ready_list = &queue->ready_lists[priority];

    /* 位图和链表理论上应保持一致，这里保留空头节点保护。 */
    if (ready_list->head == NULL)
    {
        return NULL;
    }

    /* 最高优先级链表的头节点，就是当前应运行的任务。 */
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
    list_t      *ready_list = NULL;
    list_node_t *node       = NULL;

    /* 空队列对象无法执行轮转。 */
    if (queue == NULL)
    {
        return;
    }

    /* 只有合法优先级才能找到对应链表。 */
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    ready_list = &queue->ready_lists[priority];

    /* 少于两个节点时，轮转没有意义。 */
    if (ready_list->item_count <= 1U)
    {
        return;
    }

    /* 轮转动作等价于“摘下头节点，再挂到尾部”。 */
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
    /* 约定空指针按“空队列”处理。 */
    if (queue == NULL)
    {
        return 1U;
    }

    /* 当前实现以 ready_bitmap 是否为 0 作为整个 runnable 集合是否为空的判断依据。 */
    return (uint8_t)(queue->ready_bitmap == 0U);
}
