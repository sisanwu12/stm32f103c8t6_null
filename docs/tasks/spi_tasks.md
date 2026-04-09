# STM32F103C8T6 SPI Tasks

这份清单用于拆解 `Libraries/driver/dri_ll_spi.h` 与 `Libraries/driver/dri_ll_spi.c` 的第一版开发工作，并把这版 SPI 库已经确定的职责边界、固定接口和实现顺序沉淀下来。

当前目标是先做出一套适合裸机寄存器开发的 low-level SPI 操作库：支持 `SPI1/SPI2`、主机模式、8-bit、阻塞轮询、默认引脚、超时返回，并且只负责 SPI 外设与 `SCK/MISO/MOSI` 的最小闭环。

## 当前项目状态

- 当前 SPI 模块文件：
  - `Libraries/driver/dri_ll_spi.h`
  - `Libraries/driver/dri_ll_spi.c`
- 当前 SPI 文件已经在工程中预留，但还没有完成正式实现。
- 当前工程已经具备这些可复用基础设施：
  - `Libraries/driver/dri_ll.h`
    - 已提供寄存器读写、改位、字段读取等基础能力。
  - `Libraries/driver/dri_ll_gpio.h`
  - `Libraries/driver/dri_ll_gpio.c`
    - 已提供 GPIO 初始化接口，可直接用于配置 `SCK/MISO/MOSI`。
  - `Libraries/driver/dri_ll_rcc.h`
  - `Libraries/driver/dri_ll_rcc.c`
    - 已提供 SPI 与 GPIO 时钟位定义，但本版 SPI 库不负责开时钟。
- 当前这版 SPI 的职责已经固定：
  - 负责：
    - 初始化 SPI 外设
    - 配置 GPIO，只配 `SCK/MISO/MOSI`
    - 等待状态位
    - 单字节收发
    - 缓冲区收发
    - 超时返回
  - 不负责：
    - `CS` 拉低拉高
    - 设备寄存器协议
    - 一帧命令的意义
    - 多个从设备仲裁
    - `AFIO remap`
    - 中断、DMA、CRC
    - 从机模式
    - `16-bit` 帧格式
- 当前这版 SPI 的固定约束已经明确：
  - 仅支持主机模式
  - 固定全双工
  - 固定 `8-bit`
  - 固定 `SSM=1`、`SSI=1`
  - 固定 `CRCEN=0`
  - 固定 `DFF=0`
  - 固定 `BIDIMODE=0`
  - 固定 `RXONLY=0`
  - 固定关闭 `I2S`
  - 超时统一使用轮询计数，不依赖系统时基
- 当前这版 SPI 的默认引脚固定为：
  - `SPI1`：`PA5=SCK`、`PA6=MISO`、`PA7=MOSI`
  - `SPI2`：`PB13=SCK`、`PB14=MISO`、`PB15=MOSI`
- 当前初始化前置条件固定为：
  - `SPI1` 使用前，外部必须先打开 `SPI1 + GPIOA` 时钟
  - `SPI2` 使用前，外部必须先打开 `SPI2 + GPIOB` 时钟
  - `dri_ll_spi_init()` 不负责开任何时钟

## 文档集合

- `RM0008 Rev 21` - STM32F101xx/102xx/103xx/105xx/107xx Reference manual
  - 仓库内文件：`docs/lib/rm0008-stm32f103xx参考手册.pdf`
  - SPI 寄存器、位定义、收发时序以它为准
- `DS5319 Rev 20` - STM32F103x8 / STM32F103xB Datasheet
  - 仓库内文件：`docs/lib/ds5319-stm32f103c8-datasheet.pdf`
  - 用于确认 `SPI1/SPI2` 默认引脚和封装引脚可用性
- `PM0056 Rev 7` - STM32F10xxx Cortex-M3 programming manual
  - 仓库内文件：`docs/lib/pm0056-stm32f10xxx-cortexm3-编程手册.pdf`
  - 用于理解裸机轮询、超时等待、异常行为时的辅助背景

## 第一版建议实现的函数

### 1. 初始化与等待

