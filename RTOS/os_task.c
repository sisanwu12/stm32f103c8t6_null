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
static uint8_t task_is_valid(const tcb_t *task);

/**
 * @brief 判断任务是否已经处于全局可运行任务集合中。
 *
 * @param task 待检查的任务控制块。
 *
 * @return uint8_t 非 0 表示任务在可运行集合中，0 表示任务不在可运行集合中。
 */
static uint8_t task_is_in_runnable_queue(const tcb_t *task)
{
    if (task_is_valid(task) == 0U)
    {
        return 0U;
    }

    if (ready_queue_priority_is_valid(task->priority) == 0U)
    {
        return 0U;
    }

    return (uint8_t)(task->sched_node.owner == &g_task_ready_queue.ready_lists[task->priority]);
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
    if (task == NULL)
    {
        return 0U;
    }

    return (uint8_t)(task->magic == OS_TASK_MAGIC);
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
    if (config == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (config->entry == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (config->stack_base == NULL)
    {
        return OS_STATUS_INVALID_STACK;
    }

    if (config->stack_size < OS_TASK_MIN_STACK_DEPTH)
    {
        return OS_STATUS_INVALID_STACK;
    }

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
    ready_queue_init(&g_task_ready_queue);
    g_current_task = NULL;
    g_next_task = NULL;
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

    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    status = task_validate_init_config(config);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    time_slice = task_normalize_time_slice(config->time_slice);
    initial_sp = os_port_task_stack_init(config->stack_base, config->stack_size, config->entry, config->param);

    if (initial_sp == NULL)
    {
        return OS_STATUS_INVALID_STACK;
    }

    (void)memset(task, 0, sizeof(tcb_t));

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
    task->state            = TASK_BLOCKED;
    task->time_slice       = time_slice;
    task->time_slice_reload = time_slice;

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
 * @return os_status_t 创建成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_create(tcb_t *task, const task_init_config_t *config)
{
    os_status_t status = OS_STATUS_OK;

    if (g_task_system_initialized == 0U)
    {
        (void)task_system_init();
    }

    status = task_init(task, config);
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    ready_queue_insert_tail(&g_task_ready_queue, task);

    if (task->sched_node.owner != &g_task_ready_queue.ready_lists[task->priority])
    {
        task->state = TASK_DELETED;
        return OS_STATUS_INSERT_FAILED;
    }

    return OS_STATUS_OK;
}

/**
 * @brief 从全局可运行任务集合中挑选下一个应运行的任务。
 *
 * @return os_status_t 选择成功返回 OS_STATUS_OK；若无可运行任务则返回 OS_STATUS_EMPTY。
 */
os_status_t task_select_next(void)
{
    const tcb_t *selected_task = NULL;

    if (g_task_system_initialized == 0U)
    {
        return OS_STATUS_NOT_INITIALIZED;
    }

    selected_task = ready_queue_peek_highest(&g_task_ready_queue);
    if (selected_task == NULL)
    {
        g_next_task = NULL;
        return OS_STATUS_EMPTY;
    }

    g_next_task = (tcb_t *)selected_task;
    return OS_STATUS_OK;
}

/**
 * @brief 执行一次调度决策，更新当前选中的下一个任务指针。
 *
 * @return os_status_t 调度成功返回 OS_STATUS_OK；失败时返回具体错误码。
 */
os_status_t task_schedule(void)
{
    return task_select_next();
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
 * @brief 设置当前正在运行的任务指针，供启动首任务或完成上下文切换后调用。
 *
 * @param task 新的当前任务控制块。
 *
 * @return os_status_t 设置成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t task_set_current(tcb_t *task)
{
    if (task == NULL)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (task_is_valid(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (task_is_in_runnable_queue(task) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if ((task != g_current_task) && (task->state != TASK_READY))
    {
        return OS_STATUS_INVALID_STATE;
    }

    if ((g_current_task != NULL) && (g_current_task != task) && (g_current_task->state == TASK_RUNNING))
    {
        g_current_task->state = TASK_READY;
    }

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
    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return 0U;
    }

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

    if (queue == NULL)
    {
        return;
    }

    queue->ready_bitmap = 0U;

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

    if ((queue == NULL) || (task == NULL))
    {
        return;
    }

    priority = task->priority;

    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    if (list_insert_tail(&queue->ready_lists[priority], &task->sched_node) == 0U)
    {
        return;
    }

    queue->ready_bitmap |= ready_queue_priority_mask(priority);
    task->state = TASK_READY;
}

/**
 * @brief 将任务从对应优先级的可运行链表中移除。
 *
 * @param queue 目标可运行任务集合。
 * @param task 待移除的任务控制块。
 */
void ready_queue_remove(ready_queue_t *queue, tcb_t *task)
{
    uint8_t priority = 0U;
    list_t *list     = NULL;

    if ((queue == NULL) || (task == NULL))
    {
        return;
    }

    priority = task->priority;

    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    list = &queue->ready_lists[priority];

    if (list_remove(list, &task->sched_node) == 0U)
    {
        return;
    }

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

    if ((queue == NULL) || (queue->ready_bitmap == 0U))
    {
        return 0U;
    }

    for (current = 0U; current < OS_MAX_PRIORITIES; current++)
    {
        if ((queue->ready_bitmap & ready_queue_priority_mask(current)) != 0U)
        {
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

    if (ready_queue_get_highest_priority(queue, &priority) == 0U)
    {
        return NULL;
    }

    ready_list = &queue->ready_lists[priority];

    if (ready_list->head == NULL)
    {
        return NULL;
    }

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

    if (queue == NULL)
    {
        return;
    }

    if (ready_queue_priority_is_valid(priority) == 0U)
    {
        return;
    }

    ready_list = &queue->ready_lists[priority];

    if (ready_list->item_count <= 1U)
    {
        return;
    }

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
    if (queue == NULL)
    {
        return 1U;
    }

    return (uint8_t)(queue->ready_bitmap == 0U);
}
