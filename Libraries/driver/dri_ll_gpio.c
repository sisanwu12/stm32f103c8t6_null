/**
 * @file dri_ll_gpio.c
 * @author BEAM, sisanwu12
 * @brief STM32F103 GPIO 底层驱动实现文件。
 * @note 本实现遵循 STM32F1 的 CRL/CRH 配置模型，用户层采用单个模式枚举加输出速度配置。 在 Cortex-M
 * 目标上，GPIO 翻转操作会在短临界区内完成，以降低与中断并发访问时的状态竞争风险。
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 */

/* ========== 包含头文件 ========== */
#include "dri_ll_gpio.h" // 引入 GPIO LL 对外接口
#include "dri_ll.h"      // 引入寄存器基础操作函数

/* ========== 内部函数声明 ========== */

/* 获取 GPIO 基地址 */
static inline uptr dri_ll_gpio_get_base(dri_ll_gpio_port_t port);
/* 获取 GPIO 配置寄存器偏移量 */
static inline uptr dri_ll_gpio_get_cfg_offset(dri_ll_gpio_pin_t pin);
/* 获取 GPIO 配置移位 */
static inline u32 dri_ll_gpio_get_cfg_shift(dri_ll_gpio_pin_t pin);
/* 获取 GPIO 引脚掩码 */
static inline u32 dri_ll_gpio_get_pin_mask(dri_ll_gpio_pin_t pin);

/* ========== 对外接口实现 ========== */

/**
 * @brief gpio 初始化函数
 *
 * @param cfg 初始化配置结构体指针
 * @return dri_ll_gpio_init_ret 初始化结果
 */
dri_ll_gpio_init_ret dri_ll_gpio_init(const dri_ll_gpio_init_t* cfg)
{
    if (cfg == NULL)
    {
        return GPIO_INIT_ERROR_INVALID_CFG; // 配置指针无效
    }

    // 获取 GPIO 基地址和配置寄存器信息
    uptr base_addr = dri_ll_gpio_get_base(cfg->port);

    // 获取配置寄存器地址
    uptr cfg_offset = dri_ll_gpio_get_cfg_offset(cfg->pin);

    // 获取该引脚配置字段在寄存器中的位偏移
    u32 cfg_shift = dri_ll_gpio_get_cfg_shift(cfg->pin);

    // 计算清除和设置掩码
    // 生成位数掩码，每个引脚的配置位有 4 位（CNF[1:0] + MODE[1:0]）
    u32 clear_mask = DRI_MASK_4 << cfg_shift;

    // 计算新的配置值
    u32 pin_cfg  = ((cfg->cnf & 0x3) << 2) | (cfg->mode & 0x3);
    u32 set_mask = pin_cfg << cfg_shift;

    // 配置 GPIO 模式和输出类型
    dri_ll_modify_reg(base_addr, cfg_offset, clear_mask, set_mask);

    return GPIO_INIT_SUCCESS; // 初始化成功
}

/**
 * @brief GPIO 引脚操作函数
 *
 * @param port GPIO 端口
 * @param pin GPIO 引脚
 */
void dri_ll_gpio_set_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin)
{
    dri_ll_gpio_write_pin(port, pin, GPIO_LEVEL_HIGH);
}

/**
 * @brief GPIO 引脚复位函数
 *
 * @param port GPIO 端口
 * @param pin GPIO 引脚
 */
void dri_ll_gpio_reset_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin)
{
    dri_ll_gpio_write_pin(port, pin, GPIO_LEVEL_LOW);
}

/**
 * @brief GPIO 引脚写入函数
 *
 * @param port GPIO 端口
 * @param pin GPIO 引脚
 * @param level 目标电平
 */
