/**
 * @file dri_ll_rcc.h
 * @author sisanwu12
 * @brief
 * @version 0.1
 * @date 2026-03-24
 *
 */

#ifndef __DRI_LL_RCC_H__
#define __DRI_LL_RCC_H__

#include "data_type.h"

/* ========== 数据定义 ==========*/

/* ---------- RCC 地址定义层 ---------- */

/* RCC 基地址 */
#define DRI_LL_RCC_BASE_ADDR 0x40021000UL

/* RCC 寄存器偏移地址 */
#define DRI_LL_RCC_CR_OFFSET       0x00UL // 时钟控制寄存器
#define DRI_LL_RCC_CFGR_OFFSET     0x04UL // 时钟配置寄存器
#define DRI_LL_RCC_CIR_OFFSET      0x08UL // 时钟中断寄存器
#define DRI_LL_RCC_APB2RSTR_OFFSET 0x0CUL // APB2 外设复位寄存器
#define DRI_LL_RCC_APB1RSTR_OFFSET 0x10UL // APB1 外设复位寄存器
#define DRI_LL_RCC_AHBENR_OFFSET   0x14UL // AHB 外设时钟使能寄存器
#define DRI_LL_RCC_APB2ENR_OFFSET  0x18UL // APB2 外设时钟使能寄存器
#define DRI_LL_RCC_APB1ENR_OFFSET  0x1CUL // APB1 外设时钟使能寄存器
#define DRI_LL_RCC_BDCR_OFFSET     0x20UL // 备份域控制寄存器
#define DRI_LL_RCC_CSR_OFFSET      0x24UL // 控制/状态寄存器

/* ---------- 寄存器位定义 ---------- */

/* cr寄存器位定义 */
typedef enum
{
    RCC_CR_HSION   = (1UL << 0), // HSI 时钟使能位
    RCC_CR_HSIRDY  = (1UL << 1), // HSI 就绪位
    RCC_CR_HSITRI0 = (1UL << 3), // HSI 调谐位 0
    RCC_CR_HSITRI1 = (1UL << 4), // HSI 调谐位 1
    RCC_CR_HSITRI2 = (1UL << 5), // HSI 调谐位 2
    RCC_CR_HSITRI3 = (1UL << 6), // HSI 调谐位 3
    RCC_CR_HSITRI4 = (1UL << 7), // HSI 调谐位 4
    RCC_CR_HSITRI  = (RCC_CR_HSITRI0 | RCC_CR_HSITRI1 | RCC_CR_HSITRI2 | RCC_CR_HSITRI3 |
                     RCC_CR_HSITRI4), // HSI 调谐位掩码

    RCC_CR_HSICAL0 = (1UL << 8),  // HSI 校准位 0
    RCC_CR_HSICAL1 = (1UL << 9),  // HSI 校准位 1
    RCC_CR_HSICAL2 = (1UL << 10), // HSI 校准位 2
    RCC_CR_HSICAL3 = (1UL << 11), // HSI 校准位 3
    RCC_CR_HSICAL4 = (1UL << 12), // HSI 校准位 4
    RCC_CR_HSICAL  = (RCC_CR_HSICAL0 | RCC_CR_HSICAL1 | RCC_CR_HSICAL2 | RCC_CR_HSICAL3 |
                     RCC_CR_HSICAL4), // HSI 校准位掩码

    RCC_CR_HSEON   = (1UL << 16), // HSE 时钟使能位
    RCC_CR_HSERDY  = (1UL << 17), // HSE 就绪位
    RCC_CR_HSEBYP  = (1UL << 18), // HSE 过渡使能位
    RCC_CR_CSSON   = (1UL << 19), // 时钟安全系统使能位
    RCC_CR_PLLON   = (1UL << 24), // PLL 时钟使能位
    RCC_CR_PLLRDY  = (1UL << 25), // PLL 就绪位
    RCC_CR_PLL2ON  = (1UL << 26), // PLL2 时钟使能位
    RCC_CR_PLL2RDY = (1UL << 27), // PLL2 就绪位
    RCC_CR_PLL3ON  = (1UL << 28), // PLL3 时钟使能位
    RCC_CR_PLL3RDY = (1UL << 29), // PLL3 就绪位
} dri_ll_rcc_cr_bits;

