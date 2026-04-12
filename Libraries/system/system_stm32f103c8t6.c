/**
 * @file system_stm32f103c8t6.c
 * @author sisanwu12
 * @brief
 * @version 0.1
 * @date 2026-04-04
 *
 */

#include "system_stm32f103c8t6.h"
#include "dri_ll_flash.h"
#include "dri_ll_rcc.h"

/**
 * @brief 系统初始化函数，配置系统时钟等相关设置
 *
 * @return system_init_status_t 初始化状态
 */
system_init_status_t SystemInit(void)
{
    dri_ll_flash_prefetch_enable();
    dri_ll_flash_latency_set(DRI_LL_FLASH_LATENCY_2);

    dri_ll_rcc_hse_bypass_disable();
    dri_ll_rcc_hse_enable();
    while (!dri_ll_rcc_hse_is_ready())
        ;

    dri_ll_rcc_ahb_prescaler_set(RCC_AHB_PRESCALER_DIV1);
    dri_ll_rcc_apb1_prescaler_set(RCC_APB_PRESCALER_DIV2);
    dri_ll_rcc_apb2_prescaler_set(RCC_APB_PRESCALER_DIV1);

    dri_ll_rcc_pll_disable();
    dri_ll_rcc_pll_source_set(RCC_PLL_SOURCE_HSE);
    dri_ll_rcc_pll_hse_div_set(RCC_PLL_HSE_DIV_1);
    dri_ll_rcc_pll_mul_set(RCC_PLL_MUL_9);
    dri_ll_rcc_pll_enable();

    dri_ll_rcc_sysclk_select(RCC_SYSCLK_SOURCE_PLL);
    while (dri_ll_rcc_sysclk_status_get() != RCC_SYSCLK_STATUS_PLL)
        ;

    return SYSTEM_INIT_SUCCESS;
}