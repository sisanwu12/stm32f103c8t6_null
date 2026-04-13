/**
 * @file main.c
 * @author Yukikaze
 * @brief RTOS 板上 smoke / regression 入口。
 * @version 0.1
 * @date 2026-04-13
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件用于承载当前阶段的最小板上回归框架：
 *       1. 编译期单 case 选择；
 *       2. PA0 GPIO 脉冲打点；
 *       3. panic hook 输出 panic reason。
 */

#include <string.h>
#include "stm32f1xx.h"
#include "dri_ll_gpio.h"
#include "dri_ll_rcc.h"
#include "os.h"

/* regression 入口默认只编译 case 1。
 * 若要切换其他场景，只需在构建命令里覆盖 RTOS_REGRESSION_CASE。 */
#ifndef RTOS_REGRESSION_CASE
    #define RTOS_REGRESSION_CASE 1U
#endif

/* 某些 helper 只会在部分 case 中使用。
 * 显式标成 unused，避免切换回归场景时产生无意义告警。 */
#if defined(__GNUC__) || defined(__clang__)
    #define REGRESSION_MAYBE_UNUSED __attribute__((unused))
#else
    #define REGRESSION_MAYBE_UNUSED
#endif

/* 当前 SystemInit() 会把系统时钟切到 72MHz PLL，回归入口沿用同一频率。 */
#define REGRESSION_CPU_CLOCK_HZ      72000000UL
/* PA0 只承担长/短脉冲输出，因此这里用忙等控制脉宽。 */
#define REGRESSION_PROBE_SHORT_LOOPS 30000UL
#define REGRESSION_PROBE_LONG_LOOPS  120000UL

static void regression_probe_init(void);
static void regression_busy_delay(uint32_t loops);
static void regression_emit_short_pulse(void);
static void regression_emit_long_pulse(void);
static void regression_emit_case_start(uint32_t case_id);
static void regression_emit_fail_pattern(void);
static void regression_emit_pass_pattern(void);
static void regression_halt(void);
static void regression_fail(void);
static REGRESSION_MAYBE_UNUSED void regression_pass(void);
static REGRESSION_MAYBE_UNUSED void regression_expect_ok(os_status_t status);
static void regression_panic_hook(const os_panic_info_t *info);
static REGRESSION_MAYBE_UNUSED void regression_kernel_start(void);

void os_port_app_systick_hook(void);

/**
 * @brief 初始化回归观测 GPIO：PA0。
 *
 * @note 这里直接走 LL 层，避免为了回归入口再依赖额外应用封装。
 */
static void regression_probe_init(void)
{
    dri_ll_gpio_init_t cfg = { // PA0 输出引脚初始化配置
        .port = GPIO_PORT_A,
        .pin = GPIO_PIN_0,
        .mode = GPIO_MODE_OUTPUT_2,
        .cnf = GPIO_CNF_OUTPUT_PP,
        .level = GPIO_LEVEL_LOW,
    };

    dri_ll_rcc_apb2_enable(RCC_APB2_GPIOA);
    /* 若观测引脚初始化失败，当前回归入口已经失去唯一输出通道，
     * 因此这里直接停机暴露问题。 */
    if (dri_ll_gpio_init(&cfg) != GPIO_INIT_SUCCESS)
    {
        while (1)
        {
        }
    }

    dri_ll_gpio_reset_pin(GPIO_PORT_A, GPIO_PIN_0);
}

/**
 * @brief 简单忙等，用于 GPIO 脉冲间隔。
 *
 * @param loops 忙等循环次数。
 */
static void regression_busy_delay(uint32_t loops)
{
    while (loops > 0U)
    {
        __asm volatile("nop");
        loops--;
    }
}

/**
 * @brief 输出一个短脉冲。
 */
static void regression_emit_short_pulse(void)
{
    dri_ll_gpio_set_pin(GPIO_PORT_A, GPIO_PIN_0);
    regression_busy_delay(REGRESSION_PROBE_SHORT_LOOPS);
    dri_ll_gpio_reset_pin(GPIO_PORT_A, GPIO_PIN_0);
    regression_busy_delay(REGRESSION_PROBE_SHORT_LOOPS);
}

/**
 * @brief 输出一个长脉冲。
 */
