# STM32F103C8T6 SystemInit Clock Tasks

这份文档专门回答 4 个问题：

1. `SystemInit()` 在你这个工程里应该放在哪个文件。
2. `SystemInit()` 中如何把系统时钟从默认复位状态切到 `外部 8MHz 晶振 -> PLL x9 -> 72MHz`。
3. 你当前 `driver/dri_ll_rcc.c` 和 `driver/dri_ll_rcc.h` 已经有哪些函数可以直接复用。
4. 为了把这件事做完整，还需要补哪些封装函数。

## 当前工程现状

- `startup_stm32f103c8t6.c` 里已经有你自己的 `Reset_Handler()`，当前流程是：
  - 拷贝 `.data`
  - 清零 `.bss`
  - 直接跳转 `main()`
- 当前 `Reset_Handler()` 还没有调用 `SystemInit()`。
- `project/CMakeLists.txt` 里当前只编译了 `startup_stm32f103c8t6.c`、`driver`、`RTOS`、`user/main.c`，并没有把任何 `system_*.c` 加入构建。
- 仓库中虽然带了 CMSIS 的 `system_stm32f1xx.h` 和模板 `system_stm32f1xx.c`，但它们目前只是参考文件，还没有真正接入你的构建流程。

## `SystemInit()` 应该放在哪个文件

结论先说：

- 不建议把 `SystemInit()` 放进 `driver/dri_ll_rcc.c`
- 也不建议长期把完整时钟初始化逻辑直接塞进 `startup_stm32f103c8t6.c`
- 最合适的做法是新建一个独立的系统初始化文件，例如：
  - `system_stm32f103c8t6.c`
  - 可选再配一个 `system_stm32f103c8t6.h`

原因如下：

- `driver/dri_ll_rcc.c` 的职责应该是“RCC 寄存器原子操作和低层封装”，不应该混入“本板级到底跑多少 MHz”这种系统策略。
- `startup_stm32f103c8t6.c` 的职责应该尽量保持最小化：建立启动链路、准备运行时环境、跳到系统初始化和主程序。
- `SystemInit()` 除了 RCC，还往往会涉及 `Flash ACR`、`SystemCoreClock`、可选的 `VTOR` 重定位，这些都已经超出了单纯 RCC 驱动的边界。

如果你想和 CMSIS 命名保持一致，推荐做法是：

- 自己在工程根目录放一个 `system_stm32f103c8t6.c`
- 在这个文件里实现标准签名的 `void SystemInit(void);`
- 不要直接修改 CMSIS 模板文件本体，避免以后升级 CMSIS 时混乱

## 在你这个工程里，`SystemInit()` 应该在什么时候被调用

你当前的 startup 是 C 版本而不是汇编模板，因此推荐调用顺序是：

1. 在 `Reset_Handler()` 中先拷贝 `.data`
2. 再清零 `.bss`
3. 然后调用 `SystemInit()`
4. 最后进入 `main()`

也就是建议把启动链路改成这样：

```c
void Reset_Handler(void)
{
    /* 1. copy .data */
    /* 2. clear .bss */
    SystemInit();
    main();
    while (1)
    {
    }
}
```

这样安排对你当前工程更安全，原因是：

- 你的 `Reset_Handler()` 是自己写的 C 函数
- 如果 `SystemInit()` 以后使用了全局变量、静态变量或者 `SystemCoreClock`，那么在 `.data/.bss` 已经准备好之后再调用更稳妥
- CMSIS 官方模板里常常更早调用 `SystemInit()`，那是因为它的实现通常严格限制为非常早期、非常纯粹的硬件寄存器初始化

## 72MHz 目标时钟树

如果你的板子是外部 `8MHz` 晶振，并希望系统主频跑到 `72MHz`，典型目标配置应当是：

- `SYSCLK = 72MHz`
- `HCLK = 72MHz`
- `PCLK1 = 36MHz`
- `PCLK2 = 72MHz`
- `PLL source = HSE`
- `PLLXTPRE = HSE / 1`
- `PLLMUL = x9`
- `Flash latency = 2 wait states`
- `Flash prefetch = enable`

换算关系就是：

```text
HSE = 8MHz
PLL input = 8MHz
PLL output = 8MHz x 9 = 72MHz
SYSCLK = PLL = 72MHz
AHB  = SYSCLK / 1 = 72MHz
APB1 = HCLK / 2   = 36MHz
APB2 = HCLK / 1   = 72MHz
```

这里最关键的约束是：