- [ ] `dri_ll_spi_status_t dri_ll_spi_init(const dri_ll_spi_init_t* config);`
- [ ] `dri_ll_spi_status_t dri_ll_spi_wait_flag_set(dri_ll_spi_instance_t instance, u32 flag_mask, u32 timeout_count);`
- [ ] `dri_ll_spi_status_t dri_ll_spi_wait_flag_clear(dri_ll_spi_instance_t instance, u32 flag_mask, u32 timeout_count);`

### 2. 单字节收发

- [ ] `dri_ll_spi_status_t dri_ll_spi_write_byte(dri_ll_spi_instance_t instance, u8 tx_byte, u32 timeout_count);`
- [ ] `dri_ll_spi_status_t dri_ll_spi_read_byte(dri_ll_spi_instance_t instance, u8* rx_byte, u32 timeout_count);`
- [ ] `dri_ll_spi_status_t dri_ll_spi_transfer_byte(dri_ll_spi_instance_t instance, u8 tx_byte, u8* rx_byte, u32 timeout_count);`

### 3. 缓冲区收发

- [ ] `dri_ll_spi_status_t dri_ll_spi_write_buffer(dri_ll_spi_instance_t instance, const u8* tx_buf, u32 len, u32* completed_len, u32 timeout_count);`
- [ ] `dri_ll_spi_status_t dri_ll_spi_read_buffer(dri_ll_spi_instance_t instance, u8* rx_buf, u32 len, u32* completed_len, u32 timeout_count);`
- [ ] `dri_ll_spi_status_t dri_ll_spi_transfer_buffer(dri_ll_spi_instance_t instance, const u8* tx_buf, u8* rx_buf, u32 len, u32* completed_len, u32 timeout_count);`

## 第一版固定接口与行为

### 1. 对外类型

- [ ] `dri_ll_spi_instance_t`
  - 固定提供 `SPI1`、`SPI2`
- [ ] `dri_ll_spi_mode_t`
  - 固定提供 `MODE0 ~ MODE3`
- [ ] `dri_ll_spi_baud_prescaler_t`
  - 固定提供 `DIV2 ~ DIV256`
- [ ] `dri_ll_spi_first_bit_t`
  - 固定提供 `MSB_FIRST`、`LSB_FIRST`
- [ ] `dri_ll_spi_status_t`
  - 固定提供 `OK`、`INVALID_PARAM`、`TIMEOUT`

### 2. 初始化结构体

- [ ] `dri_ll_spi_init_t`
  - 固定字段：
    - `instance`
    - `mode`
    - `baud_prescaler`
    - `first_bit`

### 3. 固定行为

- [ ] `read_*` 接口固定发送 dummy `0xFF` 产生时钟
- [ ] `write_*` 接口内部必须读回接收数据，避免遗留 `RXNE/OVR`
- [ ] `completed_len` 允许为 `NULL`
- [ ] `completed_len` 非空时，调用开始先清零
- [ ] 缓冲区收发每成功完成 `1` 字节，`completed_len` 递增 `1`
- [ ] 中途超时时保留已经完成的进度
- [ ] `timeout_count == 0` 立即返回 `TIMEOUT`

## Tasks

- [ ] 1. 明确 SPI v1 的职责边界并固化到头文件注释与任务文档
  - 目标：确保 SPI 模块只负责 SPI 外设本身和 `SCK/MISO/MOSI`，不越界承担设备级协议与片选管理。
  - 完成标准：`docs/tasks/spi_tasks.md` 与 `dri_ll_spi.h` 的文件头说明一致写明“负责什么、不负责什么、调用前置条件是什么”。
  - 依据文档：
    - 当前仓库内 `docs/tasks/gpio_tasks.md`
    - 当前仓库内 `Libraries/driver/dri_ll_gpio.h`

- [ ] 2. 核对 `SPI1/SPI2` 基地址、寄存器布局和偏移
  - 目标：确认 `SPI1/SPI2` 的基地址、寄存器成员顺序和 `STM32F1` 参考手册一致。
  - 完成标准：`dri_ll_spi_reg_t`、寄存器偏移宏、后续代码访问方式三者一致。
  - 依据文档：
    - `RM0008 Rev 21` `25 Serial peripheral interface (SPI)`
    - `RM0008 Rev 21` `25.5 SPI registers`
    - `DS5319 Rev 20` memory map 相关章节