/* cfgr寄存器位定义 */
typedef enum
{
    RCC_CFGR_SW0 = (1UL << 0),                    // 系统时钟选择位 0
    RCC_CFGR_SW1 = (1UL << 1),                    // 系统时钟选择位 1
    RCC_CFGR_SW  = (RCC_CFGR_SW0 | RCC_CFGR_SW1), // 系统时钟选择位掩码

    RCC_CFGR_SWS0 = (1UL << 2),                      // 系统时钟状态位 0
    RCC_CFGR_SWS1 = (1UL << 3),                      // 系统时钟状态位 1
    RCC_CFGR_SWS  = (RCC_CFGR_SWS0 | RCC_CFGR_SWS1), // 系统时钟状态位掩码

    RCC_CFGR_HPRE0 = (1UL << 4), // AHB 预分频位 0
    RCC_CFGR_HPRE1 = (1UL << 5), // AHB 预分频位 1
    RCC_CFGR_HPRE2 = (1UL << 6), // AHB 预分频位 2
    RCC_CFGR_HPRE3 = (1UL << 7), // AHB 预分频位 3
    RCC_CFGR_HPRE =
        (RCC_CFGR_HPRE0 | RCC_CFGR_HPRE1 | RCC_CFGR_HPRE2 | RCC_CFGR_HPRE3), // AHB 预分频位掩码

    RCC_CFGR_PPRE10 = (1UL << 8),                                            // APB1 预分频位 0
    RCC_CFGR_PPRE11 = (1UL << 9),                                            // APB1 预分频位 1
    RCC_CFGR_PPRE12 = (1UL << 10),                                           // APB1 预分频位 2
    RCC_CFGR_PPRE1  = (RCC_CFGR_PPRE10 | RCC_CFGR_PPRE11 | RCC_CFGR_PPRE12), // APB1 预分频位掩码

    RCC_CFGR_PPRE20 = (1UL << 11),                                           // APB2 预分频位 0
    RCC_CFGR_PPRE21 = (1UL << 12),                                           // APB2 预分频位 1
    RCC_CFGR_PPRE22 = (1UL << 13),                                           // APB2 预分频位 2
    RCC_CFGR_PPRE2  = (RCC_CFGR_PPRE20 | RCC_CFGR_PPRE21 | RCC_CFGR_PPRE22), // APB2 预分频位掩码

    RCC_CFGR_ADCPRE0 = (1UL << 14),                           // ADC 预分频位 0
    RCC_CFGR_ADCPRE1 = (1UL << 15),                           // ADC 预分频位 1
    RCC_CFGR_ADCPRE  = (RCC_CFGR_ADCPRE0 | RCC_CFGR_ADCPRE1), // ADC 预分频位掩码

    RCC_CFGR_PLLSRC   = (1UL << 16), // PLL 时钟源选择位
    RCC_CFGR_PLLXTPRE = (1UL << 17), // PLL HSE 分频位
    RCC_CFGR_PLLMUL0  = (1UL << 18), // PLL 倍频位 0
    RCC_CFGR_PLLMUL1  = (1UL << 19), // PLL 倍频位 1
    RCC_CFGR_PLLMUL2  = (1UL << 20), // PLL 倍频位 2
    RCC_CFGR_PLLMUL3  = (1UL << 21), // PLL 倍频位 3
    RCC_CFGR_PLLMUL   = (RCC_CFGR_PLLMUL0 | RCC_CFGR_PLLMUL1 | RCC_CFGR_PLLMUL2 |
                       RCC_CFGR_PLLMUL3), // PLL 倍频位掩码

    RCC_CFGR_OTGFSPRE = (1UL << 22),                                     // OTG FS 预分频位 0
    RCC_CFGR_MCO0     = (1UL << 24),                                     // MCO 输出位 0
    RCC_CFGR_MCO1     = (1UL << 25),                                     // MCO 输出位 1
    RCC_CFGR_MCO2     = (1UL << 26),                                     // MCO 输出位 2
    RCC_CFGR_MCO      = (RCC_CFGR_MCO0 | RCC_CFGR_MCO1 | RCC_CFGR_MCO2), // MCO 输出位掩码
} dri_ll_rcc_cfgr_bits;

