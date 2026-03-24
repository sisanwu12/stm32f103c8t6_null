# STM32F103C8T6 GPIO Tasks

这份清单用于沉淀当前 `driver/dri_ll_gpio.h` 与 `driver/dri_ll_gpio.c` 的开发结论，记录已经完成的审查项，并列出后续继续推进 GPIO 模块时需要执行的任务。

## 当前项目状态

- 当前 GPIO 模块文件：
  - `driver/dri_ll_gpio.h`
  - `driver/dri_ll_gpio.c`
- 当前模块采用三层结构：
  - 地址定义层：定义 GPIO 基地址、结束地址、寄存器偏移、寄存器结构体。
  - 基础操作层：提供寄存器入口获取、pin mask 生成、原始寄存器读写、CRL/CRH 配置。
  - 用户接口层：提供 `init`、单引脚读写、翻转、整口读写等接口。
- 当前已经完成地址复查：
  - `AFIO = 0x40010000`
  - `EXTI = 0x40010400`
  - `GPIOA = 0x40010800`
  - `GPIOB = 0x40010C00`
  - `GPIOC = 0x40011000`
  - `GPIOD = 0x40011400`
  - `GPIOE = 0x40011800`
- 当前已经确认 GPIO 寄存器偏移正确：
  - `CRL = 0x00`
  - `CRH = 0x04`
  - `IDR = 0x08`
  - `ODR = 0x0C`
  - `BSRR = 0x10`
  - `BRR = 0x14`
  - `LCKR = 0x18`
- 当前 `RCC` 已经从 GPIO 模块中剥离：
  - GPIO 不再定义 `RCC` 基地址、`APB2ENR` 位、`RCC` 寄存器结构体。
  - GPIO 不再负责打开端口时钟。
  - 在调用 `dri_ll_gpio_init()` 或其他 GPIO 接口之前，必须先由外部 `RCC` 模块打开对应端口时钟。
- 当前已修复此前审查发现的 3 个风险：
  - 非法 `port/pin` 参数校验已补强，避免负值枚举导致未定义行为。
  - 用户层不再公开保留模式位型，基础层 raw 接口和初始化接口都会拒绝写入保留值 `0xC`。
  - `dri_ll_gpio_toggle_pin()` 在 Cortex-M 目标上会进入短临界区，降低与中断并发访问时的状态竞争风险。
- 当前代码已通过宿主机和 ARM 交叉编译检查。

## 文档集合

- `RM0008 Rev 21` - STM32F101xx/102xx/103xx/105xx/107xx Reference manual
  - 仓库内文件：`docs/lib/rm0008-stm32f103xx参考手册.pdf`
- `DS5319 Rev 20` - STM32F103x8 / STM32F103xB Datasheet
  - 仓库内文件：`docs/lib/ds5319-stm32f103c8-datasheet.pdf`
- `PM0056 Rev 7` - STM32F10xxx Cortex-M3 programming manual
  - 仓库内文件：`docs/lib/pm0056-stm32f10xxx-cortexm3-编程手册.pdf`

## Tasks

- [x] 1. 核对 GPIO 地址块与 datasheet memory map
  - 目标：确认 GPIOA ~ GPIOE、AFIO、EXTI 的基地址与地址跨度和官方内存映射一致。
  - 完成标准：`driver/dri_ll_gpio.h` 中的基地址、结束地址与 datasheet memory map 一致，没有偏移错误或端口错位。
  - 依据文档：
    - `DS5319 Rev 20` `Figure 11. Memory map`
    - `RM0008 Rev 21` `3.3 Memory map`

- [x] 2. 核对 GPIO 寄存器偏移与结构体布局
  - 目标：确认 `CRL/CRH/IDR/ODR/BSRR/BRR/LCKR` 的偏移定义、结构体成员顺序和 STM32F1 参考手册一致。
  - 完成标准：头文件中的偏移宏、`dri_ll_gpio_reg_t` 的布局、代码中的寄存器访问方式三者一致。
  - 依据文档：
    - `RM0008 Rev 21` `9 General-purpose and alternate-function I/Os (GPIOs)`
    - `RM0008 Rev 21` `9.2 GPIO registers`

- [x] 3. 完成 GPIO 三层结构封装
  - 目标：按约定完成地址定义层、基础操作层、用户接口层三层组织，并保持接口职责清晰。
  - 完成标准：头文件和源文件已经按三层方式组织，且基础层接口与用户层接口已经拆开。
  - 依据文档：
    - 当前仓库内 `driver/dri_ll_gpio.h`
    - 当前仓库内 `driver/dri_ll_gpio.c`

