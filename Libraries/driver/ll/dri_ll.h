/**
 * @file dri_ll.h
 * @author sisanwu12
 * @brief 定义寄存器基础操作
 * @note
 * - 该文件提供了对寄存器的基本读写、位操作和字段操作函数，供各个外设驱动使用。
 * 注意：set_bits / clear_bits / modify_reg 仅适用于普通 R/W 寄存器位，
 * 不适用于 W1C、只写、读后清零等特殊语义寄存器
 * @version 0.1
 * @date 2026-03-31
 *
 */

#ifndef __DRI_LL_H__
#define __DRI_LL_H__

#include "data_type.h"

/* ========== 数据定义 ========== */
typedef enum
{
    DRI_MASK_1  = 0b1U,        // 1 位掩码
    DRI_MASK_2  = 0b11U,       // 2 位掩码
    DRI_MASK_3  = 0b111U,      // 3 位掩码
    DRI_MASK_4  = 0xFU,        // 4 位掩码
    DRI_MASK_5  = 0b11111U,    // 5 位掩码
    DRI_MASK_6  = 0b111111U,   // 6 位掩码
    DRI_MASK_7  = 0b1111111U,  // 7 位掩码
    DRI_MASK_8  = 0xFFU,       // 8 位掩码
    DRI_MASK_16 = 0xFFFFU,     // 16 位掩码
    DRI_MASK_32 = 0xFFFFFFFFU, // 32 位掩码
} dri_ll_mask_t;               // 寄存器位掩码类型

/* ========== 基础操作 ==========*/

/* 写寄存器函数 */
static inline void dri_ll_write_reg(uptr base_addr, uptr offset, u32 value)
{
    volatile u32* reg_addr = (volatile u32*)(base_addr + offset);
    *reg_addr              = value;
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

/**
 * @brief 修改寄存器函数
 *
 * @param base_addr 寄存器基地址
 * @param offset 寄存器偏移地址
 * @param clear_mask 要清除的位掩码
 * @param set_mask 要设置的位掩码
 */
static inline void dri_ll_modify_reg(uptr base_addr, uptr offset, u32 clear_mask, u32 set_mask)
{
    volatile u32* reg_addr = (volatile u32*)(base_addr + offset);
    *reg_addr              = (*reg_addr & ~clear_mask) | set_mask;
}

/**
 * @brief 位状态检查函数
 *
 * @param base_addr 寄存器基地址
 * @param offset 寄存器偏移地址
 * @param bits 要检查的位掩码
 * @return bool 如果所有指定的位都被置位则返回 true，否则返回 false
 */
static inline bool dri_ll_is_bits_set(uptr base_addr, uptr offset, u32 bits)
{
    return (dri_ll_read_reg(base_addr, offset) & bits) == bits;
}

/**
 * @brief 位状态清除检查函数
 *
 * @param base_addr 寄存器基地址
 * @param offset 寄存器偏移地址
 * @param bits 要检查的位掩码
 * @return bool 如果所有指定的位都被清除则返回 true，否则返回 false
 */
static inline bool dri_ll_is_bits_clear(uptr base_addr, uptr offset, u32 bits)
{
    return (dri_ll_read_reg(base_addr, offset) & bits) == 0U;
}

/**
 * @brief 读取寄存器字段函数
 *
 * @param base_addr 寄存器基地址
 * @param offset 寄存器偏移地址
 * @param mask 字段掩码
 * @param pos 字段位置
 * @return u32 字段值
 */
static inline u32 dri_ll_read_field(uptr base_addr, uptr offset, u32 mask, u32 pos)
{
    return (dri_ll_read_reg(base_addr, offset) & mask) >> pos;
}

/**
 * @brief 等待寄存器位被置位函数
 *
 * @param base_addr 寄存器基地址
 * @param offset 寄存器偏移地址
 * @param bits 要等待的位掩码
 */
static inline void dri_ll_wait_bits_set(uptr base_addr, uptr offset, u32 bits)
{
    while ((dri_ll_read_reg(base_addr, offset) & bits) != bits)
    {
    }
}

/**
 * @brief 等待寄存器位被清除函数
 *
 * @param base_addr 寄存器基地址
 * @param offset 寄存器偏移地址
 * @param bits 要等待的位掩码
 */

static inline void dri_ll_wait_bits_clear(uptr base_addr, uptr offset, u32 bits)
{
    while ((dri_ll_read_reg(base_addr, offset) & bits) != 0U)
    {
    }
}

/**
 * @brief 寄存器位翻转函数
 *
 * @param base_addr 寄存器基地址
 * @param offset 寄存器偏移地址
 * @param bits 要翻转的位掩码
 */
static inline void dri_ll_toggle_bits(uptr base_addr, uptr offset, u32 bits)
{
    volatile u32* reg_addr = (volatile u32*)(base_addr + offset);
    *reg_addr ^= bits;
}

#endif /* __DRI_LL_H__ */