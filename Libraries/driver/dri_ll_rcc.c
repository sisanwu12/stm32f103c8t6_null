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

/**
 * @brief HSI 内部高速时钟控制使能
 *
 */
void dri_ll_rcc_hsi_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSION); // 设置 HSION 位
}

/**
 * @brief HSI 内部高速时钟控制禁能
 *
 */
void dri_ll_rcc_hsi_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSION); // 清除 HSION 位
}

/**
 * @brief HSI 内部高速时钟就绪状态检查
 *
 * @return isREADY
 *  READY: HSI 就绪
 *  NOT_READY: HSI 未就绪
 */
isREADY dri_ll_rcc_hsi_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & RCC_CR_HSIRDY) !=
           0; // 检查 HSIRDY 位
}

/**
 * @brief HSE 外部高速时钟控制使能
 *
 */
void dri_ll_rcc_hse_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSEON); // 设置 HSEON 位
}

/**
 * @brief HSE 外部高速时钟控制禁能
 *
 */
void dri_ll_rcc_hse_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSEON); // 清除 HSEON 位
}

/**
 * @brief HSE 外部高速时钟就绪状态检查
 *
 * @return isREADY
 *  READY: HSE 就绪
 *  NOT_READY: HSE 未就绪
 */
isREADY dri_ll_rcc_hse_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & RCC_CR_HSERDY) !=
           0; // 检查 HSERDY 位
}

/**
 * @brief PLL 锁相环控制使能
 *
 */
void dri_ll_rcc_pll_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_PLLON); // 设置 PLLON 位

    while (!dri_ll_rcc_pll_is_ready())
    {
        // 等待 PLL 稳定
    }
}

/**
 * @brief PLL 锁相环控制禁能
 *
 */
void dri_ll_rcc_pll_disable(void)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_PLLON); // 清除 PLLON 位

    while (dri_ll_rcc_pll_is_ready())
    {
        // 等待 PLL 关闭
    }
}

/**
 * @brief PLL 锁相环就绪状态检查
 *
 * @return isREADY
 * READY: PLL 就绪
 * NOT_READY: PLL 未就绪
 */
isREADY dri_ll_rcc_pll_is_ready(void)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET) & RCC_CR_PLLRDY) !=
           0; // 检查 PLLRDY 位
}

/**
 * @brief HSE 外部高速时钟过渡使能
 *
 */
void dri_ll_rcc_hse_bypass_enable(void)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CR_OFFSET, RCC_CR_HSEBYP); // 设置 HSEBYP 位
}

/**
 * @brief HSE 外部高速时钟过渡禁能
 *
 */
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

/**
 * @brief AHB 总线预分频设置
 *
 * @param ahb_prescaler AHB 预分频系数
 */
void dri_ll_rcc_ahb_prescaler_set(dri_ll_rcc_ahb_prescaler ahb_prescaler)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_HPRE,
                      (((u32)ahb_prescaler << RCC_CFGR_HPRE_POS) & RCC_CFGR_HPRE)); // 设置 HPRE 位
}

/**
 * @brief APB1 总线预分频设置
 *
 * @param apb1_prescaler APB1 预分频系数
 */
void dri_ll_rcc_apb1_prescaler_set(dri_ll_rcc_apb_prescaler apb1_prescaler)
{
    dri_ll_modify_reg(
        DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_PPRE1,
        (((u32)apb1_prescaler << RCC_CFGR_PPRE1_POS) & RCC_CFGR_PPRE1)); // 设置 PPRE1 位
}

/**
 * @brief APB2 总线预分频设置
 *
 * @param apb2_prescaler APB2 预分频系数
 */
void dri_ll_rcc_apb2_prescaler_set(dri_ll_rcc_apb_prescaler apb2_prescaler)
{
    dri_ll_modify_reg(
        DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_PPRE2,
        (((u32)apb2_prescaler << RCC_CFGR_PPRE2_POS) & RCC_CFGR_PPRE2)); // 设置 PPRE2 位
}

/**
 * @brief 获取系统时钟源
 *
 * @return dri_ll_rcc_sysclk_source 系统时钟源
 */
dri_ll_rcc_sysclk_source dri_ll_rcc_sysclk_source_get(void)
{
    return (
        dri_ll_rcc_sysclk_source)(dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET) &
                                  RCC_CFGR_SW); // 获取 SW 位
}

/**
 * @brief 获取系统时钟状态
 *
 * @return dri_ll_rcc_sysclk_status 系统时钟状态
 */
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
                      (((u32)source << RCC_CFGR_PLLSRC_POS) & RCC_CFGR_PLLSRC)); // 设置 PLLSRC 位
}

/**
 * @brief 设置 PLL 锁相环倍频
 *
 * @param value PLL 锁相环倍频系数
 */
void dri_ll_rcc_pll_mul_set(dri_ll_rcc_pll_mul value)
{
    dri_ll_modify_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_PLLMUL,
                      (((u32)value << RCC_CFGR_PLLMUL_POS) & RCC_CFGR_PLLMUL)); // 设置 PLLMUL 位
}