- `APB1` 不能跑到 `72MHz`，所以必须分频到 `36MHz`
- Flash 在 `72MHz` 下必须提前配置等待周期，不能等切上去以后再补

## `SystemInit()` 的推荐实现步骤

建议按下面顺序做，这个顺序比“想到哪写到哪”更安全。

### 1. 先准备 `SystemInit()` 所在文件并接入构建

- 新建 `system_stm32f103c8t6.c`
- 在 `project/CMakeLists.txt` 中把这个文件加入源文件列表
- 在 `startup_stm32f103c8t6.c` 中声明并调用 `SystemInit()`

### 2. 在 `SystemInit()` 中先处理 Flash

在切换到 `72MHz` 前，先设置：

- `FLASH_ACR.PRFTBE = 1`
- `FLASH_ACR.LATENCY = 2`

这是必须项，不属于 RCC，但属于“系统时钟初始化”的一部分。

### 3. 确保外部高速晶振工作在晶振模式

如果你用的是外部晶振而不是外部时钟输入，应当确保：

- `HSE bypass = disable`

也就是不要把 `HSEBYP` 置 1。

### 4. 打开 HSE 并等待稳定

流程是：

- 使能 HSE
- 轮询 `HSERDY`
- 等到 HSE ready 再继续

如果你希望更鲁棒，可以在这里加入超时和失败策略：

- 方案 A：超时后回退到 `HSI 8MHz`
- 方案 B：超时后进入死循环，方便早期调试

### 5. 配置总线分频

在切系统时钟前，把总线分频先写好：

- `AHB prescaler = DIV1`
- `APB1 prescaler = DIV2`
- `APB2 prescaler = DIV1`

这样在切到 `72MHz` 的瞬间，总线时钟关系就已经正确。

### 6. 在 PLL 关闭状态下配置 PLL

先保证：

- `PLL = off`

然后配置：

- `PLLSRC = HSE`
- `PLLXTPRE = HSE / 1`
- `PLLMUL = x9`

注意：

- `PLLSRC` 和 `PLLMUL` 都要求在 PLL 关闭时配置
- `PLLXTPRE` 也应当在 PLL 关闭时配置

### 7. 打开 PLL 并等待锁定

流程是：

- 使能 PLL
- 轮询 `PLLRDY`
- 等 PLL ready 后继续

### 8. 切换系统时钟到 PLL

流程是：

- 把 `SW` 写成 `PLL`
- 轮询 `SWS`
- 直到 `SWS == PLL`

这一段非常关键，因为“写了 SW”不等于“系统真的已经切过去了”。

### 9. 更新系统频率变量

如果你打算遵循 CMSIS 习惯，建议在 `SystemInit()` 末尾更新：

- `SystemCoreClock = 72000000U`

后续如果你要配 `SysTick`、软件延时、串口波特率、RTOS tick，这个变量会很有价值。

### 10. 可选地关闭 HSI

当 `SYSCLK` 已经稳定切到 `PLL` 后，可以选择：

- 保留 HSI 打开，增加保守性
- 或关闭 HSI，减少一点功耗

这一步不是必须项，第一版可以先不关。

## 你现在已经可以直接复用的 RCC 封装

下面这些函数，已经足够覆盖“时钟切换流程”的大部分步骤。

### HSE 相关

- `dri_ll_rcc_hse_enable()`
- `dri_ll_rcc_hse_disable()`
- `dri_ll_rcc_hse_is_ready()`
- `dri_ll_rcc_hse_bypass_enable()`
- `dri_ll_rcc_hse_bypass_disable()`

其中为了外部晶振模式，你会直接用到：

- `dri_ll_rcc_hse_bypass_disable()`
- `dri_ll_rcc_hse_enable()`
- `dri_ll_rcc_hse_is_ready()`

### PLL 相关

- `dri_ll_rcc_pll_enable()`
- `dri_ll_rcc_pll_disable()`
- `dri_ll_rcc_pll_is_ready()`
- `dri_ll_rcc_pll_source_set()`
- `dri_ll_rcc_pll_mul_set()`
- `dri_ll_rcc_pll_source_get()`
- `dri_ll_rcc_pll_mul_get()`

其中为了 `8MHz -> 72MHz`，你会直接用到：

- `dri_ll_rcc_pll_disable()`
- `dri_ll_rcc_pll_source_set(RCC_PLL_SOURCE_HSE)`
- `dri_ll_rcc_pll_mul_set(RCC_PLL_MUL_9)`
- `dri_ll_rcc_pll_enable()`

