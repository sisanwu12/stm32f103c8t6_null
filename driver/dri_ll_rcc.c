/**
 * @file dri_ll_rcc.c
 * @author sisanwu12
 * @brief
 * @version 0.1
 * @date 2026-03-31
 *
 */

#define __USE_RCC_OFFSET

#include "dri_ll_rcc.h"
#include "dri_ll.h"

/* ========== 时钟源控制 ========== */

/* HSI 控制函数 */
void dri_ll_rcc_hsi_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, CR_HSION); // 设置 HSION 位
}
void dri_ll_rcc_hsi_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, CR_HSION); // 清除 HSION 位
}
isREADY dri_ll_rcc_hsi_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & CR_HSIRDY) !=
           0; // 检查 HSIRDY 位
}

/* HSE 控制函数 */
void dri_ll_rcc_hse_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, CR_HSEON); // 设置 HSEON 位
}
void dri_ll_rcc_hse_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, CR_HSEON); // 清除 HSEON 位
}
isREADY dri_ll_rcc_hse_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & CR_HSERDY) !=
           0; // 检查 HSERDY 位
}

/* PLL 控制函数 */
void dri_ll_rcc_pll_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, CR_PLLON); // 设置 PLLON 位

    while (!dri_ll_rcc_pll_is_ready())
    {
        // 等待 PLL 稳定
    }
}
void dri_ll_rcc_pll_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, CR_PLLON); // 清除 PLLON 位

    while (dri_ll_rcc_pll_is_ready())
    {
        // 等待 PLL 关闭
    }
}
isREADY dri_ll_rcc_pll_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & CR_PLLRDY) !=
           0; // 检查 PLLRDY 位
}

/* HSE 过渡控制函数 */
void dri_ll_rcc_hse_bypass_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, CR_HSEBYP); // 设置 HSEBYP 位
}
void dri_ll_rcc_hse_bypass_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, CR_HSEBYP); // 清除 HSEBYP 位
}

/* ========== 系统时钟选择与预分频 ========== */

/* 系统时钟选择 */
void dri_ll_rcc_sysclk_select(u32 sysclk_source)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, CFGR_SWS0 | CFGR_SWS1,
                      sysclk_source & CFGR_SW0); // 设置 SW 位
}

/* AHB 和 APB 预分频设置 */
void dri_ll_rcc_ahb_prescaler_set(u32 ahb_prescaler)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, 0x000000F0UL,
                      ahb_prescaler & 0x000000F0UL); // 设置 HPRE 位
}

/* APB1 预分频设置 */
void dri_ll_rcc_apb1_prescaler_set(u32 apb1_prescaler)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, 0x00001C00UL,
                      apb1_prescaler & 0x00001C00UL); // 设置 PPRE1 位
}

/* APB2 预分频设置 */
void dri_ll_rcc_apb2_prescaler_set(u32 apb2_prescaler)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, 0x00003800UL,
                      apb2_prescaler & 0x00003800UL); // 设置 PPRE2 位
}

/* 获取系统时钟源 */
u32 dri_ll_rcc_sysclk_source_get(void)
{
    return dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET) &
           0x00000003UL; // 获取 SW 位
}

/* 获取系统时钟状态 */
u32 dri_ll_rcc_sysclk_status_get(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET) >> 2) &
           0x00000003UL; // 获取并对齐 SWS 位
}

/* ========== PLL 配置 ========== */

/* 设置 PLL 时钟源 */
void dri_ll_rcc_pll_source_set(u32 source)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, 0x00010000UL,
                      source & 0x00010000UL); // 设置 PLLSRC 位
}

/* 设置 PLL 倍频 */
void dri_ll_rcc_pll_mul_set(u32 value)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, 0x003C0000UL,
                      value & 0x003C0000UL); // 设置 PLLMUL 位
}

/* 获取 PLL 时钟源 */
u32 dri_ll_rcc_pll_source_get(void)
{
    return dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET) &
           0x00010000UL; // 获取 PLLSRC 位
}

/* 获取 PLL 倍频 */
u32 dri_ll_rcc_pll_mul_get(void)
{
    return dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET) &
           0x003C0000UL; // 获取 PLLMUL 位
}

/* ========== 外设时钟门控 ========== */

void dri_ll_rcc_ahb_enable(dri_ll_rcc_ahbenr_bits mask)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_AHBENR_OFFSET, mask);
}
void dri_ll_rcc_ahb_disable(dri_ll_rcc_ahbenr_bits mask)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_AHBENR_OFFSET, mask);
}
void dri_ll_rcc_apb1_enable(dri_ll_rcc_apb1enr_bits mask)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB1ENR_OFFSET, mask);
}
void dri_ll_rcc_apb1_disable(dri_ll_rcc_apb1enr_bits mask)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB1ENR_OFFSET, mask);
}
void dri_ll_rcc_apb2_enable(dri_ll_rcc_apb2enr_bits mask)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB2ENR_OFFSET, mask);
}
void dri_ll_rcc_apb2_disable(dri_ll_rcc_apb2enr_bits mask)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB2ENR_OFFSET, mask);
}
isENABLE dri_ll_rcc_ahb_is_enabled(dri_ll_rcc_ahbenr_bits mask)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_AHBENR_OFFSET) & mask) != 0;
}
isENABLE dri_ll_rcc_apb1_is_enabled(dri_ll_rcc_apb1enr_bits mask)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB1ENR_OFFSET) & mask) != 0;
}
isENABLE dri_ll_rcc_apb2_is_enabled(dri_ll_rcc_apb2enr_bits mask)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB2ENR_OFFSET) & mask) != 0;
}
