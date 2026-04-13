/**
 * @file os_timer.c
 * @author Yukikaze
 * @brief RTOS 软件定时器实现文件。
 * @version 0.1
 * @date 2026-04-13
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件实现最小软件定时器服务：active list、expired FIFO 与 daemon task。
 */

#include <string.h>
#include "os_diag.h"
#include "os_port.h"
#include "os_sem.h"
#include "os_task.h"
#include "os_timer.h"

#define OS_TIMER_DAEMON_TASK_NAME "timerd"            // 软件定时器后台任务名称
#define OS_TIMER_TIMEOUT_HALF_RANGE ((os_tick_t)0x80000000UL) // 定时器 deadline 比较的半区间限制

static uint8_t     g_os_timer_service_initialized = 0U; // 非 0 表示 timer service 已建立
static list_t      g_os_timer_active_list;              // 按 expiry_tick 升序组织的 active timer 链表
static list_t      g_os_timer_expired_list;             // 待 daemon 消费的 expired FIFO 链表
static os_sem_t    g_os_timer_work_sem;                 // 用于唤醒 timer daemon 的二值信号量
static tcb_t       g_os_timer_task;                     // timer daemon 任务控制块
static uint32_t    g_os_timer_task_stack[OS_TIMER_TASK_STACK_DEPTH]; // timer daemon 栈

static uint8_t os_timer_is_valid(const os_timer_t *timer);
static uint8_t os_timer_timeout_is_supported(os_tick_t timeout_ticks);
static uint8_t os_timer_tick_is_due(os_tick_t current_tick, os_tick_t expiry_tick);
static uint8_t os_timer_tick_deadline_is_before(os_tick_t lhs, os_tick_t rhs);
static void os_timer_daemon_entry(void *param);
static os_status_t os_timer_service_init(void);
static void os_timer_service_reset_locked(void);
static void os_timer_remove_locked(os_timer_t *timer);
static void os_timer_active_list_insert_ordered_locked(os_timer_t *timer);
static void os_timer_queue_expiration_locked(os_timer_t *timer);

/**
 * @brief 判断一个软件定时器对象是否已完成合法初始化。
 *
 * @param timer 待检查的定时器对象。
 *
 * @return uint8_t 非 0 表示对象有效，0 表示对象无效。
 */
