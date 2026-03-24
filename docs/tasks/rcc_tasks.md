# STM32F103C8T6 RCC Tasks

这份清单用于拆解 `driver/dri_ll_rcc.c` 与 `driver/dri_ll_rcc.h` 需要完成的工作，并标出每一项应参考的 ST 官方文档位置。

当前目标是先做出一套适合裸机寄存器开发的 low-level RCC 操作库：能开关时钟、切换系统时钟源、配置预分频、控制外设时钟门控，并能读取关键状态。

## 文档集合

- `RM0008 Rev 21` - `docs/lib/rm0008-stm32f103xx参考手册.pdf`
  - RCC 的主文档，寄存器和位定义都以它为准
- `DS5319 Rev 20` - `docs/lib/ds5319-stm32f103c8-datasheet.pdf`
  - 用来确认时钟源边界、默认启动时钟、外部晶振/低速晶振特性
- `PM0056 Rev 7` - `docs/lib/pm0056-stm32f10xxx-cortexm3-编程手册.pdf`
  - 不是 RCC 位定义主文档，但在理解内核时钟影响、SysTick 时有辅助价值

## 第一版建议实现的函数

### 1. 基础寄存器访问
- [ ] `void dri_ll_rcc_write_reg(uint32_t offset, uint32_t value);`
- [ ] `uint32_t dri_ll_rcc_read_reg(uint32_t offset);`
- [ ] `void dri_ll_rcc_set_bits(uint32_t offset, uint32_t mask);`
- [ ] `void dri_ll_rcc_clear_bits(uint32_t offset, uint32_t mask);`
- [ ] `void dri_ll_rcc_modify_reg(uint32_t offset, uint32_t clear_mask, uint32_t set_mask);`

### 2. 时钟源控制
- [ ] `void dri_ll_rcc_hsi_enable(void);`
- [ ] `void dri_ll_rcc_hsi_disable(void);`
- [ ] `uint32_t dri_ll_rcc_hsi_is_ready(void);`
- [ ] `void dri_ll_rcc_hse_enable(void);`
- [ ] `void dri_ll_rcc_hse_disable(void);`
- [ ] `uint32_t dri_ll_rcc_hse_is_ready(void);`
- [ ] `void dri_ll_rcc_hse_bypass_enable(void);`
- [ ] `void dri_ll_rcc_hse_bypass_disable(void);`
- [ ] `void dri_ll_rcc_pll_enable(void);`
- [ ] `void dri_ll_rcc_pll_disable(void);`
- [ ] `uint32_t dri_ll_rcc_pll_is_ready(void);`

### 3. 系统时钟选择与预分频
- [ ] `void dri_ll_rcc_set_sysclk_source(uint32_t source);`
- [ ] `uint32_t dri_ll_rcc_get_sysclk_source(void);`
- [ ] `uint32_t dri_ll_rcc_get_sysclk_status(void);`
- [ ] `void dri_ll_rcc_set_ahb_prescaler(uint32_t value);`
- [ ] `void dri_ll_rcc_set_apb1_prescaler(uint32_t value);`
- [ ] `void dri_ll_rcc_set_apb2_prescaler(uint32_t value);`
- [ ] `void dri_ll_rcc_set_adc_prescaler(uint32_t value);`

### 4. PLL 配置
- [ ] `void dri_ll_rcc_set_pll_source(uint32_t source);`
- [ ] `void dri_ll_rcc_set_pll_mul(uint32_t value);`
- [ ] `uint32_t dri_ll_rcc_get_pll_source(void);`
- [ ] `uint32_t dri_ll_rcc_get_pll_mul(void);`

### 5. 外设时钟门控
- [ ] `void dri_ll_rcc_ahb_enable(uint32_t mask);`
- [ ] `void dri_ll_rcc_ahb_disable(uint32_t mask);`
- [ ] `void dri_ll_rcc_apb1_enable(uint32_t mask);`
- [ ] `void dri_ll_rcc_apb1_disable(uint32_t mask);`
- [ ] `void dri_ll_rcc_apb2_enable(uint32_t mask);`
- [ ] `void dri_ll_rcc_apb2_disable(uint32_t mask);`
- [ ] `uint32_t dri_ll_rcc_ahb_is_enabled(uint32_t mask);`
- [ ] `uint32_t dri_ll_rcc_apb1_is_enabled(uint32_t mask);`
- [ ] `uint32_t dri_ll_rcc_apb2_is_enabled(uint32_t mask);`

### 6. 外设复位
- [ ] `void dri_ll_rcc_apb1_force_reset(uint32_t mask);`
- [ ] `void dri_ll_rcc_apb1_release_reset(uint32_t mask);`
- [ ] `void dri_ll_rcc_apb2_force_reset(uint32_t mask);`
- [ ] `void dri_ll_rcc_apb2_release_reset(uint32_t mask);`
- [ ] `void dri_ll_rcc_reset_pulse_apb1(uint32_t mask);`
- [ ] `void dri_ll_rcc_reset_pulse_apb2(uint32_t mask);`

