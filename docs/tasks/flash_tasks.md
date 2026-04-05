# STM32F103C8T6 Flash Tasks

这份文档专门说明一件事：

为了让 `SystemInit()` 能安全地把系统时钟切到 `72MHz`，`Flash` 这一侧最少需要补哪些函数，以及这些函数应该如何一步步实现。

这份文档只关注 `FLASH_ACR` 相关配置，不展开 `Flash 编程/擦除` 功能。原因很简单：

- `SystemInit()` 升频到 `72MHz` 真正需要的是 `Flash access time` 配置
- 也就是 `LATENCY`、`PRFTBE`、可选的 `HLFCYA`
- 而 `KEYR/SR/CR/AR` 那一套是 `Flash Program/Erase Controller`，属于下一阶段

## 文档集合

- `RM0008 Rev 21` - [docs/lib/rm0008-stm32f103xx参考手册.pdf](/home/ssw12/Projects/stm32f103c8t6_null/docs/lib/rm0008-stm32f103xx参考手册.pdf)
  - 重点看 `Memory and bus architecture` 里关于 `FLASH_ACR` 的描述
  - 关键内容：
    - `The FLASH_ACR register is used to enable/disable prefetch and half cycle access, and to control the Flash memory access time according to the CPU frequency.`
    - `LATENCY`:
      - `0 wait state`: `0 < SYSCLK <= 24MHz`
      - `1 wait state`: `24MHz < SYSCLK <= 48MHz`
      - `2 wait states`: `48MHz < SYSCLK <= 72MHz`
- `DS5319 Rev 20` - [docs/lib/ds5319-stm32f103c8-datasheet.pdf](/home/ssw12/Projects/stm32f103c8t6_null/docs/lib/ds5319-stm32f103c8-datasheet.pdf)
  - 用来确认 `STM32F103C8T6` 的最高主频是 `72MHz`
- `PM0075`
  - 如果以后要做 `Flash 编程/擦除`，需要补这份 Flash programming manual
  - 当前仓库里没有这份文档，所以第一版先不展开 `KEYR/SR/CR` 的驱动

## 为什么 `SystemInit()` 必须先配 Flash

你的目标是：

- `HSE = 8MHz`
- `PLL = x9`
- `SYSCLK = 72MHz`

而 `RM0008` 对 `FLASH_ACR.LATENCY` 的要求是：

- `0WS` 只适用于 `24MHz` 以内
- `1WS` 只适用于 `48MHz` 以内
- `2WS` 才适用于 `72MHz`

所以结论是：

- 在把系统时钟切到 `72MHz` 之前，必须先把 `Flash latency` 配成 `2 wait states`
- 否则 CPU 已经在高频访问 Flash，但 Flash 访问时间还没跟上，系统可能直接跑飞

## 第一版真正需要的 Flash 函数

如果当前目标只是支撑 `SystemInit()`，建议第一版只实现这些函数：

```c
void dri_ll_flash_prefetch_enable(void);
void dri_ll_flash_prefetch_disable(void);
isENABLE dri_ll_flash_prefetch_is_enabled(void);

void dri_ll_flash_halfcycle_enable(void);
void dri_ll_flash_halfcycle_disable(void);
isENABLE dri_ll_flash_halfcycle_is_enabled(void);

void dri_ll_flash_latency_set(dri_ll_flash_latency latency);
dri_ll_flash_latency dri_ll_flash_latency_get(void);
```

其中：

- `prefetch` 是建议实现的，因为 `72MHz` 下通常应开启
- `halfcycle` 不是 `72MHz` 的必需项，但建议一并封装，这样 `FLASH_ACR` 这一小块接口就完整了
- `latency_set/get` 是必须项

## 为什么第一版先只做 `FLASH_ACR`

你现在的通用寄存器操作在 [dri_ll.h](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll.h) 里定义得很清楚：