static uint8_t os_timer_is_valid(const os_timer_t *timer)
{
    if ((timer == NULL) || (timer->magic != OS_TIMER_MAGIC) || (timer->callback == NULL))
    {
        return 0U;
    }

    if ((timer->mode != OS_TIMER_ONE_SHOT) && (timer->mode != OS_TIMER_PERIODIC))
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief 判断 timer start 的 timeout 是否可安全参与 deadline 比较。
 *
 * @param timeout_ticks 定时器启动超时 tick 数。
 *
 * @return uint8_t 非 0 表示支持，0 表示非法。
 */
static uint8_t os_timer_timeout_is_supported(os_tick_t timeout_ticks)
{
    return (uint8_t)((timeout_ticks > 0U) && (timeout_ticks < OS_TIMER_TIMEOUT_HALF_RANGE));
}

/**
 * @brief 判断某个 expiry_tick 是否已经到期。
 *
 * @param current_tick 当前系统 tick。
 * @param expiry_tick 目标到期 tick。
 *
 * @return uint8_t 非 0 表示已到期，0 表示尚未到期。
 */
static uint8_t os_timer_tick_is_due(os_tick_t current_tick, os_tick_t expiry_tick)
{
    return (uint8_t)(((int32_t)(current_tick - expiry_tick)) >= 0);
}

/**
 * @brief 判断两个 deadline 的先后顺序。
 *
 * @param lhs 左侧 deadline。
 * @param rhs 右侧 deadline。
 *
 * @return uint8_t 非 0 表示 lhs 更早，0 表示 lhs 不早于 rhs。
 */
static uint8_t os_timer_tick_deadline_is_before(os_tick_t lhs, os_tick_t rhs)
{
    return (uint8_t)(((int32_t)(lhs - rhs)) < 0);
}

/**
 * @brief 把软件定时器服务全局对象恢复到“未初始化”初值。
 *
 * @note 该 helper 只允许在已经持有外层临界区时调用。
 *       这样做的目的是：
 *       1. 首次初始化前先明确回到干净状态；
 *       2. 若初始化中途失败，允许后续线程再次安全重试。
 */
static void os_timer_service_reset_locked(void)
{
    list_init(&g_os_timer_active_list);
    list_init(&g_os_timer_expired_list);
    (void)memset(&g_os_timer_work_sem, 0, sizeof(g_os_timer_work_sem));
    (void)memset(&g_os_timer_task, 0, sizeof(g_os_timer_task));
    g_os_timer_service_initialized = 0U;
}

/**
 * @brief 将定时器从 active/expired 链表中彻底移除。
 *
 * @param timer 目标定时器对象。
 */
static void os_timer_remove_locked(os_timer_t *timer)
{
    if (timer == NULL)
    {
        return;
    }

    if (timer->active_node.owner == &g_os_timer_active_list)
    {
        (void)list_remove(&g_os_timer_active_list, &timer->active_node);
    }

    if (timer->expired_node.owner == &g_os_timer_expired_list)
    {
        (void)list_remove(&g_os_timer_expired_list, &timer->expired_node);
    }

    timer->active = 0U;
    timer->pending_expirations = 0U;
}

/**
 * @brief 按 expiry_tick 升序将定时器插入 active list。
 *
 * @param timer 待挂入 active list 的定时器对象。
 */
static void os_timer_active_list_insert_ordered_locked(os_timer_t *timer)
{
    list_node_t *current = NULL;      // 当前扫描到的 active list 节点
    os_timer_t  *current_timer = NULL; // current 对应的外层定时器对象
    list_node_t *node = NULL;         // timer 自己的 active_node

    if ((timer == NULL) || (timer->active_node.owner != NULL))
    {
        return;
    }

    node = &timer->active_node;

    if (g_os_timer_active_list.head == NULL)
    {
        (void)list_insert_tail(&g_os_timer_active_list, node);
        timer->active = 1U;
        return;
    }

    current = g_os_timer_active_list.head;
    while (current != NULL)
    {
        current_timer = LIST_CONTAINER_OF(current, os_timer_t, active_node);
        if (os_timer_tick_deadline_is_before(timer->expiry_tick, current_timer->expiry_tick) != 0U)
        {
            node->prev = current->prev;
            node->next = current;
            node->owner = &g_os_timer_active_list;

            if (current->prev != NULL)
            {
                current->prev->next = node;
            }
            else
            {
                g_os_timer_active_list.head = node;
            }

            current->prev = node;
            g_os_timer_active_list.item_count++;
            timer->active = 1U;
            return;
        }

        current = current->next;
    }

    (void)list_insert_tail(&g_os_timer_active_list, node);
    timer->active = 1U;
}

/**
 * @brief 记录一次到期事件，并在必要时把定时器挂入 expired FIFO。
 *
 * @param timer 当前到期的定时器对象。
 */
static void os_timer_queue_expiration_locked(os_timer_t *timer)
{
    if (timer == NULL)
    {
        return;
    }

    if (timer->pending_expirations < UINT32_MAX)
    {
        timer->pending_expirations++;
    }

    if (timer->expired_node.owner == NULL)
    {
        (void)list_insert_tail(&g_os_timer_expired_list, &timer->expired_node);
    }
}

/**
 * @brief 软件定时器后台任务入口。
 *
 * @param param 预留参数，当前未使用。
 */
static void os_timer_daemon_entry(void *param)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t    primask = 0U;            // 当前临界区保存的 PRIMASK 原值
    list_node_t *node = NULL;            // 从 expired FIFO 摘下来的队头节点
    os_timer_t  *timer = NULL;           // node 对应的外层定时器对象
    uint8_t      should_run_callback = 0U; // 当前这一轮是否还应继续执行 callback

    (void)param;

    for (;;)
    {
        status = os_sem_take(&g_os_timer_work_sem, OS_WAIT_FOREVER);
        if (status != OS_STATUS_OK)
        {
            os_panic(OS_PANIC_TASK_STATE, __FILE__, (uint32_t)__LINE__);
        }

        for (;;)
        {
            primask = os_port_enter_critical();
            node = list_remove_head(&g_os_timer_expired_list);
            if (node == NULL)
            {
                os_port_exit_critical(primask);
                break;
            }

            timer = LIST_CONTAINER_OF(node, os_timer_t, expired_node);
            os_port_exit_critical(primask);

            /* 审查点修复：
             * 不能把 pending_expirations 一次性拷进局部变量后全部消费。
             * 否则 stop() 在 daemon 正处理回调期间把共享计数清零，
             * 也阻止不了“已经复制到本地”的剩余 callback 继续执行。
             * 因此这里每执行一轮 callback 前都重新读取一次共享计数。 */
            for (;;)
            {
                primask = os_port_enter_critical();
                if (timer->pending_expirations > 0U)
                {
                    timer->pending_expirations--;
                    should_run_callback = 1U;
                }
                else
                {
                    should_run_callback = 0U;
                }
                os_port_exit_critical(primask);

                if (should_run_callback == 0U)
                {
                    break;
                }

                timer->callback(timer->arg);
            }
        }
    }
}