- [ ] 3. 补齐 `CR1/CR2/SR/I2SCFGR` 第一版所需位定义
  - 目标：只引入第一版真正会用到的位定义，避免一开始把 CRC、DMA、中断等整套都搬进来。
  - 完成标准：至少覆盖 `CPHA/CPOL/MSTR/BR/SPE/LSBFIRST/SSI/SSM/RXONLY/DFF/BIDIMODE/BIDIOE`、`TXE/RXNE/BSY`、`I2SMOD`。
  - 依据文档：
    - `RM0008 Rev 21` `25.5.1 SPI_CR1`
    - `RM0008 Rev 21` `25.5.2 SPI_CR2`
    - `RM0008 Rev 21` `25.5.3 SPI_SR`
    - `RM0008 Rev 21` `25.5.7 SPI_I2SCFGR`

- [ ] 4. 设计并固定 SPI v1 的用户层类型和函数签名
  - 目标：一次性锁定这版对外接口，避免实现过程中不断扩参数。
  - 完成标准：实例、模式、分频、位序、状态码、初始化结构体和 9 个对外函数的签名全部确定，并与任务文档一致。
  - 依据文档：
    - 当前仓库内 `Libraries/data_type.h`
    - 当前仓库内 `Libraries/driver/dri_ll_gpio.h`

- [ ] 5. 完成实例合法性检查与寄存器入口映射
  - 目标：避免非法实例值进入寄存器访问逻辑，导致地址越界或未定义行为。
  - 完成标准：实例校验与实例到寄存器入口获取函数独立存在，非法实例会被安全拒绝。
  - 依据文档：
    - 当前仓库内 `Libraries/driver/dri_ll_gpio.c`

- [ ] 6. 完成默认引脚映射并复用 GPIO 初始化接口
  - 目标：把 `SPI1/SPI2` 默认引脚的 GPIO 配置在 SPI 初始化阶段统一完成。
  - 完成标准：
    - `SPI1` 固定配置 `PA5/PA6/PA7`
    - `SPI2` 固定配置 `PB13/PB14/PB15`
    - `SCK/MOSI` 配 `GPIO_MODE_AF_PP + GPIO_SPEED_50MHZ`
    - `MISO` 配 `GPIO_MODE_INPUT_FLOATING`
    - 不配置 `NSS/CS`
  - 依据文档：
    - `DS5319 Rev 20` alternate function pin mapping 相关章节
    - 当前仓库内 `Libraries/driver/dri_ll_gpio.h`
    - 当前仓库内 `Libraries/driver/dri_ll_gpio.c`

- [ ] 7. 实现通用状态位等待接口并加入超时返回
  - 目标：提供最基础的轮询等待能力，供初始化后续收发流程复用。
  - 完成标准：
    - `dri_ll_spi_wait_flag_set()` 和 `dri_ll_spi_wait_flag_clear()` 均使用递减轮询计数
    - `timeout_count == 0` 立即超时
    - 不依赖定时器、Tick 或 RTOS
  - 依据文档：
    - 当前仓库内 `Libraries/driver/dri_ll.h`
    - `RM0008 Rev 21` `25.3.7 Status flag behavior`

- [ ] 8. 实现 SPI 初始化接口
  - 目标：完成 SPI 主机模式最小初始化闭环。
  - 完成标准：
    - 校验 `config` 和全部枚举参数
    - 先关闭 `SPE`
    - 确认关闭 `I2S`
    - 配置默认 GPIO
    - 写入 `CR1`
    - 最后打开 `SPE`
    - 不在 SPI 模块内打开任何 RCC 时钟
  - 依据文档：
    - `RM0008 Rev 21` `25.3 Functional description`
    - `RM0008 Rev 21` `25.3.1 Master and slave modes`
    - 当前仓库内 `Libraries/driver/dri_ll_rcc.h`

- [ ] 9. 实现单字节全双工传输接口
  - 目标：把最小 SPI 事务固化为稳定的阻塞式 1-byte 传输流程。
  - 完成标准：
    - 先等 `TXE`
    - 写 `DR`
    - 再等 `RXNE`
    - 读 `DR`
    - 最后等 `BSY` 清零
    - 任一等待点超时立即返回 `TIMEOUT`
  - 依据文档：
    - `RM0008 Rev 21` `25.3.5 Data transmission and reception procedures`
    - `RM0008 Rev 21` `25.3.7 Status flag behavior`

