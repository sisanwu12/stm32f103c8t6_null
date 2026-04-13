/**
 * @file os_queue.c
 * @author Yukikaze
 * @brief RTOS 最小消息队列实现文件。
 * @version 0.1
 * @date 2026-04-12
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件实现当前阶段使用的最小静态消息队列，以及双等待链表阻塞/唤醒逻辑。
 */

#include <string.h>
#include "internal/os_task_internal.h"
#include "os_port.h"
#include "os_queue.h"

#define OS_QUEUE_TIMEOUT_HALF_RANGE ((os_tick_t)0x80000000UL) // 与任务层相同的半区间限制，保证 deadline 差值比较成立

static uint8_t os_queue_is_valid(const os_queue_t *queue);
static uint8_t os_queue_timeout_is_supported(os_tick_t timeout_ticks);
static uint8_t os_queue_tick_is_due(os_tick_t current_tick, os_tick_t deadline_tick);
static uint8_t *os_queue_get_slot_ptr(const os_queue_t *queue, uint32_t index);
static uint32_t os_queue_advance_index(const os_queue_t *queue, uint32_t index);
static os_status_t os_queue_send_copy_locked(os_queue_t *queue, const void *msg);
static os_status_t os_queue_recv_copy_locked(os_queue_t *queue, void *msg);
static os_status_t os_queue_wake_first_receiver_locked(os_queue_t *queue);
static os_status_t os_queue_wake_first_sender_locked(os_queue_t *queue);

/**
 * @brief 校验一个消息队列对象是否已完成合法初始化。
 *
 * @param queue 待检查的消息队列对象指针。
 *
 * @return uint8_t 非 0 表示对象有效，0 表示对象无效。
 */