### 7. 可选增强项
- [ ] `void dri_ll_rcc_lsi_enable(void);`
- [ ] `void dri_ll_rcc_lse_enable(void);`
- [ ] `void dri_ll_rcc_set_rtc_source(uint32_t source);`
- [ ] `uint32_t dri_ll_rcc_get_reset_flags(void);`
- [ ] `void dri_ll_rcc_clear_reset_flags(void);`
- [ ] `uint32_t dri_ll_rcc_get_sysclk_hz(void);`
- [ ] `uint32_t dri_ll_rcc_get_hclk_hz(void);`
- [ ] `uint32_t dri_ll_rcc_get_pclk1_hz(void);`
- [ ] `uint32_t dri_ll_rcc_get_pclk2_hz(void);`
- [ ] `void dri_ll_rcc_enable_css(void);`

## 任务拆分与查阅位置

- [ ] 1. 定义 RCC 基地址、寄存器结构、寄存器偏移和位掩码
  - 重点文档：
  - `RM0008` `7.3 RCC registers`

- [ ] 2. 实现 `CR` 中 HSI/HSE/PLL 的开关和就绪状态查询
  - 重点文档：
  - `RM0008` `7.3.1 RCC_CR`
  - `RM0008` `7.3.2 RCC_CFGR`
  - `DS5319` `2.3.7 Clocks and startup`

- [ ] 3. 实现系统时钟源切换和 `SW/SWS` 状态查询
  - 重点文档：
  - `RM0008` `7.3.2 RCC_CFGR`
  - `DS5319` `2.3.7 Clocks and startup`

- [ ] 4. 实现 `HPRE`、`PPRE1`、`PPRE2`、`ADCPRE`
  - 重点文档：
  - `RM0008` `7.3.2 RCC_CFGR`
  - `DS5319` 时钟相关限制章节
  - 注意：
  - `APB1` 有最高频率限制，不能只会写位

- [ ] 5. 实现 `PLLSRC`、`PLLXTPRE`、`PLLMUL`
  - 重点文档：
  - `RM0008` `7.3.2 RCC_CFGR`
  - `DS5319` `2.3.7 Clocks and startup`
  - 注意：
  - 修改 PLL 配置前要先关闭 PLL

- [ ] 6. 实现 `AHBENR`、`APB1ENR`、`APB2ENR` 的门控函数
  - 重点文档：
  - `RM0008` `7.3.6 RCC_AHBENR`
  - `RM0008` `7.3.7 RCC_APB2ENR`
  - `RM0008` `7.3.8 RCC_APB1ENR`

- [ ] 7. 实现 `APB1RSTR`、`APB2RSTR` 的外设复位函数
  - 重点文档：
  - `RM0008` `7.3.4 RCC_APB2RSTR`
  - `RM0008` `7.3.5 RCC_APB1RSTR`

- [ ] 8. 实现 `BDCR`、`CSR` 中的 LSE/LSI/RTC/reset flags
  - 重点文档：
  - `RM0008` `7.3.9 RCC_BDCR`
  - `RM0008` `7.3.10 RCC_CSR`
  - `DS5319` 低速时钟相关章节

- [ ] 9. 实现时钟频率查询辅助函数
  - 重点文档：
  - `RM0008` `7.3.2 RCC_CFGR`
  - `DS5319` `2.3.7 Clocks and startup`

- [ ] 10. 第二阶段再补 `RCC_CIR` 和 `CSS`
  - 重点文档：
  - `RM0008` `7.3.3 RCC_CIR`
  - `RM0008` `7.3.1 RCC_CR`

## 推荐实现顺序

1. 先做寄存器定义和位掩码
2. 再做 `AHB/APB1/APB2` 时钟门控
3. 再做 `HSI/HSE/PLL` 开关和状态
4. 再做系统时钟切换和预分频
5. 最后补充 `BDCR/CSR/CIR`、频率查询和增强项

## 第一版最小闭环

如果目标是尽快开始写 `GPIO` 和点灯程序，第一版至少做到这些：

- [ ] `dri_ll_rcc_apb2_enable()`
- [ ] `dri_ll_rcc_apb2_disable()`
- [ ] `dri_ll_rcc_apb2_is_enabled()`
- [ ] `dri_ll_rcc_hse_enable()`
- [ ] `dri_ll_rcc_hse_is_ready()`
- [ ] `dri_ll_rcc_set_sysclk_source()`
- [ ] `dri_ll_rcc_get_sysclk_status()`
- [ ] `dri_ll_rcc_set_ahb_prescaler()`
- [ ] `dri_ll_rcc_set_apb1_prescaler()`
- [ ] `dri_ll_rcc_set_apb2_prescaler()`

一句话原则：`RCC` 库第一职责不是“决定板子跑多少 MHz”，而是“把时钟相关寄存器安全、清晰、可复用地操作起来”。
