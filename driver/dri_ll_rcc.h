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

/* ========== 对外接口 ========== */

/* ========== RCC_CR 相关函数 start */

/* PLL 使能函数 */
void dri_ll_rcc_pll_enable(void);
/* PLL 就绪状态查询函数 */
bool dri_ll_rcc_pll_is_ready(void);
/* PLL 初始化函数 */
void dri_ll_rcc_pll_init(bool pll_source, u8 pll_mul);

/* HSI 使能函数 */
void dri_ll_rcc_hsi_enable(void);
/* HSI 就绪状态查询函数 */
bool dri_ll_rcc_hsi_is_ready(void);
/* HSE 使能函数 */
void dri_ll_rcc_hse_enable(void);
/* HSE 就绪状态查询函数 */
bool dri_ll_rcc_hse_is_ready(void);

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

#endif /* __DRI_LL_RCC_H__ */