static uint8_t os_queue_is_valid(const os_queue_t *queue)
{
    /* 空指针一定不可能是一个合法队列对象。 */
    if (queue == NULL)
    {
        return 0U;
    }

    /* magic 负责识别“该对象是否已经完成过 os_queue_init()”。 */
    if (queue->magic != OS_QUEUE_MAGIC)
    {
        return 0U;
    }

    /* 当前阶段的最小消息队列要求底层字节缓冲区必须存在。 */
    if (queue->buffer == NULL)
    {
        return 0U;
    }

    /* msg_size 与 capacity 都必须是正数，否则槽位计算和空/满语义都不成立。 */
    if ((queue->msg_size == 0U) || (queue->capacity == 0U))
    {
        return 0U;
    }

    /* count 不能超过容量，否则说明队列状态已经损坏。 */
    if (queue->count > queue->capacity)
    {
        return 0U;
    }

    /* head/tail 都必须落在合法槽位范围内。 */
    if ((queue->head_index >= queue->capacity) || (queue->tail_index >= queue->capacity))
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief 判断一个有限超时值是否落在“半个 tick 空间”内。
 *
 * @param timeout_ticks 调用方传入的相对超时 tick 数。
 *
 * @return uint8_t 非 0 表示该超时值可安全用于当前的 deadline 比较逻辑，
 *                 0 表示该值会破坏基于 int32_t 差值的回绕比较前提。
 */
static uint8_t os_queue_timeout_is_supported(os_tick_t timeout_ticks)
{
    /* OS_WAIT_FOREVER 是独立语义，不参与 deadline 数值比较，因此直接允许。 */
    if (timeout_ticks == OS_WAIT_FOREVER)
    {
        return 1U;
    }

    /* 其余有限超时值必须严格落在半个 tick 空间内，
     * 这样 deadline - current_tick 的有符号差值比较才稳定。 */
    return (uint8_t)((timeout_ticks < OS_QUEUE_TIMEOUT_HALF_RANGE) || (timeout_ticks == 0U));
}

/**
 * @brief 判断某个绝对 deadline 是否已经到期。
 *
 * @param current_tick 当前绝对 tick 值。
 * @param deadline_tick 目标 deadline tick。
 *
 * @return uint8_t 非 0 表示 deadline 已到期，0 表示尚未到期。
 */
static uint8_t os_queue_tick_is_due(os_tick_t current_tick, os_tick_t deadline_tick)
{
    /* 这里沿用任务层相同的回绕安全比较方式：
     * 只要 deadline 与 current_tick 的距离始终小于半个 tick 空间，结果就可靠。 */
    return (uint8_t)(((int32_t)(current_tick - deadline_tick)) >= 0);
}

/**
 * @brief 根据槽位下标计算消息缓冲区内的实际地址。
 *
 * @param queue 目标消息队列对象。
 * @param index 槽位下标。
 *
 * @return uint8_t* 指向目标槽位的字节地址；参数非法时返回 NULL。
 */
static uint8_t *os_queue_get_slot_ptr(const os_queue_t *queue, uint32_t index)
{
    /* 无效对象或越界槽位都无法计算出合法消息地址。 */
    if ((os_queue_is_valid(queue) == 0U) || (index >= queue->capacity))
    {
        return NULL;
    }

    /* 槽位地址统一按“buffer + index * msg_size”计算。 */
    return &queue->buffer[index * queue->msg_size];
}

/**
 * @brief 计算某个槽位下标的下一个位置。
 *
 * @param queue 目标消息队列对象。
 * @param index 当前槽位下标。
 *
 * @return uint32_t 环形推进后的下一个槽位下标。
 */
static uint32_t os_queue_advance_index(const os_queue_t *queue, uint32_t index)
{
    /* 该 helper 只会在已验证通过的 queue 上被调用；
     * 这里仍保留保护，避免异常路径下除以 0 或越界。 */
    if ((os_queue_is_valid(queue) == 0U) || (queue->capacity == 0U))
    {
        return 0U;
    }

    /* 每次发送/接收后都按 capacity 做环形推进。 */
    return (uint32_t)((index + 1U) % queue->capacity);
}

/**
 * @brief 在临界区内把一条固定大小消息写入队列尾部。
 *
 * @param queue 目标消息队列对象。
 * @param msg 调用方提供的消息数据地址。
 *
 * @return os_status_t 写入成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t os_queue_send_copy_locked(os_queue_t *queue, const void *msg)
{
    uint8_t *slot = NULL; // 当前应当写入的尾部槽位地址

    /* 复制路径要求 queue 有效，且消息源地址不能为空。 */
    if ((os_queue_is_valid(queue) == 0U) || (msg == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* caller 必须先确认队列未满；若已经满了，再写入会破坏 count 语义。 */
    if (queue->count >= queue->capacity)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 先根据 tail_index 取到实际槽位地址，再做整块 memcpy。 */
    slot = os_queue_get_slot_ptr(queue, queue->tail_index);
    if (slot == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    (void)memcpy(slot, msg, queue->msg_size);

    /* 写入完成后推进 tail_index，并把消息数加 1。 */
    queue->tail_index = os_queue_advance_index(queue, queue->tail_index);
    queue->count++;
    return OS_STATUS_OK;
}

/**
 * @brief 在临界区内从队列头部取出一条固定大小消息。
 *
 * @param queue 目标消息队列对象。
 * @param msg 调用方提供的接收缓冲区地址。
 *
 * @return os_status_t 读取成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
static os_status_t os_queue_recv_copy_locked(os_queue_t *queue, void *msg)
{
    uint8_t *slot = NULL; // 当前应当读取的头部槽位地址

    /* 复制路径要求 queue 有效，且接收缓冲区必须由调用方明确提供。 */
    if ((os_queue_is_valid(queue) == 0U) || (msg == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* caller 必须先确认队列非空；空队列上执行接收会破坏 count 语义。 */
    if (queue->count == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 先取出 head_index 对应槽位地址，再做整块 memcpy。 */
    slot = os_queue_get_slot_ptr(queue, queue->head_index);
    if (slot == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    (void)memcpy(msg, slot, queue->msg_size);

    /* 读取完成后推进 head_index，并把消息数减 1。 */
    queue->head_index = os_queue_advance_index(queue, queue->head_index);
    queue->count--;
    return OS_STATUS_OK;
}

/**
 * @brief 在临界区内唤醒当前最应该接收消息的 waiter。
 *
 * @param queue 目标消息队列对象。
 *
 * @return os_status_t 若唤醒后需要切换，则返回 OS_STATUS_SWITCH_REQUIRED；
 *                     若无需切换，则返回 OS_STATUS_NO_CHANGE；
 *                     出错时返回具体错误码。
 */
static os_status_t os_queue_wake_first_receiver_locked(os_queue_t *queue)
{
    os_status_t status = OS_STATUS_OK;
    tcb_t      *waiter = NULL; // 当前最应该被唤醒的 receiver

    /* 队列对象无效时，不存在合法 receiver 可唤醒。 */
    if (os_queue_is_valid(queue) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* recv_wait_list 的队头就是当前优先级最高、且同优先级最早等待的 receiver。 */
    waiter = task_wait_list_peek_head_task(&queue->recv_wait_list);
    if (waiter == NULL)
    {
        return OS_STATUS_NO_CHANGE;
    }

    status = task_unblock(waiter, TASK_WAIT_RESULT_OBJECT);
    if ((status == OS_STATUS_OK) || (status == OS_STATUS_NO_CHANGE) || (status == OS_STATUS_SWITCH_REQUIRED))
    {
        return status;
    }

    return status;
}

/**
 * @brief 在临界区内唤醒当前最应该发送消息的 waiter。
 *
 * @param queue 目标消息队列对象。
 *
 * @return os_status_t 若唤醒后需要切换，则返回 OS_STATUS_SWITCH_REQUIRED；
 *                     若无需切换，则返回 OS_STATUS_NO_CHANGE；
 *                     出错时返回具体错误码。
 */
static os_status_t os_queue_wake_first_sender_locked(os_queue_t *queue)
{
    os_status_t status = OS_STATUS_OK;
    tcb_t      *waiter = NULL; // 当前最应该被唤醒的 sender

    /* 队列对象无效时，不存在合法 sender 可唤醒。 */
    if (os_queue_is_valid(queue) == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* send_wait_list 的队头就是当前优先级最高、且同优先级最早等待的 sender。 */
    waiter = task_wait_list_peek_head_task(&queue->send_wait_list);
    if (waiter == NULL)
    {
        return OS_STATUS_NO_CHANGE;
    }

    status = task_unblock(waiter, TASK_WAIT_RESULT_OBJECT);
    if ((status == OS_STATUS_OK) || (status == OS_STATUS_NO_CHANGE) || (status == OS_STATUS_SWITCH_REQUIRED))
    {
        return status;
    }

    return status;
}

/**
 * @brief 初始化一个最小静态消息队列对象。
 *
 * @param queue 调用方提供的消息队列对象存储区。
 * @param buffer 调用方提供的底层字节缓冲区起始地址。
 * @param msg_size 单条消息大小，单位为字节。
 * @param capacity 队列容量，表示最多可容纳多少条消息。
 *
 * @return os_status_t 初始化成功返回 OS_STATUS_OK，否则返回具体错误码。
 */
os_status_t os_queue_init(os_queue_t *queue, void *buffer, uint32_t msg_size, uint32_t capacity)
{
    /* queue 对象和底层缓冲区都必须由调用方明确提供。 */
    if ((queue == NULL) || (buffer == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 最小消息队列不支持零大小消息，也不支持零容量。 */
    if ((msg_size == 0U) || (capacity == 0U))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 已初始化对象再次 init 会破坏队列内部状态与等待链表，因此直接拒绝。 */
    if (queue->magic == OS_QUEUE_MAGIC)
    {
        return OS_STATUS_ALREADY_INITIALIZED;
    }

    /* 先清空整个对象，避免旧内容污染索引、计数和等待链表指针。 */
    (void)memset(queue, 0, sizeof(os_queue_t));

    /* 写入最小消息队列的固定元数据：
     * buffer 指向静态字节缓冲区，count 初始为 0，head/tail 都从 0 开始。 */
    queue->magic      = OS_QUEUE_MAGIC;
    queue->buffer     = (uint8_t *)buffer;
    queue->msg_size   = msg_size;
    queue->capacity   = capacity;
    queue->count      = 0U;
    queue->head_index = 0U;
    queue->tail_index = 0U;

    /* 两条等待链表都必须初始化为空，分别承载“发送等待者”和“接收等待者”。 */
    list_init(&queue->send_wait_list);
    list_init(&queue->recv_wait_list);
    return OS_STATUS_OK;
}

/**
 * @brief 在线程态向消息队列发送一条固定大小消息。
 *
 * @param queue 目标消息队列对象。
 * @param msg 调用方提供的发送消息地址。
 * @param timeout_ticks 队列满时的等待超时 tick 数；传 OS_WAIT_FOREVER 表示永久等待。
 *
 * @return os_status_t 发送成功返回 OS_STATUS_OK；
 *                     若队列满且立即失败或等待超时，则返回 OS_STATUS_TIMEOUT；
 *                     失败时返回其他具体错误码。
 */
os_status_t os_queue_send(os_queue_t *queue, const void *msg, os_tick_t timeout_ticks)
{
    os_status_t status = OS_STATUS_OK; // 当前发送路径得到的状态码
    uint32_t    primask = 0U;          // 当前外层临界区保存的 PRIMASK 原值
    tcb_t      *current_task = NULL;   // 当前发起发送的线程对象
    os_tick_t   deadline_tick = 0U;    // 本次发送操作允许阻塞到的绝对 deadline
    os_tick_t   current_tick = 0U;     // 当前循环里读取到的绝对 tick 快照
    os_tick_t   remaining_timeout = 0U; // 传给 task_block_current() 的剩余相对超时
    uint8_t     wait_forever = 0U;     // 非 0 表示本次发送采用永久等待语义

    /* 线程态发送不允许在 ISR 里被直接调用。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 队列对象和消息源地址都必须合法。 */
    if ((os_queue_is_valid(queue) == 0U) || (msg == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 队列层的 deadline 比较与任务层一样依赖“半个 tick 空间”约束，
     * 因此在进入阻塞重试循环前先把 timeout 合法性收紧。 */
    if (os_queue_timeout_is_supported(timeout_ticks) == 0U)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 当前线程对象必须存在，否则说明还没有真正运行在线程上下文里。 */
    current_task = task_get_current();
    if (current_task == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 有限等待要在第一次进入 API 时就固定 deadline，
     * 后续哪怕被对象唤醒后又输给竞争者，也只能继续消耗“剩余等待时间”。 */
    wait_forever = (uint8_t)(timeout_ticks == OS_WAIT_FOREVER);
    if (wait_forever == 0U)
    {
        deadline_tick = (os_tick_t)(os_tick_get() + timeout_ticks);
    }

    for (;;)
    {
        /* 发送路径会同时读写 count/tail_index 和 recv_wait_list/send_wait_list，
         * 因此每次尝试发送都要在临界区内完成。 */
        primask = os_port_enter_critical();

        /* 快速路径：队列未满时，直接把消息写进 tail_index 对应槽位。 */
        if (queue->count < queue->capacity)
        {
            status = os_queue_send_copy_locked(queue, msg);
            if (status != OS_STATUS_OK)
            {
                os_port_exit_critical(primask);
                return status;
            }

            /* 发送成功后，只尝试唤醒“等待接收”的任务，不触碰 sender wait list。 */
            status = os_queue_wake_first_receiver_locked(queue);
            if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
            {
                os_port_exit_critical(primask);
                return status;
            }

            /* 若因此唤醒了更高优先级 receiver，就要在恢复中断前先 pend PendSV。 */
            if (status == OS_STATUS_SWITCH_REQUIRED)
            {
                os_port_trigger_pendsv();
            }

            os_port_exit_critical(primask);
            return OS_STATUS_OK;
        }

        /* 队列已满且本次不是永久等待时，要先把“当前剩余时间”重新结算出来；
         * 这样被唤醒后再次进入循环时，不会重新拿到完整 timeout。 */
        if (wait_forever == 0U)
        {
            current_tick = os_tick_get();

            /* deadline 已到期时，这次重试不能再继续阻塞，按超时返回。 */
            if (os_queue_tick_is_due(current_tick, deadline_tick) != 0U)
            {
                os_port_exit_critical(primask);
                return OS_STATUS_TIMEOUT;
            }

            /* deadline 尚未到时，剩余可等待时间就是“deadline - 当前 tick”。 */
            remaining_timeout = (os_tick_t)(deadline_tick - current_tick);
        }
        else
        {
            /* 永久等待不做 deadline 结算，阻塞接口继续按 FOREVER 语义处理。 */
            remaining_timeout = OS_WAIT_FOREVER;
        }

        /* 慢路径先把当前任务挂进 send_wait_list，再正式提交 TASK_BLOCKED。 */
        status = task_wait_list_insert_priority_ordered(&queue->send_wait_list, current_task);
        if (status != OS_STATUS_OK)
        {
            os_port_exit_critical(primask);
            return status;
        }

        status = task_block_current(queue, remaining_timeout, NULL);
        if (status != OS_STATUS_OK)
        {
            /* 若在真正阻塞前失败，要把刚刚挂进 send_wait_list 的 event_node 立即摘掉。 */
            task_wait_list_remove_task(&queue->send_wait_list, current_task);
            os_port_exit_critical(primask);
            return status;
        }

        /* 到这里为止，阻塞路径已经把 PendSV 与等待态提交好；
         * 退出临界区后，当前线程可能立刻切走，也可能在未来恢复后从这里继续。 */
        os_port_exit_critical(primask);

        /* 当前任务恢复后，先看是不是 timeout 唤醒；
         * 若不是 timeout，就回到循环顶部继续用“剩余时间”重试发送。 */
        current_task = task_get_current();
        if (current_task == NULL)
        {
            return OS_STATUS_INVALID_STATE;
        }

        if (current_task->wait_result == TASK_WAIT_RESULT_TIMEOUT)
        {
            return OS_STATUS_TIMEOUT;
        }
    }
}

/**
 * @brief 在线程态从消息队列接收一条固定大小消息。
 *
 * @param queue 目标消息队列对象。
 * @param msg 调用方提供的接收缓冲区地址。
 * @param timeout_ticks 队列空时的等待超时 tick 数；传 OS_WAIT_FOREVER 表示永久等待。
 *
 * @return os_status_t 接收成功返回 OS_STATUS_OK；
 *                     若队列空且立即失败或等待超时，则返回 OS_STATUS_TIMEOUT；
 *                     失败时返回其他具体错误码。
 */
os_status_t os_queue_recv(os_queue_t *queue, void *msg, os_tick_t timeout_ticks)
{
    os_status_t status = OS_STATUS_OK; // 当前接收路径得到的状态码
    uint32_t    primask = 0U;          // 当前外层临界区保存的 PRIMASK 原值
    tcb_t      *current_task = NULL;   // 当前发起接收的线程对象
    os_tick_t   deadline_tick = 0U;    // 本次接收操作允许阻塞到的绝对 deadline
    os_tick_t   current_tick = 0U;     // 当前循环里读取到的绝对 tick 快照
    os_tick_t   remaining_timeout = 0U; // 传给 task_block_current() 的剩余相对超时
    uint8_t     wait_forever = 0U;     // 非 0 表示本次接收采用永久等待语义

    /* 线程态接收不允许在 ISR 中被直接调用。 */
    if (os_port_is_in_interrupt() != 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 队列对象和接收缓冲区都必须合法。 */
    if ((os_queue_is_valid(queue) == 0U) || (msg == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 队列层的 deadline 比较同样依赖“半个 tick 空间”约束，
     * 因此在进入阻塞重试循环前先把 timeout 合法性收紧。 */
    if (os_queue_timeout_is_supported(timeout_ticks) == 0U)
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* 当前线程对象必须存在，否则说明还没有真正运行在线程上下文里。 */
    current_task = task_get_current();
    if (current_task == NULL)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 有限等待在第一次进入 API 时就固定 deadline，
     * 这样被对象唤醒但又因为竞争没拿到消息时，后续只会继续消耗剩余时间。 */
    wait_forever = (uint8_t)(timeout_ticks == OS_WAIT_FOREVER);
    if (wait_forever == 0U)
    {
        deadline_tick = (os_tick_t)(os_tick_get() + timeout_ticks);
    }

    for (;;)
    {
        /* 接收路径会同时读写 count/head_index 和 recv_wait_list/send_wait_list，
         * 因此每次尝试接收都要在临界区内完成。 */
        primask = os_port_enter_critical();

        /* 快速路径：队列非空时，直接从 head_index 对应槽位取出一条消息。 */
        if (queue->count > 0U)
        {
            status = os_queue_recv_copy_locked(queue, msg);
            if (status != OS_STATUS_OK)
            {
                os_port_exit_critical(primask);
                return status;
            }

            /* 接收成功后，只尝试唤醒“等待发送”的任务，不触碰 receiver wait list。 */
            status = os_queue_wake_first_sender_locked(queue);
            if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
            {
                os_port_exit_critical(primask);
                return status;
            }

            /* 若因此唤醒了更高优先级 sender，就要在恢复中断前先 pend PendSV。 */
            if (status == OS_STATUS_SWITCH_REQUIRED)
            {
                os_port_trigger_pendsv();
            }

            os_port_exit_critical(primask);
            return OS_STATUS_OK;
        }

        /* 队列为空且本次不是永久等待时，要先把“当前剩余时间”重新结算出来，
         * 避免在多次重试中重复拿到完整 timeout。 */
        if (wait_forever == 0U)
        {
            current_tick = os_tick_get();

            /* deadline 已到期时，这次重试不能再继续阻塞，按超时返回。 */
            if (os_queue_tick_is_due(current_tick, deadline_tick) != 0U)
            {
                os_port_exit_critical(primask);
                return OS_STATUS_TIMEOUT;
            }

            /* deadline 尚未到时，剩余可等待时间就是“deadline - 当前 tick”。 */
            remaining_timeout = (os_tick_t)(deadline_tick - current_tick);
        }
        else
        {
            /* 永久等待不做 deadline 结算，阻塞接口继续按 FOREVER 语义处理。 */
            remaining_timeout = OS_WAIT_FOREVER;
        }

        /* 慢路径先把当前任务挂进 recv_wait_list，再正式提交 TASK_BLOCKED。 */
        status = task_wait_list_insert_priority_ordered(&queue->recv_wait_list, current_task);
        if (status != OS_STATUS_OK)
        {
            os_port_exit_critical(primask);
            return status;
        }

        status = task_block_current(queue, remaining_timeout, NULL);
        if (status != OS_STATUS_OK)
        {
            /* 若在真正阻塞前失败，要把刚刚挂进 recv_wait_list 的 event_node 立即摘掉。 */
            task_wait_list_remove_task(&queue->recv_wait_list, current_task);
            os_port_exit_critical(primask);
            return status;
        }

        /* 阻塞路径已完成等待态提交；退出临界区后，当前线程可能立刻切走。 */
        os_port_exit_critical(primask);

        /* 当前任务恢复后，先看是不是 timeout 唤醒；
         * 若不是 timeout，就回到循环顶部继续用“剩余时间”重试接收。 */
        current_task = task_get_current();
        if (current_task == NULL)
        {
            return OS_STATUS_INVALID_STATE;
        }

        if (current_task->wait_result == TASK_WAIT_RESULT_TIMEOUT)
        {
            return OS_STATUS_TIMEOUT;
        }
    }
}

/**
 * @brief 在 ISR 中向消息队列发送一条固定大小消息。
 *
 * @param queue 目标消息队列对象。
 * @param msg 调用方提供的发送消息地址。
 *
 * @return os_status_t 发送成功返回 OS_STATUS_OK；
 *                     若队列满导致立即失败，则返回 OS_STATUS_TIMEOUT；
 *                     失败时返回其他具体错误码。
 */
os_status_t os_queue_send_from_isr(os_queue_t *queue, const void *msg)
{
    os_status_t status = OS_STATUS_OK; // 当前 ISR 发送路径得到的状态码
    uint32_t    primask = 0U;          // 当前外层临界区保存的 PRIMASK 原值

    /* from-isr 版本只允许在异常/中断上下文里调用。 */
    if (os_port_is_in_interrupt() == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 队列对象和消息源地址都必须合法。 */
    if ((os_queue_is_valid(queue) == 0U) || (msg == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* ISR 发送同样会读写 count/tail_index 和 recv_wait_list，
     * 所以这里也要在临界区内完成。 */
    primask = os_port_enter_critical();

    /* ISR 不允许阻塞；队列满时直接按“立即失败”语义返回 TIMEOUT。 */
    if (queue->count >= queue->capacity)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_TIMEOUT;
    }

    status = os_queue_send_copy_locked(queue, msg);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* ISR 发送成功后，同样只尝试唤醒 receiver。 */
    status = os_queue_wake_first_receiver_locked(queue);
    if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 若因此唤醒了更高优先级 receiver，就在 ISR 内部直接 pend PendSV。 */
    if (status == OS_STATUS_SWITCH_REQUIRED)
    {
        os_port_trigger_pendsv();
    }

    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}

/**
 * @brief 在 ISR 中从消息队列接收一条固定大小消息。
 *
 * @param queue 目标消息队列对象。
 * @param msg 调用方提供的接收缓冲区地址。
 *
 * @return os_status_t 接收成功返回 OS_STATUS_OK；
 *                     若队列空导致立即失败，则返回 OS_STATUS_TIMEOUT；
 *                     失败时返回其他具体错误码。
 */
os_status_t os_queue_recv_from_isr(os_queue_t *queue, void *msg)
{
    os_status_t status = OS_STATUS_OK; // 当前 ISR 接收路径得到的状态码
    uint32_t    primask = 0U;          // 当前外层临界区保存的 PRIMASK 原值

    /* from-isr 版本只允许在异常/中断上下文里调用。 */
    if (os_port_is_in_interrupt() == 0U)
    {
        return OS_STATUS_INVALID_STATE;
    }

    /* 队列对象和接收缓冲区都必须合法。 */
    if ((os_queue_is_valid(queue) == 0U) || (msg == NULL))
    {
        return OS_STATUS_INVALID_PARAM;
    }

    /* ISR 接收同样会读写 count/head_index 和 send_wait_list，
     * 所以这里也要在临界区内完成。 */
    primask = os_port_enter_critical();

    /* ISR 不允许阻塞；队列空时直接按“立即失败”语义返回 TIMEOUT。 */
    if (queue->count == 0U)
    {
        os_port_exit_critical(primask);
        return OS_STATUS_TIMEOUT;
    }

    status = os_queue_recv_copy_locked(queue, msg);
    if (status != OS_STATUS_OK)
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* ISR 接收成功后，同样只尝试唤醒 sender。 */
    status = os_queue_wake_first_sender_locked(queue);
    if ((status != OS_STATUS_OK) && (status != OS_STATUS_NO_CHANGE) && (status != OS_STATUS_SWITCH_REQUIRED))
    {
        os_port_exit_critical(primask);
        return status;
    }

    /* 若因此唤醒了更高优先级 sender，就在 ISR 内部直接 pend PendSV。 */
    if (status == OS_STATUS_SWITCH_REQUIRED)
    {
        os_port_trigger_pendsv();
    }

    os_port_exit_critical(primask);
    return OS_STATUS_OK;
}