/**
 * @brief 获取 PLL 时钟源
 *
 * @return dri_ll_rcc_pll_source PLL 时钟源
 */
dri_ll_rcc_pll_source dri_ll_rcc_pll_source_get(void)
{
    return (dri_ll_rcc_pll_source)dri_ll_read_field(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET,
                                                    RCC_CFGR_PLLSRC, RCC_CFGR_PLLSRC_POS);
}

/**
 * @brief 获取 PLL 倍频
 *
 * @return dri_ll_rcc_pll_mul PLL 倍频系数
 */
dri_ll_rcc_pll_mul dri_ll_rcc_pll_mul_get(void)
{
    return (dri_ll_rcc_pll_mul)dri_ll_read_field(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET,
                                                 RCC_CFGR_PLLMUL, RCC_CFGR_PLLMUL_POS);
}

/**
 * @brief 设置 PLL 锁相环时钟源为 HSE 外部高速时钟时的 HSE 外部高速时钟的分频系数
 *
 * @param div HSE 分频系数
 */
void dri_ll_rcc_pll_hse_div_set(dri_ll_rcc_pll_hse_div div)
{
    // HSE 分频设置仅在 PLL 时钟源为 HSE 时有效, 因此调用该函数前请确保 PLL 时钟源已设置为 HSE
    dri_ll_modify_reg(
        DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_CFGR_OFFSET, RCC_CFGR_PLLXTPRE,
        (((u32)div << RCC_CFGR_PLLXTPRE_POS) & RCC_CFGR_PLLXTPRE)); // 设置 PLLXTPRE 位中的分频部分
}

/* ========== 外设时钟门控 ========== */

/**
 * @brief 使能 AHB 外设时钟
 *
 * @param mask AHB 外设时钟使能位掩码
 */
void dri_ll_rcc_ahb_enable(dri_ll_rcc_ahbenr_bits mask)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_AHBENR_OFFSET, mask);
}

/**
 * @brief 禁能 AHB 外设时钟
 *
 * @param mask AHB 外设时钟使能位掩码
 */
void dri_ll_rcc_ahb_disable(dri_ll_rcc_ahbenr_bits mask)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_AHBENR_OFFSET, mask);
}

/**
 * @brief 使能 APB1 外设时钟
 *
 * @param mask APB1 外设时钟使能位掩码
 */
void dri_ll_rcc_apb1_enable(dri_ll_rcc_apb1enr_bits mask)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB1ENR_OFFSET, mask);
}

/**
 * @brief 禁能 APB1 外设时钟
 *
 * @param mask APB1 外设时钟使能位掩码
 */
void dri_ll_rcc_apb1_disable(dri_ll_rcc_apb1enr_bits mask)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB1ENR_OFFSET, mask);
}

/**
 * @brief 使能 APB2 外设时钟
 *
 * @param mask APB2 外设时钟使能位掩码
 */
void dri_ll_rcc_apb2_enable(dri_ll_rcc_apb2enr_bits mask)
{
    dri_ll_set_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB2ENR_OFFSET, mask);
}

/**
 * @brief 禁能 APB2 外设时钟
 *
 * @param mask APB2 外设时钟使能位掩码
 */
void dri_ll_rcc_apb2_disable(dri_ll_rcc_apb2enr_bits mask)
{
    dri_ll_clear_bits(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB2ENR_OFFSET, mask);
}

/**
 * @brief 检查 AHB 外设时钟是否使能
 *
 * @param mask AHB 外设时钟使能位掩码
 * @return isENABLE
 *  ENABLE: 对应位掩码的外设时钟使能
 *  DISABLE: 对应位掩码的外设时钟未使能
 */
isENABLE dri_ll_rcc_ahb_is_enabled(dri_ll_rcc_ahbenr_bits mask)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_AHBENR_OFFSET) & mask) != 0;
}

/**
 * @brief 检查 APB1 外设时钟是否使能
 *
 * @param mask APB1 外设时钟使能位掩码
 * @return isENABLE
 *  ENABLE: 对应位掩码的外设时钟使能
 *  DISABLE: 对应位掩码的外设时钟未使能
 */
isENABLE dri_ll_rcc_apb1_is_enabled(dri_ll_rcc_apb1enr_bits mask)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB1ENR_OFFSET) & mask) != 0;
}

/**
 * @brief 检查 APB2 外设时钟是否使能
 *
 * @param mask APB2 外设时钟使能位掩码
 * @return isENABLE
 *  ENABLE: 对应位掩码的外设时钟使能
 *  DISABLE: 对应位掩码的外设时钟未使能
 */
isENABLE dri_ll_rcc_apb2_is_enabled(dri_ll_rcc_apb2enr_bits mask)
{
    return (dri_ll_read_reg(DRI_LL_RCC_BASE_ADDR, DRI_LL_RCC_APB2ENR_OFFSET) & mask) != 0;
}