static void regression_emit_long_pulse(void)
{
    dri_ll_gpio_set_pin(GPIO_PORT_A, GPIO_PIN_0);
    regression_busy_delay(REGRESSION_PROBE_LONG_LOOPS);
    dri_ll_gpio_reset_pin(GPIO_PORT_A, GPIO_PIN_0);
    regression_busy_delay(REGRESSION_PROBE_LONG_LOOPS);
}

/**
 * @brief 在 case 开始前输出 case 编号。
 *
 * @param case_id 当前回归 case 编号。
 *
 * @note 固定协议：输出 case_id 个短脉冲，再留一段更长的空隙。
 */
static void regression_emit_case_start(uint32_t case_id)
{
    uint32_t index = 0U; // 当前已经输出的短脉冲数量

    for (index = 0U; index < case_id; index++)
    {
        regression_emit_short_pulse();
    }

    regression_busy_delay(REGRESSION_PROBE_LONG_LOOPS);
}

/**
 * @brief 输出失败图样。
 */
static void regression_emit_fail_pattern(void)
{
    regression_emit_long_pulse();
    regression_emit_long_pulse();
    regression_emit_long_pulse();
}

/**
 * @brief 输出成功图样。
 */
static void regression_emit_pass_pattern(void)
{
    regression_emit_long_pulse();
}

/**
 * @brief 停机并保持引脚低电平。
 */
static void regression_halt(void)
{
    __disable_irq();
    dri_ll_gpio_reset_pin(GPIO_PORT_A, GPIO_PIN_0);

    while (1)
    {
    }
}

/**
 * @brief 以固定失败图样结束当前回归 case。
 */
static void regression_fail(void)
{
    __disable_irq();
    regression_emit_fail_pattern();
    regression_halt();
}

/**
 * @brief 以固定成功图样结束当前回归 case。
 */
static REGRESSION_MAYBE_UNUSED void regression_pass(void)
{
    __disable_irq();
    regression_emit_pass_pattern();
    regression_halt();
}

/**
 * @brief 断言某个 RTOS 接口返回成功，否则按失败结束。
 *
 * @param status 待检查的返回值。
 *
 * @note 当前 regression 采用“第一处失败立即停机”的策略，
 *       避免后续现象被前序错误串扰。
 */
static REGRESSION_MAYBE_UNUSED void regression_expect_ok(os_status_t status)
{
    if (status != OS_STATUS_OK)
    {
        regression_fail();
    }
}

/**
 * @brief panic hook：在停机前先输出 panic reason 编号。
 *
 * @param info panic 现场信息。
 *
 * @note 本阶段只要求板上可见性，因此这里不做复杂 dump，只输出 reason 编号。
 */
static void regression_panic_hook(const os_panic_info_t *info)
{
    uint32_t reason_id = 0U; // 当前 panic reason 对应的脉冲数量
    uint32_t index = 0U;     // 已经输出的短脉冲数量

    if (info == NULL)
    {
        return;
    }

    reason_id = (uint32_t)info->reason + 1U;
    for (index = 0U; index < reason_id; index++)
    {
        regression_emit_short_pulse();
    }
}

/**
 * @brief 启动内核；若异常返回，则按失败处理。
 */
static REGRESSION_MAYBE_UNUSED void regression_kernel_start(void)
{
    if (os_kernel_start(REGRESSION_CPU_CLOCK_HZ) != OS_STATUS_OK)
    {
        regression_fail();
    }
}

void os_port_app_systick_hook(void)
{
#if (RTOS_REGRESSION_CASE == 5U)
    extern os_sem_t g_case5_sem;
    extern volatile uint32_t g_case5_tick_count;
    extern volatile uint8_t g_case5_isr_give_done;

    /* case 5 需要验证 give-from-isr 路径。
     * 这里复用 SysTick 的应用 hook，在第 10 个 tick 时人工模拟一次 ISR give。 */
    g_case5_tick_count++;
    if ((g_case5_isr_give_done == 0U) && (g_case5_tick_count >= 10U))
    {
        if (os_sem_give_from_isr(&g_case5_sem) == OS_STATUS_OK)
        {
            g_case5_isr_give_done = 1U;
        }
    }
#endif
}