- `dri_ll_set_bits()`
- `dri_ll_clear_bits()`
- `dri_ll_modify_reg()`
- `dri_ll_read_field()`

这些函数非常适合操作 `FLASH_ACR`，因为：

- `FLASH_ACR` 是普通的 `R/W` 寄存器位
- 可以安全使用 `set_bits / clear_bits / modify_reg`

但它们并不适合直接无脑扩展到整套 Flash 编程寄存器，因为：

- `FLASH_SR` 有错误标志和结束标志，很多位带有特殊写语义
- `FLASH_CR` 有 `STRT` 这种一次性触发位
- `FLASH_KEYR` 需要按顺序写入 magic key

所以第一版建议明确分层：

- 第 1 阶段：只做 `FLASH_ACR`
- 第 2 阶段：再做 `Flash program/erase`

## 推荐文件放置位置

按你当前工程结构，建议新建：

- [dri_ll_flash.h](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll_flash.h)
- [dri_ll_flash.c](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll_flash.c)

然后在 [system_stm32f103c8t6.c](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/system_stm32f103c8t6.c) 的 `SystemInit()` 里调用。

## 第一步：在 `dri_ll_flash.h` 中定义地址和偏移

第一版只需要 `FLASH_ACR`，因此最小定义可以先这样写：

```c
#define DRI_LL_FLASH_BASE_ADDR 0x40022000UL

#define DRI_LL_FLASH_ACR_OFFSET 0x00UL
```

如果你想顺手把后续编程/擦除寄存器也占好位置，也可以一并写上：

```c
#define DRI_LL_FLASH_KEYR_OFFSET 0x04UL
#define DRI_LL_FLASH_OPTKEYR_OFFSET 0x08UL
#define DRI_LL_FLASH_SR_OFFSET 0x0CUL
#define DRI_LL_FLASH_CR_OFFSET 0x10UL
#define DRI_LL_FLASH_AR_OFFSET 0x14UL
#define DRI_LL_FLASH_OBR_OFFSET 0x1CUL
#define DRI_LL_FLASH_WRPR_OFFSET 0x20UL
```

但第一版实现时只真正使用 `ACR`。

## 第二步：定义 `FLASH_ACR` 的位和字段

`FLASH_ACR` 第一版需要这几个位：

```c
typedef enum
{
    FLASH_ACR_LATENCY0 = (1UL << 0),
    FLASH_ACR_LATENCY1 = (1UL << 1),
    FLASH_ACR_LATENCY2 = (1UL << 2),
    FLASH_ACR_LATENCY  = (FLASH_ACR_LATENCY0 | FLASH_ACR_LATENCY1 | FLASH_ACR_LATENCY2),

    FLASH_ACR_HLFCYA   = (1UL << 3),
    FLASH_ACR_PRFTBE   = (1UL << 4),
    FLASH_ACR_PRFTBS   = (1UL << 5),
} dri_ll_flash_acr_bits;
```

同时定义字段位置与枚举值：

```c
typedef enum
{
    FLASH_ACR_LATENCY_POS = 0,
} dri_ll_flash_field_pos;

typedef enum
{
    DRI_LL_FLASH_LATENCY_0 = 0x00UL,
    DRI_LL_FLASH_LATENCY_1 = 0x01UL,
    DRI_LL_FLASH_LATENCY_2 = 0x02UL,
} dri_ll_flash_latency;
```

这里建议把 `latency` 的枚举值定义成“字段原始值”，这样 `set/get` 都会很顺。

## 第三步：声明对外接口

建议在 [dri_ll_flash.h](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll_flash.h) 中声明：

```c
void dri_ll_flash_prefetch_enable(void);
void dri_ll_flash_prefetch_disable(void);
isENABLE dri_ll_flash_prefetch_is_enabled(void);

void dri_ll_flash_halfcycle_enable(void);
void dri_ll_flash_halfcycle_disable(void);
isENABLE dri_ll_flash_halfcycle_is_enabled(void);

void dri_ll_flash_latency_set(dri_ll_flash_latency latency);
dri_ll_flash_latency dri_ll_flash_latency_get(void);
```