void dri_ll_gpio_write_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin,
                           dri_ll_gpio_level_t level)
{
    uptr base_addr = dri_ll_gpio_get_base(port);
    u32  pin_mask  = dri_ll_gpio_get_pin_mask(pin);

    if (level == GPIO_LEVEL_HIGH)
    {
        dri_ll_write_reg(base_addr, DRI_LL_GPIO_BSRR_OFFSET, pin_mask);
    }
    else
    {
        dri_ll_write_reg(base_addr, DRI_LL_GPIO_BRR_OFFSET, pin_mask);
    }
}

/**
 * @brief GPIO 引脚输入读取函数
 *
 * @param port GPIO 端口
 * @param pin GPIO 引脚
 * @return dri_ll_gpio_level_t 读取到的电平值
 */
dri_ll_gpio_level_t dri_ll_gpio_read_input(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin)
{
    // 实现 GPIO 引脚读取逻辑
    u32 idr_value = dri_ll_read_reg(dri_ll_gpio_get_base(port), DRI_LL_GPIO_IDR_OFFSET);
    return (idr_value & dri_ll_gpio_get_pin_mask(pin)) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
}

/**
 * @brief GPIO 引脚输出读取函数
 *
 * @param port GPIO 端口
 * @param pin GPIO 引脚
 * @return dri_ll_gpio_level_t 读取到的电平值
 */
dri_ll_gpio_level_t dri_ll_gpio_read_output(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin)
{
    // 实现 GPIO 引脚输出读取逻辑
    u32 odr_value = dri_ll_read_reg(dri_ll_gpio_get_base(port), DRI_LL_GPIO_ODR_OFFSET);
    return (odr_value & dri_ll_gpio_get_pin_mask(pin)) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
}

/**
 * @brief GPIO 引脚翻转函数
 *
 * @param port GPIO 端口
 * @param pin GPIO 引脚
 */
void dri_ll_gpio_toggle_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin)
{
    uptr base_addr = dri_ll_gpio_get_base(port);
    u32  pin_mask  = dri_ll_gpio_get_pin_mask(pin);
    u32  odr_value = dri_ll_read_reg(base_addr, DRI_LL_GPIO_ODR_OFFSET);

    if (odr_value & pin_mask)
    {
        dri_ll_write_reg(base_addr, DRI_LL_GPIO_BRR_OFFSET, pin_mask);
    }
    else
    {
        dri_ll_write_reg(base_addr, DRI_LL_GPIO_BSRR_OFFSET, pin_mask);
    }
}

/* ========== 内部函数实现 ========== */

/* 获取 GPIO 基地址 */
static inline uptr dri_ll_gpio_get_base(dri_ll_gpio_port_t port)
{
    switch (port)
    {
    case GPIO_PORT_A:
        return DRI_LL_GPIOA_BASE_ADDR;
    case GPIO_PORT_B:
        return DRI_LL_GPIOB_BASE_ADDR;
    case GPIO_PORT_C:
        return DRI_LL_GPIOC_BASE_ADDR;
    case GPIO_PORT_D:
        return DRI_LL_GPIOD_BASE_ADDR;
    case GPIO_PORT_E:
        return DRI_LL_GPIOE_BASE_ADDR;
    default:
        return 0UL; // 无效端口返回 0
    }
}

/* 获取 GPIO 配置寄存器偏移量 */
static inline uptr dri_ll_gpio_get_cfg_offset(dri_ll_gpio_pin_t pin)
{
    if (pin <= GPIO_PIN_7)
    {
        return DRI_LL_GPIO_CRL_OFFSET; // 低位引脚配置寄存器
    }
    else if (pin <= GPIO_PIN_15)
    {
        return DRI_LL_GPIO_CRH_OFFSET; // 高位引脚配置寄存器
    }
    else
    {
        return 0UL; // 无效引脚返回 0
    }
}

/* 获取 GPIO 配置移位 */
static inline u32 dri_ll_gpio_get_cfg_shift(dri_ll_gpio_pin_t pin)
{
    return (pin % 8) * 4;
}

/* 获取 GPIO 引脚掩码 */
static inline u32 dri_ll_gpio_get_pin_mask(dri_ll_gpio_pin_t pin)
{
    return 1UL << pin;
}