- [x] 4. 从 GPIO 模块中剥离 RCC 内容
  - 目标：让 GPIO 模块只负责 GPIO 本身，不重复维护时钟控制逻辑。
  - 完成标准：GPIO 文件中不再定义 `RCC` 基地址、寄存器结构体、时钟位和开时钟函数；初始化前由外部 `RCC` 模块先开时钟。
  - 依据文档：
    - `RM0008 Rev 21` `7 Reset and clock control (RCC)`
    - 当前仓库内 `driver/dri_ll_gpio.h`
    - 当前仓库内 `driver/dri_ll_gpio.c`

- [x] 5. 修复非法 port/pin 参数风险
  - 目标：避免非法枚举值进入位移和寄存器访问逻辑，导致未定义行为。
  - 完成标准：`port` 与 `pin` 都经过有符号边界检查，非法值会被安全拒绝。
  - 依据文档：
    - 当前仓库内 `driver/dri_ll_gpio.c`

- [x] 6. 禁止保留模式写入寄存器
  - 目标：避免用户把 GPIO 初始化到 STM32F1 手册中的保留配置位型。
  - 完成标准：用户层不公开保留模式枚举，且基础层 raw 配置接口与初始化接口都不会把保留值 `0xC` 写入 `CRL/CRH`。
  - 依据文档：
    - `RM0008 Rev 21` `9.1.11 Port bit configuration table`
    - 当前仓库内 `driver/dri_ll_gpio.h`
    - 当前仓库内 `driver/dri_ll_gpio.c`

- [x] 7. 降低 toggle 读改写竞争风险
  - 目标：在不改变现有对外接口的前提下，减小 `toggle` 与中断并发访问同一端口时的竞争窗口。
  - 完成标准：在 Cortex-M 目标上，`dri_ll_gpio_toggle_pin()` 会在短临界区内完成读取 `ODR` 和写入 `BSRR`。
  - 依据文档：
    - `PM0056 Rev 7` `2.3 Exceptions`
    - 当前仓库内 `driver/dri_ll_gpio.c`

- [x] 8. 完善对外文档
  - 目标：让后续团队成员和开源使用者能快速理解模块职责、使用限制和接口行为。
  - 完成标准：已经补齐文件头 `@brief/@note`，并提供 GPIO 接口说明书。
  - 依据文档：
    - 当前仓库内 `driver/dri_ll_gpio.h`
    - 当前仓库内 `driver/dri_ll_gpio.c`
    - 当前仓库内 `docs/接口说明书.md`

- [ ] 9. 接入团队统一的 RCC 接口
  - 目标：在实际板级初始化流程中，把 GPIO 使用前的时钟开启交给你们团队的 `RCC` 模块完成。
  - 完成标准：上层调用路径已经明确为“先开 GPIO 时钟，再调 `dri_ll_gpio_init()`”，且不再出现 GPIO 模块私自维护 RCC 定义的情况。
  - 依据文档：
    - 团队后续的 `dri_ll_rcc` 设计文档
    - 当前仓库内 `driver/dri_ll_gpio.h`

- [ ] 10. 增加一个最小板级示例
  - 目标：给后续联调提供一个稳定的参考用例，例如 `PC13` 点灯或 `PA0` 按键输入。
  - 完成标准：示例中已经体现“先开 RCC 时钟，再初始化 GPIO，再进行读写控制”的完整顺序。
  - 依据文档：
    - 当前仓库内 `docs/接口说明书.md`
    - `RM0008 Rev 21` `9 General-purpose and alternate-function I/Os (GPIOs)`

- [ ] 11. 增加最小自检或编译验证流程
  - 目标：在后续继续开发 `AFIO`、`EXTI`、`RCC` 等模块时，确保 GPIO 基础能力不会被改坏。
  - 完成标准：至少保留宿主编译和 `arm-none-eabi-gcc` 交叉编译两类检查；如果后续接入脚本或 CI，则把 GPIO 编译检查固化进去。
  - 依据文档：
    - 当前仓库构建脚本
    - 当前仓库内 `driver/dri_ll_gpio.c`

- [ ] 12. 评估 AFIO 和 EXTI 的职责边界
  - 目标：明确 `AFIO`、`EXTI` 的地址常量是否继续保留在 GPIO 头文件中，还是后续拆分到独立模块。
  - 完成标准：如果团队后续会单独开发 `dri_ll_afio` / `dri_ll_exti`，则决定公共常量的归属并避免重复维护。
  - 依据文档：
    - `RM0008 Rev 21` `8 Alternate function I/O and debug configuration (AFIO)`
    - `RM0008 Rev 21` `10 External interrupt/event controller (EXTI)`

## 建议执行顺序

1. 先完成 Task 9，把 GPIO 与团队 `RCC` 模块正式接起来，先打通真实调用链路。
2. 再完成 Task 10，准备一个最小板级示例，方便后续硬件联调和回归验证。
3. 最后完成 Task 11 和 Task 12，把验证流程与模块边界进一步固化下来。

## 一句话原则

GPIO 模块只负责“地址映射、模式配置、电平读写”，不负责时钟开启；所有后续扩展都尽量围绕这个职责边界继续推进。
