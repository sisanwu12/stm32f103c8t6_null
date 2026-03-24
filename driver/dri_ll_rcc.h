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

#ifdef __cplusplus // 兼容 C++ 编译器
extern "C"
{      // 使用 C 链接方式导出接口
#endif // 结束 C++ 兼容判断

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

#endif /* __DRI_LL_RCC_H__ */