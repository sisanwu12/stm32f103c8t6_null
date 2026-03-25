#include "dri_ll_rcc.h"

/* ========== RCC 对外接口实现 ========== */

/* ========== RCC_CR 相关函数 start */

/* PLL 使能函数 */
void dri_ll_rcc_pll_enable(void)
{
    /* 设置 RCC_CR 寄存器的 PLLON 位 (第24位) 来使能 PLL */
    volatile uptr* rcc_cr = (volatile uptr*)(DRI_LL_RCC_BASE_ADDR + DRI_LL_RCC_CR_OFFSET);
    *rcc_cr |= (1U << 24); // 置位 PLLON 位
}

/* PLL 就绪状态查询函数 */
bool dri_ll_rcc_pll_is_ready(void)
{
    /* 查询 RCC_CR 寄存器的 PLLRDY 位 (第25位) 来判断 PLL 是否就绪 */
    volatile uptr* rcc_cr = (volatile uptr*)(DRI_LL_RCC_BASE_ADDR + DRI_LL_RCC_CR_OFFSET);
    return (*rcc_cr & (1U << 25)) != 0; // 返回 PLLRDY 位的值
}

/* HSI 使能函数 */
void dri_ll_rcc_hsi_enable(void)
{
    /* 设置 RCC_CR 寄存器的 HSION 位 (第0位) 来使能 HSI */
    volatile uptr* rcc_cr = (volatile uptr*)(DRI_LL_RCC_BASE_ADDR + DRI_LL_RCC_CR_OFFSET);
    *rcc_cr |= (1U << 0); // 置位 HSION 位
}

/* HSI 就绪状态查询函数 */
bool dri_ll_rcc_hsi_is_ready(void)
{
    /* 查询 RCC_CR 寄存器的 HSIRDY 位 (第1位) 来判断 HSI 是否就绪 */
    volatile uptr* rcc_cr = (volatile uptr*)(DRI_LL_RCC_BASE_ADDR + DRI_LL_RCC_CR_OFFSET);
    return (*rcc_cr & (1U << 1)) != 0; // 返回 HSIRDY 位的值
}

/* HSE 使能函数 */
void dri_ll_rcc_hse_enable(void)
{
    /* 设置 RCC_CR 寄存器的 HSEON 位 (第16位) 来使能 HSE */
    volatile uptr* rcc_cr = (volatile uptr*)(DRI_LL_RCC_BASE_ADDR + DRI_LL_RCC_CR_OFFSET);
    *rcc_cr |= (1U << 16); // 置位 HSEON 位
}

/* HSE 就绪状态查询函数 */
bool dri_ll_rcc_hse_is_ready(void)
{
    /* 查询 RCC_CR 寄存器的 HSERDY 位 (第17位) 来判断 HSE 是否就绪 */
    volatile uptr* rcc_cr = (volatile uptr*)(DRI_LL_RCC_BASE_ADDR + DRI_LL_RCC_CR_OFFSET);
    return (*rcc_cr & (1U << 17)) != 0; // 返回 HSERDY 位的值
}

/* 设置 HSE 旁路模式函数 */
bool dri_ll_rcc_set_hsebyp(bool hse_byp)
{
    /* 设置 RCC_CR 寄存器的 HSEBYP 位 (第18位) 来配置 HSE 的旁路模式 */
    volatile uptr* rcc_cr = (volatile uptr*)(DRI_LL_RCC_BASE_ADDR + DRI_LL_RCC_CR_OFFSET);
    if (hse_byp)
    {
        *rcc_cr |= (1U << 18); // 置位 HSEBYP 位，启用旁路模式
    }
    else
    {
        *rcc_cr &= ~(1U << 18); // 清除 HSEBYP 位，使用晶振模式
    }
    return true; // 返回设置成功
}

/* RCC_CR 相关函数 end ========== */

/* ========== RCC_CFGR 相关函数 start */

/* RCC_CFGR 相关函数 end ========== */

/* ========== RCC_CIR 相关函数 start */

/* RCC_CIR 相关函数 end ========== */

/* ========== RCC_APB2RSTR 相关函数 start */

/* RCC_APB2RSTR 相关函数 end ========== */

/* ========== RCC_APB1RSTR 相关函数 start */

/* RCC_APB1RSTR 相关函数 end ========== */

/* ========== RCC_AHBENR 相关函数 start */

/* RCC_AHBENR 相关函数 end ========== */

/* ========== RCC_APB2ENR 相关函数 start */

/* RCC_APB2ENR 相关函数 end ========== */

/* ========== RCC_APB1ENR 相关函数 start */

/* RCC_APB1ENR 相关函数 end ========== */

/* ========== RCC_BDCR 相关函数 start */

/* RCC_BDCR 相关函数 end ========== */

/* ========== RCC_CSR 相关函数 start */

/* RCC_CSR 相关函数 end ========== */