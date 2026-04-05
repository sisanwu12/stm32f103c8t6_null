/**
 * @file dri_ll_rcc.c
 * @author sisanwu12
 * @brief
 * @version 0.1
 * @date 2026-03-31
 *
 */

#include "dri_ll_rcc.h"
#include "dri_ll.h"

/* ========== 时钟源控制 ========== */

/* HSI 控制函数 */
void dri_ll_rcc_hsi_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSION); // 设置 HSION 位
}
void dri_ll_rcc_hsi_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSION); // 清除 HSION 位
}
isREADY dri_ll_rcc_hsi_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & RCC_CR_HSIRDY) !=
           0; // 检查 HSIRDY 位
}

/* HSE 控制函数 */
void dri_ll_rcc_hse_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSEON); // 设置 HSEON 位
}
void dri_ll_rcc_hse_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSEON); // 清除 HSEON 位
}
isREADY dri_ll_rcc_hse_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & RCC_CR_HSERDY) !=
           0; // 检查 HSERDY 位
}

/* PLL 控制函数 */
void dri_ll_rcc_pll_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_PLLON); // 设置 PLLON 位

    while (!dri_ll_rcc_pll_is_ready())
    {
        // 等待 PLL 稳定
    }
}
void dri_ll_rcc_pll_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_PLLON); // 清除 PLLON 位

    while (dri_ll_rcc_pll_is_ready())
    {
        // 等待 PLL 关闭
    }
}
isREADY dri_ll_rcc_pll_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & RCC_CR_PLLRDY) !=
           0; // 检查 PLLRDY 位
}

/* HSE 过渡控制函数 */
void dri_ll_rcc_hse_bypass_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSEBYP); // 设置 HSEBYP 位
}
void dri_ll_rcc_hse_bypass_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSEBYP); // 清除 HSEBYP 位
}

/* ========== 系统时钟选择与预分频 ========== */

/* 系统时钟选择 */

/**
 * @brief 选择系统时钟源
 *
 * @param sysclk_source 系统时钟源
 *        0x00000000UL: HSI 作为系统时钟
 *        0x00000001UL: HSE 作为系统时钟
 *        0x00000002UL: PLL 作为系统时钟
 */
void dri_ll_rcc_sysclk_select(dri_ll_rcc_sysclk_source sysclk_source)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_SW,
                      (sysclk_source & RCC_CFGR_SW)); // 设置 SW 位
}

/* AHB 和 APB 预分频设置 */
void dri_ll_rcc_ahb_prescaler_set(dri_ll_rcc_ahb_prescaler ahb_prescaler)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_HPRE,
                      (((u32)ahb_prescaler << RCC_CFGR_HPRE_POS) & RCC_CFGR_HPRE)); // 设置 HPRE 位
}

/* APB1 预分频设置 */
void dri_ll_rcc_apb1_prescaler_set(dri_ll_rcc_apb_prescaler apb1_prescaler)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_PPRE1,
                      (((u32)apb1_prescaler << RCC_CFGR_PPRE1_POS) &
                       RCC_CFGR_PPRE1)); // 设置 PPRE1 位
}

/* APB2 预分频设置 */
void dri_ll_rcc_apb2_prescaler_set(dri_ll_rcc_apb_prescaler apb2_prescaler)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_PPRE2,
                      (((u32)apb2_prescaler << RCC_CFGR_PPRE2_POS) &
                       RCC_CFGR_PPRE2)); // 设置 PPRE2 位
}

/* 获取系统时钟源 */
dri_ll_rcc_sysclk_source dri_ll_rcc_sysclk_source_get(void)
{
    return (
        dri_ll_rcc_sysclk_source)(dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET) &
                                  RCC_CFGR_SW); // 获取 SW 位
}

/* 获取系统时钟状态 */
dri_ll_rcc_sysclk_status dri_ll_rcc_sysclk_status_get(void)
{
    return (dri_ll_rcc_sysclk_status)dri_ll_read_field(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET,
                                                       RCC_CFGR_SWS_MSK, RCC_CFGR_SWS_POS);
}

/* ========== PLL 配置 ========== */

/**
 * @brief 设置 PLL 时钟源
 *
 * @param source
 *       0x00000000UL: (HSI / 2) 作为 PLL 时钟源
 *       0x00000001UL: HSE 作为 PLL 时钟源
 * @note 该函数会修改 CFGR 寄存器中的 PLLSRC 位,
 *      PLLSRC 位 仅能在 PLL 关闭时修改, 因此调用该函数前请确保 PLL 已关闭
 */
void dri_ll_rcc_pll_source_set(dri_ll_rcc_pll_source source)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_PLLSRC,
                      (((u32)source << RCC_CFGR_PLLSRC_POS) &
                       RCC_CFGR_PLLSRC)); // 设置 PLLSRC 位
}

/* 设置 PLL 倍频 */
void dri_ll_rcc_pll_mul_set(dri_ll_rcc_pll_mul value)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_PLLMUL,
                      (((u32)value << RCC_CFGR_PLLMUL_POS) & RCC_CFGR_PLLMUL)); // 设置 PLLMUL 位
}

/* 获取 PLL 时钟源 */
dri_ll_rcc_pll_source dri_ll_rcc_pll_source_get(void)
{
    return (dri_ll_rcc_pll_source)dri_ll_read_field(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET,
                                                    RCC_CFGR_PLLSRC, RCC_CFGR_PLLSRC_POS);
}

/* 获取 PLL 倍频 */
dri_ll_rcc_pll_mul dri_ll_rcc_pll_mul_get(void)
{
    return (dri_ll_rcc_pll_mul)dri_ll_read_field(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET,
                                                 RCC_CFGR_PLLMUL, RCC_CFGR_PLLMUL_POS);
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
