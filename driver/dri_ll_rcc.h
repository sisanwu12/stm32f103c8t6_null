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

/* ========== RCC 地址定义层 ========== */

#define __USE_RCC_OFFSET

#ifdef __USE_RCC_OFFSET

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

/* =========== 寄存器位定义 =========== */

/* cr寄存器位定义 */
typedef enum
{
    CR_HSION   = (1UL << 0),  // HSI 时钟使能位
    CR_HSIRDY  = (1UL << 1),  // HSI 就绪位
    CR_HSITRI0 = (1UL << 3),  // HSI 调谐位 0
    CR_HSITRI1 = (1UL << 4),  // HSI 调谐位 1
    CR_HSITRI2 = (1UL << 5),  // HSI 调谐位 2
    CR_HSITRI3 = (1UL << 6),  // HSI 调谐位 3
    CR_HSITRI4 = (1UL << 7),  // HSI 调谐位 4
    CR_HSICAL0 = (1UL << 8),  // HSI 校准位 0
    CR_HSICAL1 = (1UL << 9),  // HSI 校准位 1
    CR_HSICAL2 = (1UL << 10), // HSI 校准位 2
    CR_HSICAL3 = (1UL << 11), // HSI 校准位 3
    CR_HSICAL4 = (1UL << 12), // HSI 校准位 4
    CR_HSEON   = (1UL << 16), // HSE 时钟使能位
    CR_HSERDY  = (1UL << 17), // HSE 就绪位
    CR_HSEBYP  = (1UL << 18), // HSE 过渡使能位
    CR_CSSON   = (1UL << 19), // 时钟安全系统使能位
    CR_PLLON   = (1UL << 24), // PLL 时钟使能位
    CR_PLLRDY  = (1UL << 25), // PLL 就绪位
    CR_PLL2ON  = (1UL << 26), // PLL2 时钟使能位
    CR_PLL2RDY = (1UL << 27), // PLL2 就绪位
    CR_PLL3ON  = (1UL << 28), // PLL3 时钟使能位
    CR_PLL3RDY = (1UL << 29), // PLL3 就绪位
} dri_ll_rcc_cr_bits;

/* cfgr寄存器位定义 */
typedef enum
{
    CFGR_SW0      = (1UL << 0),  // 系统时钟选择位 0
    CFGR_SW1      = (1UL << 1),  // 系统时钟选择位 1
    CFGR_SWS0     = (1UL << 2),  // 系统时钟状态位 0
    CFGR_SWS1     = (1UL << 3),  // 系统时钟状态位 1
    CFGR_HPRE0    = (1UL << 4),  // AHB 预分频位 0
    CFGR_HPRE1    = (1UL << 5),  // AHB 预分频位 1
    CFGR_HPRE2    = (1UL << 6),  // AHB 预分频位 2
    CFGR_HPRE3    = (1UL << 7),  // AHB 预分频位 3
    CFGR_PPRE10   = (1UL << 8),  // APB1 预分频位 0
    CFGR_PPRE11   = (1UL << 9),  // APB1 预分频位 1
    CFGR_PPRE12   = (1UL << 10), // APB1 预分频位 2
    CFGR_PPRE20   = (1UL << 11), // APB2 预分频位 0
    CFGR_PPRE21   = (1UL << 12), // APB2 预分频位 1
    CFGR_PPRE22   = (1UL << 13), // APB2 预分频位 2
    CFGR_ADCPRE0  = (1UL << 14), // ADC 预分频位 0
    CFGR_ADCPRE1  = (1UL << 15), // ADC 预分频位 1
    CFGR_PLLSRC   = (1UL << 16), // PLL 时钟源选择位
    CFGR_PLLXTPRE = (1UL << 17), // PLL HSE 分频位
    CFGR_PLLMUL0  = (1UL << 18), // PLL 倍频位 0
    CFGR_PLLMUL1  = (1UL << 19), // PLL 倍频位 1
    CFGR_PLLMUL2  = (1UL << 20), // PLL 倍频位 2
    CFGR_PLLMUL3  = (1UL << 21), // PLL 倍频位 3
    CFGR_OTGFSPRE = (1UL << 22), // OTG FS 预分频位 0
    CFGR_MCO0     = (1UL << 24), // MCO 输出位 0
    CFGR_MCO1     = (1UL << 25), // MCO 输出位 1
    CFGR_MCO2     = (1UL << 26), // MCO 输出位 2
} dri_ll_rcc_cfgr_bits;

