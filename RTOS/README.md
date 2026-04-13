# RTOS Public API Guide

本目录下的 RTOS 当前只面向 `STM32F103C8T6 / Cortex-M3` 单平台。

这份文档描述的是**当前阶段稳定承诺给应用代码使用的 public API**。  
这里稳定的是：

- public 头文件组织
- public 函数名
- 调用语义
- 线程态 / ISR 调用约束

这里**不稳定**、暂时**不承诺**的是：

- `os_task_t / os_sem_t / os_mutex_t / os_queue_t / os_timer_t` 的字段布局
- 这些对象内部是否继续使用 `list_t / list_node_t`
- 这些对象字段名、字段顺序、字段数量

换句话说：

- 你可以定义这些对象实例，并把它们传给 RTOS API
- 你不应该直接读写它们的内部字段
- 当前字段可见只是为了静态分配与过渡兼容，不表示 layout 已经成为稳定 ABI / API

## 1. 统一入口

应用代码通常只需要：

```c
#include "os.h"
```

`os.h` 是 umbrella header，会统一转出当前阶段全部 stable public headers：

- `os_kernel.h`
- `os_task.h`
- `os_sem.h`
- `os_mutex.h`
- `os_queue.h`
- `os_timer.h`
- `os_diag.h`

以下头文件**不是 public API**，应用代码不要直接 include：

- `os_port.h`
- `internal/*`
- `portable/os_port_cortex_m3.h`

## 2. 命名约定

当前 public API 统一采用 `os_*` 前缀。

任务层已经进入语义收口阶段，因此推荐使用：

- `os_task_*`
- `os_kernel_tick_get()`
- `os_panic_hook_set()`

旧名字目前仍保留一轮 compatibility，用于过渡兼容：

- `task_*`
- `os_tick_get()`
- `os_panic_set_hook()`

新代码和示例应优先使用新名字。

## 3. Public Header Layout

### `os_kernel.h`

负责：

- 启动内核
- 查询全局 tick

当前 public API：

- `os_kernel_start(uint32_t cpu_clock_hz)`
- `os_kernel_tick_get(void)`

兼容 API：

- `os_tick_get(void)`

### `os_task.h`

负责：

- 任务对象语义类型
- 任务创建 / 删除 / 延时 / 让出
- 任务查询与优先级服务层

当前 public semantic alias：

- `os_task_t`
- `os_task_config_t`
- `os_task_state_t`
- `os_task_entry_t`

当前 public API：

- `os_task_create`
- `os_task_delete`
- `os_task_yield`
- `os_task_delay`
- `os_task_delay_until`
- `os_task_current_get`
- `os_task_priority_get`
- `os_task_base_priority_get`
- `os_task_base_priority_set`
- `os_task_state_get`
- `os_task_name_get`
- `os_task_stack_high_water_mark_get`

兼容 API：

- `task_create`
- `task_delete`
- `task_yield`
- `task_delay`
- `task_delay_until`
- `task_get_current`
- `task_priority_get`
- `task_base_priority_get`
- `task_base_priority_set`
- `task_state_get`
- `task_name_get`
- `task_stack_high_water_mark_get`

### `os_sem.h`

负责二值信号量：

- `os_sem_init`
- `os_sem_take`
- `os_sem_give`
- `os_sem_give_from_isr`

### `os_mutex.h`

负责 non-recursive mutex：

- `os_mutex_init`
- `os_mutex_lock`
- `os_mutex_unlock`

### `os_queue.h`

负责固定大小、静态缓冲区消息队列：

- `os_queue_init`
- `os_queue_send`
- `os_queue_recv`
- `os_queue_send_from_isr`
- `os_queue_recv_from_isr`

### `os_timer.h`

负责最小软件定时器：

- `os_timer_t`
- `os_timer_config_t`
- `os_timer_mode_t`
- `os_timer_callback_t`
- `os_timer_init`
- `os_timer_start`
- `os_timer_stop`

### `os_diag.h`

负责 panic / assert / 诊断：

- `os_panic_reason_t`
- `os_panic_info_t`
- `os_panic_hook_t`
- `os_panic_hook_set`
- `os_panic`
- `OS_ASSERT`

兼容 API：

- `os_panic_set_hook`

## 4. 调用约束

### 线程态 API

以下 API 只允许在线程态调用：

- `os_task_create`
- `os_task_delete`
- `os_task_yield`
- `os_task_delay`
- `os_task_delay_until`
- `os_task_base_priority_set`
- `os_sem_take`
- `os_sem_give`
- `os_mutex_lock`
- `os_mutex_unlock`
- `os_queue_send`
- `os_queue_recv`
- `os_timer_start`
- `os_timer_stop`

### ISR API

当前只有以下对象层接口允许在 ISR 使用：

- `os_sem_give_from_isr`
- `os_queue_send_from_isr`
- `os_queue_recv_from_isr`

本阶段**没有** `task` 层 ISR API，也**没有** `timer` 层 ISR API。

## 5. 对象模型

当前 RTOS 采用**静态对象模型**：

- 任务对象由调用方提供 `os_task_t`
- 任务栈由调用方提供静态数组
- 队列缓冲区由调用方提供静态缓冲区
- 信号量 / 互斥锁 / 软件定时器对象都由调用方提供静态实例

