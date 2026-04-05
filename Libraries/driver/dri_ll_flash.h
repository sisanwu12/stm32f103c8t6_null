/**
 * @file dri_ll_flash.h
 * @author sisanwu12 (sisanwu12@outlook.com)
 * @brief
 * @version 0.1
 * @date 2026-04-05
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef __DRI_LL_FLASH_H__
#define __DRI_LL_FLASH_H__

#include "data_type.h"

/* ========== 数据定义 ==========*/

/* ---------- FLASH 地址定义层 ---------- */

/* FLASH 基地址 */
#define FLASH_BASE_ADDR 0x40022000UL // FLASH寄存器基地址

/* FLASH 寄存器偏移地址*/
#define FLASH_ACR_OFFSET     0x00UL // 访问控制寄存器
#define FLASH_KEYR_OFFSET    0x04UL // 密钥寄存器
#define FLASH_OPTKEYR_OFFSET 0x08UL // 选项密钥寄存器
#define FLASH_SR_OFFSET      0x0CUL // 状态寄存器
#define FLASH_CR_OFFSET      0x10UL // 控制寄存器
#define FLASH_AR_OFFSET      0x14UL // 地址寄存器
#define FLASH_OBR_OFFSET     0x1CUL // 选项字节寄存器
#define FLASH_WRPR_OFFSET    0x20UL // 写保护寄存器

/* ---------- 寄存器位定义 ---------- */

/* acr寄存器位定义 */
typedef enum
{
    FLASH_ACR_LATENCY0 = (1UL << 0),                                // 延迟位 0
    FLASH_ACR_LATENCY1 = (1UL << 1),                                // 延迟位 1
    FLASH_ACR_LATENCY  = (FLASH_ACR_LATENCY0 | FLASH_ACR_LATENCY1), // 延迟位掩码

    FLASH_ACR_PRFTBE = (1UL << 4), // 预取缓冲使能位
    FLASH_ACR_PRFTBS = (1UL << 5), // 预取缓冲状态位

    FLASH_ACR_HLFCYA = (1UL << 8), // 半周期访问使能位
    FLASH_ACR_HLFCYS = (1UL << 9), // 半周期访问状态位
} dri_ll_flash_acr_bits;

/* ---------- 参数 ---------- */

/* FLASH延迟等级设置 */
typedef enum
{
    DRI_LL_FLASH_LATENCY_0 = 0x00UL, // 0 等级 (0 < SYSCLK <= 24MHz)
    DRI_LL_FLASH_LATENCY_1 = 0x01UL, // 1 等级 (24MHz < SYSCLK <= 48MHz)
    DRI_LL_FLASH_LATENCY_2 = 0x02UL, // 2 等级 (48MHz < SYSCLK <= 72MHz)
} dri_ll_flash_latency;

void     dri_ll_flash_prefetch_enable(void);
void     dri_ll_flash_prefetch_disable(void);
isENABLE dri_ll_flash_prefetch_is_enabled(void);

void     dri_ll_flash_halfcycle_enable(void);
void     dri_ll_flash_halfcycle_disable(void);
isENABLE dri_ll_flash_halfcycle_is_enabled(void);

void                 dri_ll_flash_latency_set(dri_ll_flash_latency latency);
dri_ll_flash_latency dri_ll_flash_latency_get(void);

#endif /* __DRI_LL_FLASH_H__ */