/* ahb 高速时钟外设 */
typedef enum
{
    RCC_AHB_DMA1  = (1UL << 0),  // DMA1 时钟使能位
    RCC_AHB_DMA2  = (1UL << 1),  // DMA2 时钟使能位
    RCC_AHB_SRAM  = (1UL << 2),  // SRAM 时钟使能位
    RCC_AHB_FLITF = (1UL << 4),  // FLITF 时钟使能位
    RCC_AHB_CRC   = (1UL << 6),  // CRC 时钟使能位
    RCC_AHB_FSMC  = (1UL << 8),  // FSMC 时钟使能位
    RCC_AHB_SDIO  = (1UL << 10), // SDIO 时钟使能位
} dri_ll_rcc_ahbenr_bits;

/* apb1 低速外设 */
typedef enum
{
    RCC_APB1_TIM2   = (1UL << 0),  // TIM2 时钟使能位
    RCC_APB1_TIM3   = (1UL << 1),  // TIM3 时钟使能位
    RCC_APB1_TIM4   = (1UL << 2),  // TIM4 时钟使能位
    RCC_APB1_WWDG   = (1UL << 11), // WWDG 时钟使能位
    RCC_APB1_SPI2   = (1UL << 14), // SPI2 时钟使能位
    RCC_APB1_SPI3   = (1UL << 15), // SPI3 时钟使能位
    RCC_APB1_USART2 = (1UL << 17), // USART2 时钟使能位
    RCC_APB1_USART3 = (1UL << 18), // USART3 时钟使能位
    RCC_APB1_I2C1   = (1UL << 21), // I2C1 时钟使能位
    RCC_APB1_I2C2   = (1UL << 22), // I2C2 时钟使能位
    RCC_APB1_USB    = (1UL << 23), // USB 时钟使能位
    RCC_APB1_CAN    = (1UL << 25), // CAN 时钟使能位
    RCC_APB1_BKP    = (1UL << 27), // BKP 时钟使能位
    RCC_APB1_PWR    = (1UL << 28), // PWR 时钟使能位
    RCC_APB1_DAC    = (1UL << 29), // DAC 时钟使能位
} dri_ll_rcc_apb1enr_bits;

/* apb2 高速外设 */
typedef enum
{
    RCC_APB2_AFIO   = (1UL << 0),  // AFIO 时钟使能位
    RCC_APB2_GPIOA  = (1UL << 2),  // GPIOA 时钟使能位
    RCC_APB2_GPIOB  = (1UL << 3),  // GPIOB 时钟使能位
    RCC_APB2_GPIOC  = (1UL << 4),  // GPIOC 时钟使能位
    RCC_APB2_GPIOD  = (1UL << 5),  // GPIOD 时钟使能位
    RCC_APB2_GPIOE  = (1UL << 6),  // GPIOE 时钟使能位
    RCC_APB2_ADC1   = (1UL << 9),  // ADC1 时钟使能位
    RCC_APB2_ADC2   = (1UL << 10), // ADC2 时钟使能位
    RCC_APB2_TIM1   = (1UL << 11), // TIM1 时钟使能位
    RCC_APB2_SPI1   = (1UL << 12), // SPI1 时钟使能位
    RCC_APB2_USART1 = (1UL << 14), // USART1 时钟使能位
} dri_ll_rcc_apb2enr_bits;

/* ---------- 参数 ---------- */

/* 时钟源定义 */
typedef enum
{
    RCC_SYSCLK_SOURCE_HSI = 0x00UL, // HSI 作为系统时钟
    RCC_SYSCLK_SOURCE_HSE = 0x01UL, // HSE 作为系统时钟
    RCC_SYSCLK_SOURCE_PLL = 0x02UL, // PLL 作为系统时钟
} dri_ll_rcc_sysclk_source;