/* ==================== Case 1: 同优先级时间片轮转 ==================== */
#if (RTOS_REGRESSION_CASE == 1U)
static volatile uint32_t g_case1_counter_a = 0U;
static volatile uint32_t g_case1_counter_b = 0U;
static os_task_t g_case1_task_a;
static os_task_t g_case1_task_b;
static os_task_t g_case1_checker;
static uint32_t g_case1_stack_a[64];
static uint32_t g_case1_stack_b[64];
static uint32_t g_case1_checker_stack[64];

static void case1_worker_a(void *param)
{
    (void)param;
    for (;;)
    {
        g_case1_counter_a++;
    }
}

static void case1_worker_b(void *param)
{
    (void)param;
    for (;;)
    {
        g_case1_counter_b++;
    }
}

static void case1_checker(void *param)
{
    const char *name = NULL;      // 读回 task_a 的名称指针
    os_task_state_t state = TASK_DELETED; // 读回 task_a 的状态快照
    uint8_t priority = 0U;        // 读回 task_a 的当前生效优先级

    (void)param;

    regression_expect_ok(os_task_delay(80U));
    regression_expect_ok(os_task_name_get(&g_case1_task_a, &name));
    regression_expect_ok(os_task_state_get(&g_case1_task_a, &state));
    regression_expect_ok(os_task_priority_get(&g_case1_task_a, &priority));

    if ((name == NULL) || (strcmp(name, "rr_a") != 0))
    {
        regression_fail();
    }

    /* checker 观察 task_a 时，task_a 可能正好在运行，也可能刚被轮转回 READY。 */
    if ((state != TASK_READY) && (state != TASK_RUNNING))
    {
        regression_fail();
    }

    if (priority != 5U)
    {
        regression_fail();
    }

    if ((g_case1_counter_a == 0U) || (g_case1_counter_b == 0U))
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    cfg.entry = case1_worker_a;
    cfg.param = NULL;
    cfg.stack_base = g_case1_stack_a;
    cfg.stack_size = 64U;
    cfg.name = "rr_a";
    cfg.priority = 5U;
    cfg.time_slice = 1U;
    regression_expect_ok(os_task_create(&g_case1_task_a, &cfg));

    cfg.entry = case1_worker_b;
    cfg.stack_base = g_case1_stack_b;
    cfg.name = "rr_b";
    regression_expect_ok(os_task_create(&g_case1_task_b, &cfg));

    cfg.entry = case1_checker;
    cfg.stack_base = g_case1_checker_stack;
    cfg.name = "rr_chk";
    cfg.priority = 4U;
    cfg.time_slice = 1U;
    regression_expect_ok(os_task_create(&g_case1_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 2U)
/* ==================== Case 2: 高优先级唤醒抢占 ==================== */
static volatile uint32_t g_case2_low_counter = 0U;
static volatile uint8_t  g_case2_high_ran = 0U;
static os_task_t g_case2_low_task;
static os_task_t g_case2_high_task;
static os_task_t g_case2_checker;
static uint32_t g_case2_low_stack[64];
static uint32_t g_case2_high_stack[64];
static uint32_t g_case2_checker_stack[64];

static void case2_low_task(void *param)
{
    (void)param;
    for (;;)
    {
        g_case2_low_counter++;
    }
}

static void case2_high_task(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(20U));
    g_case2_high_ran = 1U;
    for (;;)
    {
    }
}

static void case2_checker(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(40U));

    if ((g_case2_low_counter == 0U) || (g_case2_high_ran == 0U))
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    cfg.entry = case2_low_task;
    cfg.stack_base = g_case2_low_stack;
    cfg.stack_size = 64U;
    cfg.name = "preempt_low";
    cfg.priority = 6U;
    regression_expect_ok(os_task_create(&g_case2_low_task, &cfg));

    cfg.entry = case2_high_task;
    cfg.stack_base = g_case2_high_stack;
    cfg.name = "preempt_high";
    cfg.priority = 3U;
    regression_expect_ok(os_task_create(&g_case2_high_task, &cfg));

    cfg.entry = case2_checker;
    cfg.stack_base = g_case2_checker_stack;
    cfg.name = "preempt_chk";
    cfg.priority = 2U;
    regression_expect_ok(os_task_create(&g_case2_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 3U)
/* ==================== Case 3: task_delay timeout 恢复 ==================== */
static volatile uint8_t  g_case3_done = 0U;
static volatile os_tick_t g_case3_elapsed = 0U;
static os_task_t g_case3_worker;
static os_task_t g_case3_checker;
static uint32_t g_case3_worker_stack[64];
static uint32_t g_case3_checker_stack[64];

static void case3_worker(void *param)
{
    os_tick_t start_tick = 0U; // 进入 delay 前记录的起始 tick

    (void)param;

    start_tick = os_kernel_tick_get();
    regression_expect_ok(os_task_delay(15U));
    g_case3_elapsed = (os_tick_t)(os_kernel_tick_get() - start_tick);
    g_case3_done = 1U;

    for (;;)
    {
    }
}

static void case3_checker(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(30U));

    if ((g_case3_done == 0U) || (g_case3_elapsed < 15U))
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    cfg.entry = case3_worker;
    cfg.stack_base = g_case3_worker_stack;
    cfg.stack_size = 64U;
    cfg.name = "delay_worker";
    cfg.priority = 5U;
    regression_expect_ok(os_task_create(&g_case3_worker, &cfg));

    cfg.entry = case3_checker;
    cfg.stack_base = g_case3_checker_stack;
    cfg.name = "delay_chk";
    cfg.priority = 4U;
    regression_expect_ok(os_task_create(&g_case3_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 4U)
/* ==================== Case 4: task_delay_until 周期稳定性 ==================== */
static volatile uint8_t  g_case4_done = 0U;
static volatile os_tick_t g_case4_timestamps[4] = {0U};
static os_task_t g_case4_worker;
static os_task_t g_case4_checker;
static uint32_t g_case4_worker_stack[64];
static uint32_t g_case4_checker_stack[64];

static void case4_worker(void *param)
{
    os_tick_t previous = 0U; // delay_until 使用的上一轮唤醒基准
    uint32_t  index = 0U;    // 当前已经完成了多少轮周期等待

    (void)param;

    previous = os_kernel_tick_get();
    for (index = 0U; index < 4U; index++)
    {
        regression_expect_ok(os_task_delay_until(&previous, 10U));
        g_case4_timestamps[index] = os_kernel_tick_get();
    }

    g_case4_done = 1U;
    for (;;)
    {
    }
}

static void case4_checker(void *param)
{
    uint32_t index = 0U; // 当前检查的是哪一对相邻时间戳
    os_tick_t delta = 0U; // 相邻两次唤醒之间的 tick 间隔

    (void)param;
    regression_expect_ok(os_task_delay(60U));

    if (g_case4_done == 0U)
    {
        regression_fail();
    }

    for (index = 1U; index < 4U; index++)
    {
        delta = (os_tick_t)(g_case4_timestamps[index] - g_case4_timestamps[index - 1U]);
        /* 目标周期是 10 tick；这里把 10~12 tick 视为可接受抖动窗口。 */
        if ((delta < 10U) || (delta > 12U))
        {
            regression_fail();
        }
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    cfg.entry = case4_worker;
    cfg.stack_base = g_case4_worker_stack;
    cfg.stack_size = 64U;
    cfg.name = "delay_until";
    cfg.priority = 5U;
    regression_expect_ok(os_task_create(&g_case4_worker, &cfg));

    cfg.entry = case4_checker;
    cfg.stack_base = g_case4_checker_stack;
    cfg.name = "delay_until_chk";
    cfg.priority = 4U;
    regression_expect_ok(os_task_create(&g_case4_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 5U)
/* ==================== Case 5: os_sem_give_from_isr ==================== */
volatile uint32_t g_case5_tick_count = 0U;
volatile uint8_t  g_case5_isr_give_done = 0U;
static volatile uint8_t  g_case5_took = 0U;
os_sem_t g_case5_sem;
static os_task_t g_case5_waiter;
static os_task_t g_case5_checker;
static uint32_t g_case5_waiter_stack[64];
static uint32_t g_case5_checker_stack[64];

static void case5_waiter(void *param)
{
    (void)param;
    regression_expect_ok(os_sem_take(&g_case5_sem, OS_WAIT_FOREVER));
    g_case5_took = 1U;
    for (;;)
    {
    }
}

static void case5_checker(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(30U));

    if ((g_case5_took == 0U) || (g_case5_isr_give_done == 0U))
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    regression_expect_ok(os_sem_init(&g_case5_sem, 0U));

    cfg.entry = case5_waiter;
    cfg.stack_base = g_case5_waiter_stack;
    cfg.stack_size = 64U;
    cfg.name = "sem_isr_wait";
    cfg.priority = 5U;
    regression_expect_ok(os_task_create(&g_case5_waiter, &cfg));

    cfg.entry = case5_checker;
    cfg.stack_base = g_case5_checker_stack;
    cfg.name = "sem_isr_chk";
    cfg.priority = 4U;
    regression_expect_ok(os_task_create(&g_case5_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 6U)
/* ==================== Case 6: queue timeout / retry ==================== */
static os_queue_t g_case6_queue;
static uint32_t   g_case6_queue_buffer[4] = {0U};
static volatile uint8_t  g_case6_first_timeout = 0U;
static volatile uint8_t  g_case6_second_success = 0U;
static volatile uint32_t g_case6_value = 0U;
static os_task_t g_case6_receiver;
static os_task_t g_case6_sender;
static os_task_t g_case6_checker;
static uint32_t g_case6_receiver_stack[64];
static uint32_t g_case6_sender_stack[64];
static uint32_t g_case6_checker_stack[64];

static void case6_receiver(void *param)
{
    os_status_t status = OS_STATUS_OK; // 每次接收尝试得到的返回值
    uint32_t    value = 0U;            // 接收到的队列消息缓存

    (void)param;

    status = os_queue_recv(&g_case6_queue, &value, 10U);
    if (status == OS_STATUS_TIMEOUT)
    {
        g_case6_first_timeout = 1U;
    }

    status = os_queue_recv(&g_case6_queue, &value, 30U);
    if (status == OS_STATUS_OK)
    {
        g_case6_second_success = 1U;
        g_case6_value = value;
    }

    for (;;)
    {
    }
}

static void case6_sender(void *param)
{
    uint32_t value = 0x12345678UL; // 第二次接收应当拿到的测试消息

    (void)param;
    regression_expect_ok(os_task_delay(15U));
    regression_expect_ok(os_queue_send(&g_case6_queue, &value, OS_WAIT_FOREVER));

    for (;;)
    {
    }
}

static void case6_checker(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(60U));

    if ((g_case6_first_timeout == 0U) || (g_case6_second_success == 0U) || (g_case6_value != 0x12345678UL))
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    regression_expect_ok(os_queue_init(&g_case6_queue, g_case6_queue_buffer, sizeof(uint32_t), 4U));

    cfg.entry = case6_receiver;
    cfg.stack_base = g_case6_receiver_stack;
    cfg.stack_size = 64U;
    cfg.name = "queue_rx";
    cfg.priority = 5U;
    regression_expect_ok(os_task_create(&g_case6_receiver, &cfg));

    cfg.entry = case6_sender;
    cfg.stack_base = g_case6_sender_stack;
    cfg.name = "queue_tx";
    cfg.priority = 6U;
    regression_expect_ok(os_task_create(&g_case6_sender, &cfg));

    cfg.entry = case6_checker;
    cfg.stack_base = g_case6_checker_stack;
    cfg.name = "queue_chk";
    cfg.priority = 4U;
    regression_expect_ok(os_task_create(&g_case6_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 7U)
/* ==================== Case 7: mutex priority inheritance ==================== */
static os_mutex_t g_case7_mutex;
static volatile uint8_t g_case7_allow_unlock = 0U;
static volatile uint8_t g_case7_high_got_lock = 0U;
static os_task_t g_case7_low_task;
static os_task_t g_case7_high_task;
static os_task_t g_case7_checker;
static uint32_t g_case7_low_stack[64];
static uint32_t g_case7_high_stack[64];
static uint32_t g_case7_checker_stack[64];

static void case7_low_owner(void *param)
{
    (void)param;
    regression_expect_ok(os_mutex_lock(&g_case7_mutex, OS_WAIT_FOREVER));

    while (g_case7_allow_unlock == 0U)
    {
    }

    regression_expect_ok(os_mutex_unlock(&g_case7_mutex));
    for (;;)
    {
    }
}

static void case7_high_waiter(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(5U));
    regression_expect_ok(os_mutex_lock(&g_case7_mutex, OS_WAIT_FOREVER));
    g_case7_high_got_lock = 1U;
    regression_expect_ok(os_mutex_unlock(&g_case7_mutex));

    for (;;)
    {
    }
}

static void case7_checker(void *param)
{
    uint8_t effective = 0U; // low owner 在 waiter 存在时的 effective priority

    (void)param;
    regression_expect_ok(os_task_delay(15U));
    regression_expect_ok(os_task_priority_get(&g_case7_low_task, &effective));
    if (effective != 3U)
    {
        regression_fail();
    }

    g_case7_allow_unlock = 1U;
    regression_expect_ok(os_task_delay(10U));

    if (g_case7_high_got_lock == 0U)
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    regression_expect_ok(os_mutex_init(&g_case7_mutex));

    cfg.entry = case7_low_owner;
    cfg.stack_base = g_case7_low_stack;
    cfg.stack_size = 64U;
    cfg.name = "mtx_low";
    cfg.priority = 6U;
    regression_expect_ok(os_task_create(&g_case7_low_task, &cfg));

    cfg.entry = case7_high_waiter;
    cfg.stack_base = g_case7_high_stack;
    cfg.name = "mtx_high";
    cfg.priority = 3U;
    regression_expect_ok(os_task_create(&g_case7_high_task, &cfg));

    cfg.entry = case7_checker;
    cfg.stack_base = g_case7_checker_stack;
    cfg.name = "mtx_chk";
    cfg.priority = 2U;
    regression_expect_ok(os_task_create(&g_case7_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 8U)
/* ==================== Case 8: task_base_priority_set 立即抢占 ==================== */
static volatile uint8_t g_case8_sequence = 0U;
static volatile uint8_t g_case8_observed = 0U;
static volatile uint8_t g_case8_preempted = 0U;
static os_task_t g_case8_setter_task;
static os_task_t g_case8_target_task;
static os_task_t g_case8_checker;
static uint32_t g_case8_setter_stack[64];
static uint32_t g_case8_target_stack[64];
static uint32_t g_case8_checker_stack[64];

static void case8_setter(void *param)
{
    (void)param;
    g_case8_sequence = 1U;
    regression_expect_ok(os_task_base_priority_set(&g_case8_target_task, 4U));
    g_case8_sequence = 2U;

    for (;;)
    {
    }
}

static void case8_target(void *param)
{
    (void)param;
    for (;;)
    {
        if (g_case8_observed == 0U)
        {
            /* 若 target 第一次观察到的是 sequence=1，
             * 说明 setter 在 task_base_priority_set() 内部就被立即抢占了。 */
            if (g_case8_sequence == 1U)
            {
                g_case8_preempted = 1U;
                g_case8_observed = 1U;
            }
            /* 若 target 第一次观察到的已经是 2，说明 setter 一路跑完都没有被立即抢占。 */
            else if (g_case8_sequence == 2U)
            {
                g_case8_preempted = 0U;
                g_case8_observed = 1U;
            }
        }
    }
}

static void case8_checker(void *param)
{
    uint8_t base_priority = 0U; // 读回 target 任务的新 base priority

    (void)param;
    regression_expect_ok(os_task_delay(20U));
    regression_expect_ok(os_task_base_priority_get(&g_case8_target_task, &base_priority));

    if ((g_case8_observed == 0U) || (g_case8_preempted == 0U) || (base_priority != 4U))
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    cfg.entry = case8_setter;
    cfg.stack_base = g_case8_setter_stack;
    cfg.stack_size = 64U;
    cfg.name = "prio_setter";
    cfg.priority = 5U;
    regression_expect_ok(os_task_create(&g_case8_setter_task, &cfg));

    cfg.entry = case8_target;
    cfg.stack_base = g_case8_target_stack;
    cfg.name = "prio_target";
    cfg.priority = 6U;
    regression_expect_ok(os_task_create(&g_case8_target_task, &cfg));

    cfg.entry = case8_checker;
    cfg.stack_base = g_case8_checker_stack;
    cfg.name = "prio_chk";
    cfg.priority = 2U;
    regression_expect_ok(os_task_create(&g_case8_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 9U)
/* ==================== Case 9: stack sentinel panic ==================== */
static os_task_t g_case9_task;
static uint32_t g_case9_stack[64];

static void case9_corrupt_sentinel(void *param)
{
    os_task_t *current = NULL; // 当前正在运行的任务对象

    (void)param;
    current = os_task_current_get();
    if (current == NULL)
    {
        regression_fail();
    }

    /* 直接破坏栈底哨兵字，等待下一个 SysTick 走到 sentinel 检查时触发 panic。 */
    current->stack_base[0] = 0U;
    for (;;)
    {
    }
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = { // 破坏 sentinel 的测试任务初始化配置
        .entry = case9_corrupt_sentinel,
        .param = NULL,
        .stack_base = g_case9_stack,
        .stack_size = 64U,
        .name = "sentinel_panic",
        .priority = 5U,
        .time_slice = 0U,
    };

    regression_expect_ok(os_task_create(&g_case9_task, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 10U)
/* ==================== Case 10: invalid PSP panic ==================== */
static os_task_t g_case10_controller;
static os_task_t g_case10_victim;
static uint32_t g_case10_controller_stack[64];
static uint32_t g_case10_victim_stack[64];

static void case10_victim(void *param)
{
    (void)param;
    for (;;)
    {
    }
}

static void case10_controller(void *param)
{
    (void)param;
    /* 把 victim 的 PSP 改到合法栈区间外，等待真正发生 PendSV 时触发端口层检查。 */
    g_case10_victim.sp = g_case10_victim.stack_base - 1;
    regression_expect_ok(os_task_delay(1U));
    regression_fail();
}

static void regression_case_setup(void)
{
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    cfg.entry = case10_controller;
    cfg.stack_base = g_case10_controller_stack;
    cfg.stack_size = 64U;
    cfg.name = "sp_ctl";
    cfg.priority = 4U;
    regression_expect_ok(os_task_create(&g_case10_controller, &cfg));

    cfg.entry = case10_victim;
    cfg.stack_base = g_case10_victim_stack;
    cfg.name = "sp_victim";
    cfg.priority = 5U;
    regression_expect_ok(os_task_create(&g_case10_victim, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 11U)
/* ==================== Case 11: HardFault 统一 panic ==================== */
void HardFault_Handler(void);

static void regression_case_setup(void)
{
    /* 这里不真实制造 CPU fault，只验证 fault handler -> os_panic 这条统一链路。 */
    HardFault_Handler();
    regression_fail();
}
#elif (RTOS_REGRESSION_CASE == 12U)
/* ==================== Case 12: UsageFault 统一 panic ==================== */
void UsageFault_Handler(void);

static void regression_case_setup(void)
{
    UsageFault_Handler();
    regression_fail();
}
#elif (RTOS_REGRESSION_CASE == 13U)
/* ==================== Case 13: one-shot timer callback ==================== */
static os_timer_t g_case13_timer;
static volatile uint32_t g_case13_callback_count = 0U;
static os_task_t g_case13_checker;
static uint32_t g_case13_checker_stack[64];

static void case13_timer_callback(void *arg)
{
    (void)arg;
    g_case13_callback_count++;
}

static void case13_checker(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(30U));

    if (g_case13_callback_count != 1U)
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_timer_config_t timer_cfg = { // one-shot timer 的静态初始化配置
        .name = "timer_one_shot",
        .mode = OS_TIMER_ONE_SHOT,
        .callback = case13_timer_callback,
        .arg = NULL,
    };
    os_task_config_t cfg = { // checker 任务的静态初始化配置
        .entry = case13_checker,
        .param = NULL,
        .stack_base = g_case13_checker_stack,
        .stack_size = 64U,
        .name = "timer_one_chk",
        .priority = 4U,
        .time_slice = 0U,
    };

    regression_expect_ok(os_timer_init(&g_case13_timer, &timer_cfg));
    regression_expect_ok(os_timer_start(&g_case13_timer, 10U));
    regression_expect_ok(os_task_create(&g_case13_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 14U)
/* ==================== Case 14: periodic timer callback ==================== */
static os_timer_t g_case14_timer;
static volatile uint32_t g_case14_callback_count = 0U;
static os_task_t g_case14_checker;
static uint32_t g_case14_checker_stack[64];

static void case14_timer_callback(void *arg)
{
    (void)arg;
    g_case14_callback_count++;
}

static void case14_checker(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(30U));
    regression_expect_ok(os_timer_stop(&g_case14_timer));

    if (g_case14_callback_count < 3U)
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_timer_config_t timer_cfg = { // periodic timer 的静态初始化配置
        .name = "timer_periodic",
        .mode = OS_TIMER_PERIODIC,
        .callback = case14_timer_callback,
        .arg = NULL,
    };
    os_task_config_t cfg = { // checker 任务的静态初始化配置
        .entry = case14_checker,
        .param = NULL,
        .stack_base = g_case14_checker_stack,
        .stack_size = 64U,
        .name = "timer_periodic_chk",
        .priority = 4U,
        .time_slice = 0U,
    };

    regression_expect_ok(os_timer_init(&g_case14_timer, &timer_cfg));
    regression_expect_ok(os_timer_start(&g_case14_timer, 5U));
    regression_expect_ok(os_task_create(&g_case14_checker, &cfg));
}
#elif (RTOS_REGRESSION_CASE == 15U)
/* ==================== Case 15: timer stop / restart ==================== */
static os_timer_t g_case15_timer;
static volatile uint32_t g_case15_callback_count = 0U;
static os_task_t g_case15_stopper;
static os_task_t g_case15_checker;
static uint32_t g_case15_stopper_stack[64];
static uint32_t g_case15_checker_stack[64];

static void case15_timer_callback(void *arg)
{
    (void)arg;
    g_case15_callback_count++;
}

static void case15_stopper(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(10U));
    regression_expect_ok(os_timer_stop(&g_case15_timer));
    regression_expect_ok(os_timer_start(&g_case15_timer, 5U));
    for (;;)
    {
    }
}

static void case15_checker(void *param)
{
    (void)param;
    regression_expect_ok(os_task_delay(25U));

    if (g_case15_callback_count != 1U)
    {
        regression_fail();
    }

    regression_pass();
}

static void regression_case_setup(void)
{
    os_timer_config_t timer_cfg = { // restart 场景使用的 one-shot timer 初始化配置
        .name = "timer_restart",
        .mode = OS_TIMER_ONE_SHOT,
        .callback = case15_timer_callback,
        .arg = NULL,
    };
    os_task_config_t cfg = {0}; // 复用的任务创建配置对象

    regression_expect_ok(os_timer_init(&g_case15_timer, &timer_cfg));
    regression_expect_ok(os_timer_start(&g_case15_timer, 20U));

    cfg.entry = case15_stopper;
    cfg.stack_base = g_case15_stopper_stack;
    cfg.stack_size = 64U;
    cfg.name = "timer_stopper";
    cfg.priority = 4U;
    regression_expect_ok(os_task_create(&g_case15_stopper, &cfg));

    cfg.entry = case15_checker;
    cfg.stack_base = g_case15_checker_stack;
    cfg.name = "timer_restart_chk";
    cfg.priority = 3U;
    regression_expect_ok(os_task_create(&g_case15_checker, &cfg));
}
#else
static void regression_case_setup(void)
{
    regression_fail();
}
#endif

int main(void)
{
    regression_probe_init();
    /* 所有 panic 类 case 都统一通过 panic hook 先输出 reason 编号。 */
    (void)os_panic_hook_set(regression_panic_hook);
    regression_emit_case_start(RTOS_REGRESSION_CASE);
    regression_case_setup();

#if (RTOS_REGRESSION_CASE >= 1U) && (RTOS_REGRESSION_CASE <= 10U)
    /* 1~10 依赖真正启动内核并运行任务/中断路径。 */
    regression_kernel_start();
#elif (RTOS_REGRESSION_CASE >= 13U) && (RTOS_REGRESSION_CASE <= 15U)
    /* 13~15 是 timer 回归，也必须启动内核让 timer daemon 真正运行。 */
    regression_kernel_start();
#endif

    regression_fail();
    return 0;
}