如果你想让接口再严谨一点，也可以把 `prefetch_is_enabled()` 改名为：

```c
isENABLE dri_ll_flash_prefetch_status_get(void);
```

因为 `PRFTBS` 读出来的是“状态”，不是“命令位”。  
不过从日常使用体验看，`is_enabled` 也完全能理解。

## 第四步：实现 `prefetch` 相关函数

`prefetch enable/disable` 是最直接的两组函数：

```c
void dri_ll_flash_prefetch_enable(void)
{
    dri_ll_set_bits(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_PRFTBE);
}

void dri_ll_flash_prefetch_disable(void)
{
    dri_ll_clear_bits(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_PRFTBE);
}
```

状态读取建议读取 `PRFTBS`：

```c
isENABLE dri_ll_flash_prefetch_is_enabled(void)
{
    return (dri_ll_read_reg(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET) & FLASH_ACR_PRFTBS) != 0U;
}
```

为什么这里读 `PRFTBS` 而不是 `PRFTBE`：

- `PRFTBE` 是软件写入的“使能请求”
- `PRFTBS` 是硬件反馈的“实际状态”
- 作为 `is_enabled()`，读状态位更合理

## 第五步：实现 `half cycle` 相关函数

虽然 `72MHz` 初始化流程里通常不会去打开 `HLFCYA`，但既然它和 `ACR` 属于同一块配置，建议一并实现：

```c
void dri_ll_flash_halfcycle_enable(void)
{
    dri_ll_set_bits(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_HLFCYA);
}

void dri_ll_flash_halfcycle_disable(void)
{
    dri_ll_clear_bits(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_HLFCYA);
}

isENABLE dri_ll_flash_halfcycle_is_enabled(void)
{
    return (dri_ll_read_reg(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET) & FLASH_ACR_HLFCYA) != 0U;
}
```

对于你当前 `72MHz` 目标，建议在 `SystemInit()` 里明确保持：

- `HLFCYA = 0`

也就是：

```c
dri_ll_flash_halfcycle_disable();
```

## 第六步：实现 `latency set/get`

这是最关键的一组函数。

设置函数推荐用 `dri_ll_modify_reg()`：

```c
void dri_ll_flash_latency_set(dri_ll_flash_latency latency)
{
    dri_ll_modify_reg(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_LATENCY,
                      ((u32)latency << FLASH_ACR_LATENCY_POS) & FLASH_ACR_LATENCY);
}
```

读取函数推荐用 `dri_ll_read_field()`：

```c
dri_ll_flash_latency dri_ll_flash_latency_get(void)
{
    return (dri_ll_flash_latency)dri_ll_read_field(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET,
                                                   FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_POS);
}
```

这两段的思路和你的 RCC 写法保持一致，后续阅读体验会很统一。

## 第七步：在 `SystemInit()` 中如何调用

如果你的目标是 `72MHz`，推荐在 [system_stm32f103c8t6.c](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/system_stm32f103c8t6.c) 中，把 Flash 配置放在时钟升频之前。

推荐顺序：

1. 关闭 `half cycle`
2. 设置 `latency = 2`
3. 打开 `prefetch`
4. 再去做 `HSE/PLL/SYSCLK` 切换

对应伪代码：

```c
void SystemInit(void)
{
    dri_ll_flash_halfcycle_disable();
    dri_ll_flash_latency_set(DRI_LL_FLASH_LATENCY_2);
    dri_ll_flash_prefetch_enable();

    /* 然后再做 RCC 切时钟 */
}
```

这里有两个实现细节值得注意：