/* 时钟源状态系数 */
typedef enum
{
    RCC_SYSCLK_STATUS_HSI   = 0x00UL, // HSI 作为系统时钟
    RCC_SYSCLK_STATUS_HSE   = 0x01UL, // HSE 作为系统时钟
    RCC_SYSCLK_STATUS_PLL   = 0x02UL, // PLL 作为系统时钟
    RCC_SYSCLK_STATUS_NOALL = 0x03UL, // 不允许的状态
} dri_ll_rcc_sysclk_status;

/* AHB分频系数（HPRE 字段原始值） */
typedef enum
{
    RCC_AHB_PRESCALER_DIV1   = 0x00UL, // HCLK = SYSCLK
    RCC_AHB_PRESCALER_DIV2   = 0x08UL, // HCLK = SYSCLK / 2
    RCC_AHB_PRESCALER_DIV4   = 0x09UL, // HCLK = SYSCLK / 4
    RCC_AHB_PRESCALER_DIV8   = 0x0AUL, // HCLK = SYSCLK / 8
    RCC_AHB_PRESCALER_DIV16  = 0x0BUL, // HCLK = SYSCLK / 16
    RCC_AHB_PRESCALER_DIV64  = 0x0CUL, // HCLK = SYSCLK / 64
    RCC_AHB_PRESCALER_DIV128 = 0x0DUL, // HCLK = SYSCLK / 128
    RCC_AHB_PRESCALER_DIV256 = 0x0EUL, // HCLK = SYSCLK / 256
    RCC_AHB_PRESCALER_DIV512 = 0x0FUL, // HCLK = SYSCLK / 512
} dri_ll_rcc_ahb_prescaler;

/* APB分频系数（PPRE 字段原始值） */
typedef enum
{
    RCC_APB_PRESCALER_DIV1  = 0x00UL, // PCLK = HCLK
    RCC_APB_PRESCALER_DIV2  = 0x04UL, // PCLK = HCLK / 2
    RCC_APB_PRESCALER_DIV4  = 0x05UL, // PCLK = HCLK / 4
    RCC_APB_PRESCALER_DIV8  = 0x06UL, // PCLK = HCLK / 8
    RCC_APB_PRESCALER_DIV16 = 0x07UL, // PCLK = HCLK / 16
} dri_ll_rcc_apb_prescaler;

/* PLL时钟源系数 */
typedef enum
{
    RCC_PLL_SOURCE_HSI_DIV2 = 0x00UL, // (HSI / 2) 作为 PLL 时钟源
    RCC_PLL_SOURCE_HSE      = 0x01UL, // HSE 作为 PLL 时钟源
} dri_ll_rcc_pll_source;

/* PLL倍频系数 */
typedef enum
{
    RCC_PLL_MUL_2  = 0x00UL, // 2倍频
    RCC_PLL_MUL_3  = 0x01UL, // 3倍频
    RCC_PLL_MUL_4  = 0x02UL, // 4倍频
    RCC_PLL_MUL_5  = 0x03UL, // 5倍频
    RCC_PLL_MUL_6  = 0x04UL, // 6倍频
    RCC_PLL_MUL_7  = 0x05UL, // 7倍频
    RCC_PLL_MUL_8  = 0x06UL, // 8倍频
    RCC_PLL_MUL_9  = 0x07UL, // 9倍频
    RCC_PLL_MUL_10 = 0x08UL, // 10倍频
    RCC_PLL_MUL_11 = 0x09UL, // 11倍频
    RCC_PLL_MUL_12 = 0x0AUL, // 12倍频
    RCC_PLL_MUL_13 = 0x0BUL, // 13倍频
    RCC_PLL_MUL_14 = 0x0CUL, // 14倍频
    RCC_PLL_MUL_15 = 0x0DUL, // 15倍频
    RCC_PLL_MUL_16 = 0x0EUL, // 16倍频
} dri_ll_rcc_pll_mul;

typedef enum
{
    RCC_PLL_HSE_DIV_1 = 0x00UL, // HSE 不分频
    RCC_PLL_HSE_DIV_2 = 0x01UL, // HSE 分频 2
} dri_ll_rcc_pll_hse_div;

