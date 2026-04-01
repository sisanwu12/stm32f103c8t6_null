/**
 * @file dri_ll.h
 * @author sisanwu12
 * @brief 定义寄存器基础操作
 * @version 0.1
 * @date 2026-03-31
 *
 */

#ifndef __DRI_LL_H__
#define __DRI_LL_H__

#include "data_type.h"

/* ========== 基础操作 ==========*/

/* 写寄存器函数 */
static inline void dri_ll_write_reg(uptr base_addr, uptr offset, u32 value)
{
    *(volatile u32*)(base_addr + offset) = value;
}

/* 读寄存器函数 */
static inline u32 dri_ll_read_reg(uptr base_addr, uptr offset)
{
    volatile u32* reg_addr = (volatile u32*)(base_addr + offset);
    return *reg_addr;
}

/* 设置寄存器位函数 */
static inline void dri_ll_set_bits(uptr base_addr, uptr offset, u32 bits)
{
    volatile u32* reg_addr = (volatile u32*)(base_addr + offset);
    *reg_addr |= bits;
}

/* 清除寄存器位函数 */
static inline void dri_ll_clear_bits(uptr base_addr, uptr offset, u32 bits)
{
    volatile u32* reg_addr = (volatile u32*)(base_addr + offset);
    *reg_addr &= ~bits;
}

/* 修改寄存器函数 */
static inline void dri_ll_modify_reg(uptr base_addr, uptr offset, u32 clear_mask, u32 set_mask)
{
    volatile u32* reg_addr = (volatile u32*)(base_addr + offset);
    *reg_addr              = (*reg_addr & ~clear_mask) | set_mask;
}

#endif /* __DRI_LL_H__ */