### 系统时钟和分频相关

- `dri_ll_rcc_sysclk_select()`
- `dri_ll_rcc_sysclk_source_get()`
- `dri_ll_rcc_sysclk_status_get()`
- `dri_ll_rcc_ahb_prescaler_set()`
- `dri_ll_rcc_apb1_prescaler_set()`
- `dri_ll_rcc_apb2_prescaler_set()`

其中为了 `72MHz` 目标配置，你会直接用到：

- `dri_ll_rcc_ahb_prescaler_set(RCC_AHB_PRESCALER_DIV1)`
- `dri_ll_rcc_apb1_prescaler_set(RCC_APB_PRESCALER_DIV2)`
- `dri_ll_rcc_apb2_prescaler_set(RCC_APB_PRESCALER_DIV1)`
- `dri_ll_rcc_sysclk_select(RCC_SYSCLK_SOURCE_PLL)`
- `dri_ll_rcc_sysclk_status_get()`

### `dri_ll.h` 中可直接辅助使用的基础函数

除了 `dri_ll_rcc.c` 里的接口，你底层还有这些通用寄存器辅助函数可用：

- `dri_ll_wait_bits_set()`
- `dri_ll_wait_bits_clear()`
- `dri_ll_read_field()`
- `dri_ll_modify_reg()`

它们可以帮你减少手写轮询代码。

例如：

- 等 `HSERDY`
- 等 `PLLRDY`
- 读取字段状态

## 为了完成这个功能，RCC 里还需要补哪些封装

结论是：

- 你现在的 RCC 封装已经覆盖了大约 70% 到 80% 的切时钟工作
- 真正“还差的关键点”主要有两个 RCC 项和一个非 RCC 项

### 必须补的 RCC 封装 1: `PLLXTPRE` 配置

为了明确表达“PLL 输入来自 HSE 且不分频”，你还需要封装：

- `RCC_CFGR.PLLXTPRE`

建议新增：

```c
typedef enum
{
    RCC_PLL_HSE_DIV_1 = 0x00UL,
    RCC_PLL_HSE_DIV_2 = 0x01UL,
} dri_ll_rcc_pll_hse_div;

void dri_ll_rcc_pll_hse_div_set(dri_ll_rcc_pll_hse_div div);
dri_ll_rcc_pll_hse_div dri_ll_rcc_pll_hse_div_get(void);
```

原因是：

- 目标配置要求的是 `8MHz / 1` 后再送入 PLL
- 如果不显式封装这一步，你就只能依赖“复位默认值刚好是 `/1`”
- 从驱动设计角度，这会让时钟树配置不完整，也不够自说明

### 必须补的 RCC 封装 2: 等待系统时钟切换完成

你现在已经有：

- `dri_ll_rcc_sysclk_select()`
- `dri_ll_rcc_sysclk_status_get()`

但你还没有一个“切换并等待完成”的封装。

建议新增其中一种：

```c
void dri_ll_rcc_sysclk_wait(dri_ll_rcc_sysclk_status target);
```

或者：

```c
void dri_ll_rcc_sysclk_select_and_wait(dri_ll_rcc_sysclk_source source,
                                       dri_ll_rcc_sysclk_status target);
```

原因是：

- `SW` 写入只是请求切换
- 真正是否切换完成，要看 `SWS`
- 这一步是启动时钟切换的关键路径，值得封成明确接口

### 推荐补的 RCC 封装 3: 带等待或超时的 HSE 启动接口

虽然你现在已经能用：

- `dri_ll_rcc_hse_enable()`
- `dri_ll_rcc_hse_is_ready()`

手工写出等待循环，但如果想让 `SystemInit()` 更清爽，建议补一个：

```c
bool dri_ll_rcc_hse_enable_and_wait(void);
```

或者：

```c
bool dri_ll_rcc_hse_wait_ready_timeout(u32 timeout);
```

这样可以把“等待晶振稳定”和“失败策略”集中管理，而不是每个调用者都自己写死循环。

### 推荐补的 RCC 封装 4: ADC 预分频

这一项不是“切到 72MHz”本身的硬要求，但只要你以后要上 ADC，就最好现在一并补上：

- `RCC_CFGR.ADCPRE`

建议新增：

```c
void dri_ll_rcc_adc_prescaler_set(...);
```

原因是：

- 当 `PCLK2 = 72MHz` 时，ADC 时钟不能直接跟着跑满
- 后续如果启用 ADC，通常还要再配置 `/6` 或 `/8`