/**
 * @brief 建立软件定时器服务全局状态，并创建 timer daemon task。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t os_timer_service_init(void)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t    primask = 0U; // 序列化 first-use init 的外层临界区
    task_init_config_t config = {
        .entry = os_timer_daemon_entry,
        .param = NULL,
        .stack_base = g_os_timer_task_stack,
        .stack_size = OS_TIMER_TASK_STACK_DEPTH,
        .name = OS_TIMER_DAEMON_TASK_NAME,
        .priority = OS_TIMER_TASK_PRIORITY,
        .time_slice = 0U,
    };

    /* 审查点修复：
     * 首次使用 timer service 的初始化必须整段串行化。
     * 在单核 RTOS 中，最直接可靠的方式就是把整个 first-use init
     * 放进最小临界区内，避免两个线程并发第一次调用 os_timer_start()
     * 时重复初始化全局链表、信号量和后台任务。 */
    primask = os_port_enter_critical();

    if (g_os_timer_service_initialized != 0U)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_OK;
    }

    if (OS_TIMER_TASK_PRIORITY >= OS_IDLE_TASK_PRIORITY)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_INVALID_PRIORITY;
    }

    os_timer_service_reset_locked();

    status = os_sem_init(&g_os_timer_work_sem, 0U);
    if (status != OS_STATUS_OK)
    {
        os_timer_service_reset_locked();
        os_port_exit_critical(primask);
        return status;
    }

    status = task_create(&g_os_timer_task, &config);
    if (status == OS_STATUS_SWITCH_REQUIRED)
    {
        os_port_trigger_pendsv();
        status = OS_STATUS_OK;
    }

    if (status != OS_STATUS_OK)
    {
        os_timer_service_reset_locked();
        os_port_exit_critical(primask);
        return status;
    }

    g_os_timer_service_initialized = 1U;
    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 初始化一个静态软件定时器对象。
 *
 * @param timer 调用方提供的定时器对象。
 * @param config 定时器初始化配置。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t os_timer_init(os_timer_t *timer, const os_timer_config_t *config)
{
    if ((timer == NULL) || (config == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if ((config->callback == NULL) || ((config->mode != OS_TIMER_ONE_SHOT) && (config->mode != OS_TIMER_PERIODIC)))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    if (timer->magic == OS_TIMER_MAGIC)
    {
        return OS_STATUS_ALREADY_INITIALIZED;
    }

    (void)memset(timer, 0, sizeof(os_timer_t));
    timer->magic = OS_TIMER_MAGIC;
    timer->name = config->name;
    timer->mode = config->mode;
    timer->callback = config->callback;
    timer->arg = config->arg;
    timer->period_ticks = 0U;
    timer->expiry_tick = 0U;
    timer->active = 0U;
    timer->pending_expirations = 0U;
    list_node_init(&timer->active_node);
    list_node_init(&timer->expired_node);
    return OS_STATUS_OK;
}

/**
 * @brief 启动或重启一个软件定时器。
 *
 * @param timer 目标定时器对象。
 * @param timeout_ticks 首次到期等待 tick 数；periodic 模式同时作为周期。
 *
 * @return os_status_t 启动成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t os_timer_start(os_timer_t *timer, os_tick_t timeout_ticks)
{
    os_status_t status = OS_STATUS_OK;
    uint32_t    primask = 0U; // 当前 start 路径的外层临界区

    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (os_timer_is_valid(timer) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (os_timer_timeout_is_supported(timeout_ticks) == 0U)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    status = os_timer_service_init();
    if (status != OS_STATUS_OK)
    {
        return status;
    }

    primask = os_port_enter_critical();
    os_timer_remove_locked(timer);
    timer->period_ticks = (timer->mode == OS_TIMER_PERIODIC) ? timeout_ticks : 0U;
    timer->expiry_tick = (os_tick_t)(os_tick_get() + timeout_ticks);
    os_timer_active_list_insert_ordered_locked(timer);
    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 停止一个软件定时器，并清空尚未消费的 pending callback。
 *
 * @param timer 目标定时器对象。
 *
 * @return os_status_t 停止成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t os_timer_stop(os_timer_t *timer)
{
    uint32_t primask = 0U; // 当前 stop 路径的外层临界区

    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (os_timer_is_valid(timer) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    if (g_os_timer_service_initialized == 0U)
    {
        return OS_STATUS_OK;
    }

    primask = os_port_enter_critical();
    os_timer_remove_locked(timer);
    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 在 SysTick 路径推进软件定时器 deadline，并唤醒 daemon。
 *
 * @return os_status_t 成功处理返回 OS_STATUS_OK 或 OS_STATUS_NO_CHANGE；失败返回具体错误码。
 */
os_status_t os_timer_system_tick(void)
{
    uint32_t    primask = 0U;         // tick 路径外层临界区
    list_node_t *node = NULL;         // 当前 active list 队头节点
    os_timer_t  *timer = NULL;        // node 对应的外层定时器对象
    os_tick_t    current_tick = 0U;   // 本次 tick 处理读取到的绝对 tick 快照
    uint8_t      expired_any = 0U;    // 非 0 表示本次至少有一个 timer 到期
    os_status_t  status = OS_STATUS_OK; // give-from-isr 的返回值

    if (g_os_timer_service_initialized == 0U)
    {
        return OS_STATUS_NO_CHANGE;
    }

    current_tick = os_tick_get();
    primask = os_port_enter_critical();

    node = g_os_timer_active_list.head;
    while (node != NULL)
    {
        timer = LIST_CONTAINER_OF(node, os_timer_t, active_node);
        if (os_timer_tick_is_due(current_tick, timer->expiry_tick) == 0U)
        {
            break;
        }

        if (list_remove(&g_os_timer_active_list, &timer->active_node) == 0U)
        {
            os_port_exit_critical(primask);
            return OS_STATUS_INVALID_STATE;
        }

        timer->active = 0U;
        os_timer_queue_expiration_locked(timer);
        expired_any = 1U;

        if (timer->mode == OS_TIMER_PERIODIC)
        {
            timer->expiry_tick = (os_tick_t)(timer->expiry_tick + timer->period_ticks);
            os_timer_active_list_insert_ordered_locked(timer);
        }

        node = g_os_timer_active_list.head;
    }

    os_port_exit_critical(primask);

    if (expired_any == 0U)
    {
        return OS_STATUS_NO_CHANGE;
    }

    status = os_sem_give_from_isr(&g_os_timer_work_sem);
    if (status == OS_STATUS_OK)
    {
        return OS_STATUS_OK;
    }

    return status;
}