/* 字段位移系数 */
typedef enum
{
    RCC_CFGR_SWS_POS    = 2,  // SWS 位位置
    RCC_CFGR_HPRE_POS   = 4,  // HPRE 位位置
    RCC_CFGR_PPRE1_POS  = 8,  // PPRE1 位位置
    RCC_CFGR_PPRE2_POS  = 11, // PPRE2 位位置
    RCC_CFGR_PLLSRC_POS = 16, // PLLSRC 位位置
    RCC_CFGR_PLLMUL_POS = 18, // PLLMUL 位位置
} dri_ll_rcc_field_pos;

/* 读取位掩码 */
typedef enum
{
    RCC_CFGR_SWS_MSK = (0x3 << RCC_CFGR_SWS_POS), // SWS 位掩码
} dri_ll_rcc_read_msk;

/* ========== 对外接口 ========== */

/* ---------- 时钟源控制 ---------- */

/* HSI 控制函数 */
void    dri_ll_rcc_hsi_enable(void);
void    dri_ll_rcc_hsi_disable(void);
isREADY dri_ll_rcc_hsi_is_ready(void);

/* HSE 控制函数 */
void    dri_ll_rcc_hse_enable(void);
void    dri_ll_rcc_hse_disable(void);
isREADY dri_ll_rcc_hse_is_ready(void);

/* PLL 控制函数 */
void    dri_ll_rcc_pll_enable(void);
void    dri_ll_rcc_pll_disable(void);
isREADY dri_ll_rcc_pll_is_ready(void);

/* HSE 过渡控制函数 */
void dri_ll_rcc_hse_bypass_enable(void);
void dri_ll_rcc_hse_bypass_disable(void);

/* 系统时钟选择与预分频 */
void                     dri_ll_rcc_sysclk_select(dri_ll_rcc_sysclk_source sysclk_source);
void                     dri_ll_rcc_ahb_prescaler_set(dri_ll_rcc_ahb_prescaler ahb_prescaler);
void                     dri_ll_rcc_apb1_prescaler_set(dri_ll_rcc_apb_prescaler apb1_prescaler);
void                     dri_ll_rcc_apb2_prescaler_set(dri_ll_rcc_apb_prescaler apb2_prescaler);
dri_ll_rcc_sysclk_source dri_ll_rcc_sysclk_source_get(void);
dri_ll_rcc_sysclk_status dri_ll_rcc_sysclk_status_get(void);

/* ---------- PLL 配置 ---------- */
void                  dri_ll_rcc_pll_source_set(dri_ll_rcc_pll_source source);
void                  dri_ll_rcc_pll_mul_set(dri_ll_rcc_pll_mul value);
dri_ll_rcc_pll_source dri_ll_rcc_pll_source_get(void);
dri_ll_rcc_pll_mul    dri_ll_rcc_pll_mul_get(void);
void                  dri_ll_rcc_pll_hse_div_set(dri_ll_rcc_pll_hse_div div);

/* ---------- 外设时钟门控 ---------- */

void     dri_ll_rcc_ahb_enable(dri_ll_rcc_ahbenr_bits mask);
void     dri_ll_rcc_ahb_disable(dri_ll_rcc_ahbenr_bits mask);
isENABLE dri_ll_rcc_ahb_is_enabled(dri_ll_rcc_ahbenr_bits mask);

void     dri_ll_rcc_apb1_enable(dri_ll_rcc_apb1enr_bits mask);
void     dri_ll_rcc_apb1_disable(dri_ll_rcc_apb1enr_bits mask);
isENABLE dri_ll_rcc_apb1_is_enabled(dri_ll_rcc_apb1enr_bits mask);

void     dri_ll_rcc_apb2_enable(dri_ll_rcc_apb2enr_bits mask);
void     dri_ll_rcc_apb2_disable(dri_ll_rcc_apb2enr_bits mask);
isENABLE dri_ll_rcc_apb2_is_enabled(dri_ll_rcc_apb2enr_bits mask);

#endif /* __DRI_LL_RCC_H__ */
