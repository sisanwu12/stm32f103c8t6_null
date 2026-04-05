/**
 * @file dri_ll_flash.c
 * @author sisanwu12
 * @brief
 * @version 0.1
 * @date 2026-04-05
 *
 */

#include "dri_ll_flash.h"
#include "dri_ll.h"

/* ---------- FLASH操作函数 ---------- */

/**
 * @brief 使能 FLASH 预取指
 * @retval None
 */
void dri_ll_flash_prefetch_enable(void)
{
    dri_ll_set_bits(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_PRFTBE);
}

/**
 * @brief 禁用 FLASH 预取指
 * @retval None
 */
void dri_ll_flash_prefetch_disable(void)
{
    dri_ll_clear_bits(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_PRFTBE);
}

/**
 * @brief 使能 FLASH 半周期访问
 * @retval None
 */
void dri_ll_flash_halfcycle_enable(void)
{
    dri_ll_set_bits(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_HLFCYA);
}

/**
 * @brief 禁用 FLASH 半周期访问
 * @retval None
 */
void dri_ll_flash_halfcycle_disable(void)
{
    dri_ll_clear_bits(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_HLFCYA);
}

/**
 * @brief 检查 FLASH 半周期访问是否使能
 * @retval isENABLE 如果使能返回 true，否则返回 false
 */
isENABLE dri_ll_flash_halfcycle_is_enabled(void)
{
    return (dri_ll_read_reg(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET) & FLASH_ACR_HLFCYA) !=
           0U;
}

/**
 * @brief 设置 FLASH 延迟等级
 *
 * @param latency FLASH 延迟等级
 *        DRI_LL_FLASH_LATENCY_0: 0 等级 (0 < SYSCLK <= 24MHz)
 *        DRI_LL_FLASH_LATENCY_1: 1 等级 (24MHz < SYSCLK <= 48MHz)
 *        DRI_LL_FLASH_LATENCY_2: 2 等级 (48MHz < SYSCLK <= 72MHz)
 * @retval None
 */
void dri_ll_flash_latency_set(dri_ll_flash_latency latency)
{
    dri_ll_modify_reg(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET, FLASH_ACR_LATENCY,
                      ((u32)latency << FLASH_ACR_LATENCY_POS) & FLASH_ACR_LATENCY);
}

/**
 * @brief 获取当前 FLASH 延迟等级
 * @retval dri_ll_flash_latency 当前 FLASH 延迟等级
 */
dri_ll_flash_latency dri_ll_flash_latency_get(void)
{
    return (dri_ll_flash_latency)dri_ll_read_field(DRI_LL_FLASH_BASE_ADDR, DRI_LL_FLASH_ACR_OFFSET,
                                                   FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_POS);
}