- 虽然 `RM0008` 给出的 `FLASH_ACR` 复位值是 `0x00000030`，意味着 prefetch 默认就是开着的，但第一版仍建议在 `SystemInit()` 里显式再写一次，避免依赖“默认值恰好如此”
- `latency` 一定要在 `SYSCLK` 升到 `72MHz` 前配置好，这是关键中的关键

## 第八步：把新文件真正接入构建

补完 [dri_ll_flash.c](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll_flash.c) 和 [dri_ll_flash.h](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll_flash.h) 后，还要同步处理构建系统。

至少要确认这两件事：

- `dri_ll_flash.c` 已经加入 `DRIVER_SOURCES`
- 头文件搜索路径能覆盖 `Libraries/driver`

你当前的 [CMakeLists.txt](/home/ssw12/Projects/stm32f103c8t6_null/project/CMakeLists.txt) 里路径变量还保留着旧结构痕迹：

- `LIB_DIR` 指向 `Libraries`
- 但 `DRI_DIR` 仍然指向 `${ROOT_DIR}/driver`
- `STARTUP_SOURCES` 和 `LINKER_SCRIPT` 也还指向仓库根目录的旧位置

所以在接入 `dri_ll_flash.c` 时，建议顺手把这些路径一并统一到你当前实际结构：

- `Libraries/startup_stm32f103c8t6.c`
- `Libraries/stm32f103c8t6.ld`
- `Libraries/driver/*.c`

## 第九步：最小验证方法

第一版不需要上烧录器脚本也能先做静态验证和逻辑验证。

### 1. 代码层验证

检查这些点：

- `prefetch_enable/disable` 只操作 `PRFTBE`
- `halfcycle_enable/disable` 只操作 `HLFCYA`
- `latency_set` 只修改 `LATENCY[2:0]`
- 没有误写保留位

### 2. 启动路径验证

检查这些点：

- `SystemInit()` 最开始就配置 `FLASH_ACR`
- `latency = 2` 发生在切到 `PLL 72MHz` 之前

### 3. 读回验证

如果你后面加一个临时调试口或者断点，可以读回：

- `dri_ll_flash_latency_get() == DRI_LL_FLASH_LATENCY_2`
- `dri_ll_flash_prefetch_is_enabled() == true`

## 这份 Flash 文档不建议现在做的内容

下面这些先不要混进第一版：

- `Flash unlock`
- `Page erase`
- `Mass erase`
- `Half-word programming`
- `Busy wait`
- `EOP/PGERR/WRPRTERR` 清除逻辑
- `Option bytes`

原因是：

- 它们不是 `SystemInit()` 升频必需项
- 它们会把驱动从“普通 R/W 配置”升级成“带状态机和特殊写语义的编程控制器”
- 做对它们通常还需要 `PM0075`

## 推荐实现顺序

建议按下面顺序推进：

1. 先写 [dri_ll_flash.h](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll_flash.h) 的地址、位定义、枚举和函数声明
2. 再写 [dri_ll_flash.c](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll_flash.c) 的 `prefetch/halfcycle/latency`
3. 再把 [dri_ll_flash.c](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/driver/dri_ll_flash.c) 接入 `CMake`
4. 再在 [system_stm32f103c8t6.c](/home/ssw12/Projects/stm32f103c8t6_null/Libraries/system_stm32f103c8t6.c) 中最先调用 Flash 配置
5. 最后再继续写 RCC 切换到 `HSE -> PLL -> 72MHz`

## 第一版最小闭环

如果你只想尽快把时钟切换跑起来，最低限度做到这些就够了：

- [ ] `dri_ll_flash_prefetch_enable()`
- [ ] `dri_ll_flash_halfcycle_disable()`
- [ ] `dri_ll_flash_latency_set()`
- [ ] 在 `SystemInit()` 中先调用这三个函数
- [ ] 然后再执行 RCC 升频流程

## 一句话原则

一句话总结就是：

`Flash` 在这件事里的职责不是“烧写程序”，而是“在 CPU 升到 72MHz 之前，把 Flash 访问时序准备好”。