- [ ] 10. 基于全双工接口实现单字节写和单字节读
  - 目标：避免重复维护两套底层流程，让所有单字节收发都复用统一路径。
  - 完成标准：
    - `write_byte()` 基于 `transfer_byte()` 实现，并丢弃回读值
    - `read_byte()` 基于 `transfer_byte(0xFF, ...)` 实现
  - 依据文档：
    - 当前仓库内后续 `dri_ll_spi.c` 设计

- [ ] 11. 基于单字节接口实现缓冲区收发
  - 目标：在不引入 DMA 和中断的前提下，提供可直接使用的多字节阻塞接口。
  - 完成标准：
    - `write_buffer()` 逐字节发送并丢弃收到的数据
    - `read_buffer()` 逐字节发送 `0xFF`
    - `transfer_buffer()` 同时处理 `tx/rx`
    - `completed_len` 行为符合任务文档定义
  - 依据文档：
    - 当前仓库内后续 `dri_ll_spi.c` 设计

- [ ] 12. 完善参数校验与超时行为
  - 目标：让 SPI 第一版在非法参数和异常硬件状态下能安全返回，而不是卡死。
  - 完成标准：
    - `config == NULL` 返回 `INVALID_PARAM`
    - 非法实例或非法枚举返回 `INVALID_PARAM`
    - `rx_byte == NULL` 返回 `INVALID_PARAM`
    - `tx_buf/rx_buf == NULL && len > 0` 返回 `INVALID_PARAM`
    - `len == 0` 返回 `OK` 且 `completed_len == 0`
  - 依据文档：
    - 当前仓库内 `Libraries/driver/dri_ll_gpio.c`

- [ ] 13. 补充 SPI 使用说明与最小板级示例
  - 目标：让后续联调者清楚调用顺序与边界条件，避免把 SPI 库当成设备协议库使用。
  - 完成标准：
    - 至少提供 `SPI1` 默认脚示例
    - 至少提供 `SPI2` 默认脚示例
    - 示例顺序固定为“先开 RCC，再初始化 SPI，再手动控制 `CS`，再收发数据”
  - 依据文档：
    - 当前仓库内 `docs/接口说明书.md`
    - 当前仓库内 `Libraries/driver/dri_ll_rcc.h`

- [ ] 14. 完成宿主编译与 ARM 交叉编译检查
  - 目标：确保 SPI 第一版接入工程后不会破坏现有构建流程。
  - 完成标准：新增 SPI 文件被 `project/CMakeLists.txt` 自动收集，并通过宿主检查和 `arm-none-eabi` 交叉编译检查。
  - 依据文档：
    - 当前仓库内 `project/CMakeLists.txt`
    - 当前仓库内 `.vscode/tasks.json`

## 推荐实现顺序

1. 先完成 Task 2、Task 3 和 Task 4，把寄存器定义、位定义、公共接口一次定死。
2. 再完成 Task 5、Task 6 和 Task 7，把实例映射、默认引脚和等待超时这些共用基础能力做好。
3. 然后完成 Task 8、Task 9、Task 10 和 Task 11，打通初始化到单字节/多字节收发的最小功能闭环。
4. 最后完成 Task 12、Task 13 和 Task 14，把参数边界、文档示例和构建验证一起补齐。

## 第一版最小闭环

如果目标是尽快拿 SPI 去驱动一个最简单的从设备，第一版至少做到这些：

- [ ] `dri_ll_spi_init()`
- [ ] `dri_ll_spi_wait_flag_set()`
- [ ] `dri_ll_spi_wait_flag_clear()`
- [ ] `dri_ll_spi_transfer_byte()`
- [ ] `dri_ll_spi_write_byte()`
- [ ] `dri_ll_spi_read_byte()`
- [ ] `dri_ll_spi_transfer_buffer()`

## 一句话原则

SPI 模块第一版只负责“把 SPI 外设和默认 `SCK/MISO/MOSI` 稳定驱动起来”，不负责 `CS` 管理，不负责设备协议，也不提前为第二版扩展埋入复杂配置。