## RCC 之外还必须补的封装

真正缺失的最关键非 RCC 项是 `FLASH_ACR`。

### 必须补的非 RCC 封装: Flash 等待周期和预取

为了让 `72MHz` 正常工作，建议新建一个 `driver/dri_ll_flash.c` 和 `driver/dri_ll_flash.h`，至少封装：

```c
void dri_ll_flash_prefetch_enable(void);
void dri_ll_flash_prefetch_disable(void);
void dri_ll_flash_latency_set(...);
```

最少要能表达：

- `prefetch enable`
- `latency = 2 wait states`

原因是：

- 这一步不是 RCC 配置
- 但它又是 `SystemInit()` 在升频到 `72MHz` 时不可缺少的前置条件
- 如果把它硬编码在 `SystemInit()` 里直接裸写寄存器，也能跑，但后面会让你的系统层缺少统一的 Flash low-level 抽象

## 这一版 `SystemInit()` 最小闭环需要调用的函数

如果先不做超时和回退策略，最小可运行闭环大致会用到这些接口：

```c
void SystemInit(void)
{
    /* 1. Flash */
    dri_ll_flash_prefetch_enable();
    dri_ll_flash_latency_set(FLASH_LATENCY_2);

    /* 2. HSE */
    dri_ll_rcc_hse_bypass_disable();
    dri_ll_rcc_hse_enable();
    while (!dri_ll_rcc_hse_is_ready())
    {
    }

    /* 3. Bus prescaler */
    dri_ll_rcc_ahb_prescaler_set(RCC_AHB_PRESCALER_DIV1);
    dri_ll_rcc_apb1_prescaler_set(RCC_APB_PRESCALER_DIV2);
    dri_ll_rcc_apb2_prescaler_set(RCC_APB_PRESCALER_DIV1);

    /* 4. PLL config */
    dri_ll_rcc_pll_disable();
    dri_ll_rcc_pll_source_set(RCC_PLL_SOURCE_HSE);
    dri_ll_rcc_pll_hse_div_set(RCC_PLL_HSE_DIV_1);
    dri_ll_rcc_pll_mul_set(RCC_PLL_MUL_9);
    dri_ll_rcc_pll_enable();

    /* 5. switch SYSCLK */
    dri_ll_rcc_sysclk_select(RCC_SYSCLK_SOURCE_PLL);
    while (dri_ll_rcc_sysclk_status_get() != RCC_SYSCLK_STATUS_PLL)
    {
    }

    SystemCoreClock = 72000000U;
}
```

上面这段伪代码的重点不是让你直接复制，而是帮助你确认：

- 现在已有的 RCC 接口已经够你完成主体流程
- 真正阻塞你完整实现的关键缺口是：
  - `Flash ACR`
  - `PLLXTPRE`
  - `SYSCLK switch wait`

## 推荐任务拆分

建议按下面顺序推进，而不是一次把所有代码都堆进 `SystemInit()`。

- [ ] 1. 新建 `system_stm32f103c8t6.c`
- [ ] 2. 在 `project/CMakeLists.txt` 中把该文件加入构建
- [ ] 3. 在 `startup_stm32f103c8t6.c` 的 `Reset_Handler()` 中增加 `SystemInit()`
- [ ] 4. 新建 `dri_ll_flash.c/.h`，补 `FLASH_ACR` 的最小封装
- [ ] 5. 在 `dri_ll_rcc.h/.c` 中补 `PLLXTPRE` 的设置与读取接口
- [ ] 6. 在 `dri_ll_rcc.h/.c` 中补 `SYSCLK` 切换完成等待接口
- [ ] 7. 在 `system_stm32f103c8t6.c` 中实现 `SystemInit()`
- [ ] 8. 在 `SystemInit()` 末尾维护 `SystemCoreClock = 72000000U`
- [ ] 9. 视需要补 `SystemCoreClockUpdate()`
- [ ] 10. 视需要补 HSE 启动超时与回退策略

## 一句话结论

一句话总结就是：

- `SystemInit()` 应该放在独立的系统初始化文件里，而不是 `dri_ll_rcc.c`
- 你现有的 RCC 封装已经足够搭起主干流程
- 但要把 `8MHz HSE -> 72MHz PLL` 做成一套完整、可维护的方案，还必须补上 `PLLXTPRE`、`SYSCLK 切换等待` 和 `Flash ACR` 封装