/* ahb 高速时钟外设 */
typedef enum
{
    AHB_DMA1  = (1UL << 0),  // DMA1 时钟使能位
    AHB_DMA2  = (1UL << 1),  // DMA2 时钟使能位
    AHB_SRAM  = (1UL << 2),  // SRAM 时钟使能位
    AHB_FLITF = (1UL << 4),  // FLITF 时钟使能位
    AHB_CRC   = (1UL << 6),  // CRC 时钟使能位
    AHB_FSMC  = (1UL << 8),  // FSMC 时钟使能位
    AHB_SDIO  = (1UL << 10), // SDIO 时钟使能位
} dri_ll_rcc_ahbenr_bits;

/* apb1 低速外设 */
typedef enum
{
    APB1_TIM2   = (1UL << 0),  // TIM2 时钟使能位
    APB1_TIM3   = (1UL << 1),  // TIM3 时钟使能位
    APB1_TIM4   = (1UL << 2),  // TIM4 时钟使能位
    APB1_WWDG   = (1UL << 11), // WWDG 时钟使能位
    APB1_SPI2   = (1UL << 14), // SPI2 时钟使能位
    APB1_SPI3   = (1UL << 15), // SPI3 时钟使能位
    APB1_USART2 = (1UL << 17), // USART2 时钟使能位
    APB1_USART3 = (1UL << 18), // USART3 时钟使能位
    APB1_I2C1   = (1UL << 21), // I2C1 时钟使能位
    APB1_I2C2   = (1UL << 22), // I2C2 时钟使能位
    APB1_USB    = (1UL << 23), // USB 时钟使能位
    APB1_CAN    = (1UL << 25), // CAN 时钟使能位
    APB1_BKP    = (1UL << 27), // BKP 时钟使能位
    APB1_PWR    = (1UL << 28), // PWR 时钟使能位
    APB1_DAC    = (1UL << 29), // DAC 时钟使能位
} dri_ll_rcc_apb1enr_bits;

/* apb2 高速外设 */
typedef enum
{
    APB2_AFIO   = (1UL << 0),  // AFIO 时钟使能位
    APB2_GPIOA  = (1UL << 2),  // GPIOA 时钟使能位
    APB2_GPIOB  = (1UL << 3),  // GPIOB 时钟使能位
    APB2_GPIOC  = (1UL << 4),  // GPIOC 时钟使能位
    APB2_GPIOD  = (1UL << 5),  // GPIOD 时钟使能位
    APB2_GPIOE  = (1UL << 6),  // GPIOE 时钟使能位
    APB2_ADC1   = (1UL << 9),  // ADC1 时钟使能位
    APB2_ADC2   = (1UL << 10), // ADC2 时钟使能位
    APB2_TIM1   = (1UL << 11), // TIM1 时钟使能位
    APB2_SPI1   = (1UL << 12), // SPI1 时钟使能位
    APB2_USART1 = (1UL << 14), // USART1 时钟使能位
} dri_ll_rcc_apb2enr_bits;

#endif /* __USE_RCC_OFFSET */

/* ========== 时钟源控制 ========== */

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
void dri_ll_rcc_sysclk_select(u32 sysclk_source);
void dri_ll_rcc_ahb_prescaler_set(u32 ahb_prescaler);
void dri_ll_rcc_apb1_prescaler_set(u32 apb1_prescaler);
void dri_ll_rcc_apb2_prescaler_set(u32 apb2_prescaler);
u32  dri_ll_rcc_sysclk_source_get(void);
u32  dri_ll_rcc_sysclk_status_get(void);

/* ========== PLL 配置 ========== */
void dri_ll_rcc_pll_source_set(u32 source);
void dri_ll_rcc_pll_mul_set(u32 value);
u32  dri_ll_rcc_pll_source_get(void);
u32  dri_ll_rcc_pll_mul_get(void);

/* ========== 外设时钟门控 ========== */

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