RTOS 当前不要求动态分配，也不依赖 heap。

## 6. 最小使用示例

### 6.1 创建任务并启动内核

```c
#include "os.h"

static os_task_t blink_task;
static uint32_t  blink_stack[128];

static void blink_entry(void *param)
{
    (void)param;

    for (;;)
    {
        /* do something */
        os_task_delay(100U);
    }
}

int main(void)
{
    os_task_config_t cfg = {
        .entry = blink_entry,
        .param = NULL,
        .stack_base = blink_stack,
        .stack_size = 128U,
        .name = "blink",
        .priority = 5U,
        .time_slice = 0U,
    };

    if (os_task_create(&blink_task, &cfg) != OS_STATUS_OK)
    {
        while (1)
        {
        }
    }

    (void)os_kernel_start(72000000UL);

    while (1)
    {
    }
}
```

### 6.2 周期任务

```c
static void periodic_entry(void *param)
{
    os_tick_t previous = 0U;

    (void)param;
    previous = os_kernel_tick_get();

    for (;;)
    {
        /* do periodic work */
        (void)os_task_delay_until(&previous, 10U);
    }
}
```

### 6.3 二值信号量

```c
static os_sem_t done_sem;

void init_objects(void)
{
    (void)os_sem_init(&done_sem, 0U);
}

void worker_task(void)
{
    (void)os_sem_take(&done_sem, OS_WAIT_FOREVER);
}

void some_isr(void)
{
    (void)os_sem_give_from_isr(&done_sem);
}
```

### 6.4 互斥锁

```c
static os_mutex_t uart_mutex;

void init_mutex(void)
{
    (void)os_mutex_init(&uart_mutex);
}

void print_task(void)
{
    (void)os_mutex_lock(&uart_mutex, OS_WAIT_FOREVER);
    /* critical section */
    (void)os_mutex_unlock(&uart_mutex);
}
```

### 6.5 静态消息队列

```c
static os_queue_t queue;
static uint8_t    queue_buffer[8 * sizeof(uint32_t)];

void init_queue(void)
{
    (void)os_queue_init(&queue, queue_buffer, sizeof(uint32_t), 8U);
}

void sender(void)
{
    uint32_t value = 123U;
    (void)os_queue_send(&queue, &value, OS_WAIT_FOREVER);
}

void receiver(void)
{
    uint32_t value = 0U;
    (void)os_queue_recv(&queue, &value, OS_WAIT_FOREVER);
}
```

### 6.6 软件定时器

```c
static os_timer_t timer;

static void timer_cb(void *arg)
{
    (void)arg;
    /* callback runs in timer daemon task context */
}

void init_timer(void)
{
    os_timer_config_t cfg = {
        .name = "demo_timer",
        .mode = OS_TIMER_PERIODIC,
        .callback = timer_cb,
        .arg = NULL,
    };

    (void)os_timer_init(&timer, &cfg);
    (void)os_timer_start(&timer, 10U);
}
```

## 7. 任务对象与优先级语义

### effective priority

`os_task_priority_get()` 返回的是当前 **effective priority**：

- 正常情况下等于 `base priority`
- 当 mutex 优先级继承发生时，可能临时高于 `base priority`

### base priority

`os_task_base_priority_get()` / `os_task_base_priority_set()` 作用的是任务配置上的 **base priority**。

调用 `os_task_base_priority_set()` 后：

- RTOS 会按当前继承关系重算 effective priority
- 若调度结果发生变化，接口内部会自行触发抢占
- 应用代码不需要再处理 `OS_STATUS_SWITCH_REQUIRED`

## 8. 诊断与 panic

可以通过：

```c
(void)os_panic_hook_set(my_hook);
```

注册一个全局 panic hook。

panic hook 收到的是：

- `reason`
- `file`
- `line`
- `current_task`

当前 panic reason 包括：

- `OS_PANIC_ASSERT`
- `OS_PANIC_TASK_STATE`
- `OS_PANIC_STACK_POINTER_RANGE`
- `OS_PANIC_STACK_SENTINEL`
- `OS_PANIC_PORT_FAILURE`
- `OS_PANIC_HARDFAULT`
- `OS_PANIC_USAGEFAULT`

断言使用：

```c
OS_ASSERT(expr);
```

## 9. Compatibility 说明

当前仍保留以下 compatibility API：

- `os_tick_get()`
- `task_*`
- `os_panic_set_hook()`

这些旧名字的作用只有两个：

- 让现有代码过渡期间继续可编译
- 给下一阶段彻底切换新名字留窗口

新代码、示例、文档都应优先使用：

- `os_kernel_tick_get()`
- `os_task_*`
- `os_panic_hook_set()`

## 10. 当前不属于 public contract 的内容

以下内容当前都不属于稳定 public contract：

- `os_port.h`
- `internal/*`
- `portable/*`
- 任意对象 struct 的字段布局
- 任意调度内部 helper
- 任意 `_locked` / wait-list / ready-queue / inheritance 内部接口

如果你的应用代码开始直接访问这些内容，未来版本升级时很容易 break。
