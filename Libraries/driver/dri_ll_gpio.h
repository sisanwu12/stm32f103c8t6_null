/**
 * @file dri_ll_gpio.h
 * @author BEAM，sisanwu12
 * @brief STM32F103 GPIO 底层驱动对外接口，基于寄存器直接访问方式实现。
 * @version 0.1
 * @date 2026-03-23
 *
 */
#ifndef __DRI_LL_GPIO_H__
#define __DRI_LL_GPIO_H__

#include "data_type.h"

/* ========== 数据定义 ==========*/

/* ---------- GPIO 地址定义层 ---------- */

/* GPIO 基地址 */
#define DRI_LL_PERIPH_BASE_ADDR      (0x40000000UL) // 片上外设总线基地址
#define DRI_LL_APB2_PERIPH_BASE_ADDR (0x40010000UL) // APB2 外设总线基地址

#define DRI_LL_AFIO_BASE_ADDR  (0x40010000UL) // AFIO 起始地址
#define DRI_LL_EXTI_BASE_ADDR  (0x40010400UL) // EXTI 起始地址
#define DRI_LL_GPIOA_BASE_ADDR (0x40010800UL) // GPIOA 起始地址
#define DRI_LL_GPIOB_BASE_ADDR (0x40010C00UL) // GPIOB 起始地址
#define DRI_LL_GPIOC_BASE_ADDR (0x40011000UL) // GPIOC 起始地址
#define DRI_LL_GPIOD_BASE_ADDR (0x40011400UL) // GPIOD 起始地址
#define DRI_LL_GPIOE_BASE_ADDR (0x40011800UL) // GPIOE 起始地址

/* GPIO 寄存器偏移地址 */
#define DRI_LL_GPIO_CRL_OFFSET  0x00UL // GPIO 低位引脚配置寄存器
#define DRI_LL_GPIO_CRH_OFFSET  0x04UL // GPIO 高位引脚配置寄存器
#define DRI_LL_GPIO_IDR_OFFSET  0x08UL // GPIO 输入数据寄存器
#define DRI_LL_GPIO_ODR_OFFSET  0x0CUL // GPIO 输出数据寄存器
#define DRI_LL_GPIO_BSRR_OFFSET 0x10UL // GPIO 位设置/复位寄存器
#define DRI_LL_GPIO_BRR_OFFSET  0x14UL // GPIO 位复位寄存器
#define DRI_LL_GPIO_LCKR_OFFSET 0x18UL // GPIO 配置锁定寄存器

/* ---------- 寄存器位定义 ---------- */

/* crl 低位(0~7) 引脚配置寄存器位定义 */
typedef enum
{
    GPIO_CRL_MODE0_0 = (1UL << 0),                            // 引脚 0 MODE 位 0
    GPIO_CRL_MODE0_1 = (1UL << 1),                            // 引脚 0 MODE 位 1
    GPIO_CRL_MODE0   = (GPIO_CRL_MODE0_0 | GPIO_CRL_MODE0_1), // 引脚 0 MODE 位掩码

    GPIO_CRL_CNF0_0 = (1UL << 2),                          // 引脚 0 CNF 位 0
    GPIO_CRL_CNF0_1 = (1UL << 3),                          // 引脚 0 CNF 位 1
    GPIO_CRL_CNF0   = (GPIO_CRL_CNF0_0 | GPIO_CRL_CNF0_1), // 引脚 0 CNF 位掩码

    GPIO_CRL_MODE1_0 = (1UL << 4),                            // 引脚 1 MODE 位 0
    GPIO_CRL_MODE1_1 = (1UL << 5),                            // 引脚 1 MODE 位 1
    GPIO_CRL_MODE1   = (GPIO_CRL_MODE1_0 | GPIO_CRL_MODE1_1), // 引脚 1 MODE 位掩码

    GPIO_CRL_CNF1_0 = (1UL << 6),                          // 引脚 1 CNF 位 0
    GPIO_CRL_CNF1_1 = (1UL << 7),                          // 引脚 1 CNF 位 1
    GPIO_CRL_CNF1   = (GPIO_CRL_CNF1_0 | GPIO_CRL_CNF1_1), // 引脚 1 CNF 位掩码

    GPIO_CRL_MODE2_0 = (1UL << 8),                            // 引脚 2 MODE 位 0
    GPIO_CRL_MODE2_1 = (1UL << 9),                            // 引脚 2 MODE 位 1
    GPIO_CRL_MODE2   = (GPIO_CRL_MODE2_0 | GPIO_CRL_MODE2_1), // 引脚 2 MODE 位掩码

    GPIO_CRL_CNF2_0 = (1UL << 10),                         // 引脚 2 CNF 位 0
    GPIO_CRL_CNF2_1 = (1UL << 11),                         // 引脚 2 CNF 位 1
    GPIO_CRL_CNF2   = (GPIO_CRL_CNF2_0 | GPIO_CRL_CNF2_1), // 引脚 2 CNF 位掩码

    GPIO_CRL_MODE3_0 = (1UL << 12),                           // 引脚 3 MODE 位 0
    GPIO_CRL_MODE3_1 = (1UL << 13),                           // 引脚 3 MODE 位 1
    GPIO_CRL_MODE3   = (GPIO_CRL_MODE3_0 | GPIO_CRL_MODE3_1), // 引脚 3 MODE 位掩码

    GPIO_CRL_CNF3_0 = (1UL << 14),                         // 引脚 3 CNF 位 0
    GPIO_CRL_CNF3_1 = (1UL << 15),                         // 引脚 3 CNF 位 1
    GPIO_CRL_CNF3   = (GPIO_CRL_CNF3_0 | GPIO_CRL_CNF3_1), // 引脚 3 CNF 位掩码

    GPIO_CRL_MODE4_0 = (1UL << 16),                           // 引脚 4 MODE 位 0
    GPIO_CRL_MODE4_1 = (1UL << 17),                           // 引脚 4 MODE 位 1
    GPIO_CRL_MODE4   = (GPIO_CRL_MODE4_0 | GPIO_CRL_MODE4_1), // 引脚 4 MODE 位掩码

    GPIO_CRL_CNF4_0 = (1UL << 18),                         // 引脚 4 CNF 位 0
    GPIO_CRL_CNF4_1 = (1UL << 19),                         // 引脚 4 CNF 位 1
    GPIO_CRL_CNF4   = (GPIO_CRL_CNF4_0 | GPIO_CRL_CNF4_1), // 引脚 4 CNF 位掩码

    GPIO_CRL_MODE5_0 = (1UL << 20),                           // 引脚 5 MODE 位 0
    GPIO_CRL_MODE5_1 = (1UL << 21),                           // 引脚 5 MODE 位 1
    GPIO_CRL_MODE5   = (GPIO_CRL_MODE5_0 | GPIO_CRL_MODE5_1), // 引脚 5 MODE 位掩码

    GPIO_CRL_CNF5_0 = (1UL << 22),                         // 引脚 5 CNF 位 0
    GPIO_CRL_CNF5_1 = (1UL << 23),                         // 引脚 5 CNF 位 1
    GPIO_CRL_CNF5   = (GPIO_CRL_CNF5_0 | GPIO_CRL_CNF5_1), // 引脚 5 CNF 位掩码

    GPIO_CRL_MODE6_0 = (1UL << 24),                           // 引脚 6 MODE 位 0
    GPIO_CRL_MODE6_1 = (1UL << 25),                           // 引脚 6 MODE 位 1
    GPIO_CRL_MODE6   = (GPIO_CRL_MODE6_0 | GPIO_CRL_MODE6_1), // 引脚 6 MODE 位掩码

    GPIO_CRL_CNF6_0 = (1UL << 26),                         // 引脚 6 CNF 位 0
    GPIO_CRL_CNF6_1 = (1UL << 27),                         // 引脚 6 CNF 位 1
    GPIO_CRL_CNF6   = (GPIO_CRL_CNF6_0 | GPIO_CRL_CNF6_1), // 引脚 6 CNF 位掩码

    GPIO_CRL_MODE7_0 = (1UL << 28),                           // 引脚 7 MODE 位 0
    GPIO_CRL_MODE7_1 = (1UL << 29),                           // 引脚 7 MODE 位 1
    GPIO_CRL_MODE7   = (GPIO_CRL_MODE7_0 | GPIO_CRL_MODE7_1), // 引脚 7 MODE 位掩码

    GPIO_CRL_CNF7_0 = (1UL << 30),                        // 引脚 7 CNF 位 0
    GPIO_CRL_CNF7_1 = (1UL << 31),                        // 引脚 7 CNF 位 1
    GPIO_CRL_CNF7   = (GPIO_CRL_CNF7_0 | GPIO_CRL_CNF7_1) // 引脚 7 CNF 位掩码
} dri_ll_gpio_crl_bits;

/* crh 高位(8~15) 引脚配置寄存器位定义 */
typedef enum
{
    GPIO_CRL_MODE8_0 = (1UL << 0),                            // 引脚 8 MODE 位 0
    GPIO_CRL_MODE8_1 = (1UL << 1),                            // 引脚 8 MODE 位 1
    GPIO_CRL_MODE8   = (GPIO_CRL_MODE8_0 | GPIO_CRL_MODE8_1), // 引脚 8 MODE 位掩码

    GPIO_CRL_CNF8_0 = (1UL << 2),                          // 引脚 8 CNF 位 0
    GPIO_CRL_CNF8_1 = (1UL << 3),                          // 引脚 8 CNF 位 1
    GPIO_CRL_CNF8   = (GPIO_CRL_CNF8_0 | GPIO_CRL_CNF8_1), // 引脚 8 CNF 位掩码

    GPIO_CRL_MODE9_0 = (1UL << 4),                            // 引脚 9 MODE 位 0
    GPIO_CRL_MODE9_1 = (1UL << 5),                            // 引脚 9 MODE 位 1
    GPIO_CRL_MODE9   = (GPIO_CRL_MODE9_0 | GPIO_CRL_MODE9_1), // 引脚 9 MODE 位掩码

    GPIO_CRL_CNF9_0 = (1UL << 6),                          // 引脚 9 CNF 位 0
    GPIO_CRL_CNF9_1 = (1UL << 7),                          // 引脚 9 CNF 位 1
    GPIO_CRL_CNF9   = (GPIO_CRL_CNF9_0 | GPIO_CRL_CNF9_1), // 引脚 9 CNF 位掩码

    GPIO_CRL_MODE10_0 = (1UL << 8),                              // 引脚 10 MODE 位 0
    GPIO_CRL_MODE10_1 = (1UL << 9),                              // 引脚 10 MODE 位 1
    GPIO_CRL_MODE10   = (GPIO_CRL_MODE10_0 | GPIO_CRL_MODE10_1), // 引脚 10 MODE 位掩码

    GPIO_CRL_CNF10_0 = (1UL << 10),                           // 引脚 10 CNF 位 0
    GPIO_CRL_CNF10_1 = (1UL << 11),                           // 引脚 10 CNF 位 1
    GPIO_CRL_CNF10   = (GPIO_CRL_CNF10_0 | GPIO_CRL_CNF10_1), // 引脚 10 CNF 位掩码

    GPIO_CRL_MODE11_0 = (1UL << 12),                             // 引脚 11 MODE 位 0
    GPIO_CRL_MODE11_1 = (1UL << 13),                             // 引脚 11 MODE 位 1
    GPIO_CRL_MODE11   = (GPIO_CRL_MODE11_0 | GPIO_CRL_MODE11_1), // 引脚 11 MODE 位掩码

    GPIO_CRL_CNF11_0 = (1UL << 14),                           // 引脚 11 CNF 位 0
    GPIO_CRL_CNF11_1 = (1UL << 15),                           // 引脚 11 CNF 位 1
    GPIO_CRL_CNF11   = (GPIO_CRL_CNF11_0 | GPIO_CRL_CNF11_1), // 引脚 11 CNF 位掩码

    GPIO_CRL_MODE12_0 = (1UL << 16),                             // 引脚 12 MODE 位 0
    GPIO_CRL_MODE12_1 = (1UL << 17),                             // 引脚 12 MODE 位 1
    GPIO_CRL_MODE12   = (GPIO_CRL_MODE12_0 | GPIO_CRL_MODE12_1), // 引脚 12 MODE 位掩码

    GPIO_CRL_CNF12_0 = (1UL << 18),                           // 引脚 12 CNF 位 0
    GPIO_CRL_CNF12_1 = (1UL << 19),                           // 引脚 12 CNF 位 1
    GPIO_CRL_CNF12   = (GPIO_CRL_CNF12_0 | GPIO_CRL_CNF12_1), // 引脚 12 CNF 位掩码

    GPIO_CRL_MODE13_0 = (1UL << 20),                             // 引脚 13 MODE 位 0
    GPIO_CRL_MODE13_1 = (1UL << 21),                             // 引脚 13 MODE 位 1
    GPIO_CRL_MODE13   = (GPIO_CRL_MODE13_0 | GPIO_CRL_MODE13_1), // 引脚 13 MODE 位掩码

    GPIO_CRL_CNF13_0 = (1UL << 22),                           // 引脚 13 CNF 位 0
    GPIO_CRL_CNF13_1 = (1UL << 23),                           // 引脚 13 CNF 位 1
    GPIO_CRL_CNF13   = (GPIO_CRL_CNF13_0 | GPIO_CRL_CNF13_1), // 引脚 13 CNF 位掩码

    GPIO_CRL_MODE14_0 = (1UL << 24),                             // 引脚 14 MODE 位 0
    GPIO_CRL_MODE14_1 = (1UL << 25),                             // 引脚 14 MODE 位 1
    GPIO_CRL_MODE14   = (GPIO_CRL_MODE14_0 | GPIO_CRL_MODE14_1), // 引脚 14 MODE 位掩码

    GPIO_CRL_CNF14_0 = (1UL << 26),                           // 引脚 14 CNF 位 0
    GPIO_CRL_CNF14_1 = (1UL << 27),                           // 引脚 14 CNF 位 1
    GPIO_CRL_CNF14   = (GPIO_CRL_CNF14_0 | GPIO_CRL_CNF14_1), // 引脚 14 CNF 位掩码

    GPIO_CRL_MODE15_0 = (1UL << 28),                             // 引脚 15 MODE 位 0
    GPIO_CRL_MODE15_1 = (1UL << 29),                             // 引脚 15 MODE 位 1
    GPIO_CRL_MODE15   = (GPIO_CRL_MODE15_0 | GPIO_CRL_MODE15_1), // 引脚 15 MODE 位掩码

    GPIO_CRL_CNF15_0 = (1UL << 30),                          // 引脚 15 CNF 位 0
    GPIO_CRL_CNF15_1 = (1UL << 31),                          // 引脚 15 CNF 位 1
    GPIO_CRL_CNF15   = (GPIO_CRL_CNF15_0 | GPIO_CRL_CNF15_1) // 引脚 15 CNF 位掩码
} dri_ll_gpio_crh_bits;

/* idr 输入数据寄存器位定义 */
typedef enum
{
    GPIO_IDR_PIN0  = (1UL << 0),  // 引脚 0  输入数据位
    GPIO_IDR_PIN1  = (1UL << 1),  // 引脚 1  输入数据位
    GPIO_IDR_PIN2  = (1UL << 2),  // 引脚 2  输入数据位
    GPIO_IDR_PIN3  = (1UL << 3),  // 引脚 3  输入数据位
    GPIO_IDR_PIN4  = (1UL << 4),  // 引脚 4  输入数据位
    GPIO_IDR_PIN5  = (1UL << 5),  // 引脚 5  输入数据位
    GPIO_IDR_PIN6  = (1UL << 6),  // 引脚 6  输入数据位
    GPIO_IDR_PIN7  = (1UL << 7),  // 引脚 7  输入数据位
    GPIO_IDR_PIN8  = (1UL << 8),  // 引脚 8  输入数据位
    GPIO_IDR_PIN9  = (1UL << 9),  // 引脚 9  输入数据位
    GPIO_IDR_PIN10 = (1UL << 10), // 引脚 10 输入数据位
    GPIO_IDR_PIN11 = (1UL << 11), // 引脚 11 输入数据位
    GPIO_IDR_PIN12 = (1UL << 12), // 引脚 12 输入数据位
    GPIO_IDR_PIN13 = (1UL << 13), // 引脚 13 输入数据位
    GPIO_IDR_PIN14 = (1UL << 14), // 引脚 14 输入数据位
    GPIO_IDR_PIN15 = (1UL << 15)  // 引脚 15 输入数据位
} dri_ll_gpio_idr_bits;

/* odr 输出数据寄存器位定义 */
typedef enum
{
    GPIO_ODR_PIN0  = (1UL << 0),  // 引脚 0  输出数据位
    GPIO_ODR_PIN1  = (1UL << 1),  // 引脚 1  输出数据位
    GPIO_ODR_PIN2  = (1UL << 2),  // 引脚 2  输出数据位
    GPIO_ODR_PIN3  = (1UL << 3),  // 引脚 3  输出数据位
    GPIO_ODR_PIN4  = (1UL << 4),  // 引脚 4  输出数据位
    GPIO_ODR_PIN5  = (1UL << 5),  // 引脚 5  输出数据位
    GPIO_ODR_PIN6  = (1UL << 6),  // 引脚 6  输出数据位
    GPIO_ODR_PIN7  = (1UL << 7),  // 引脚 7  输出数据位
    GPIO_ODR_PIN8  = (1UL << 8),  // 引脚 8  输出数据位
    GPIO_ODR_PIN9  = (1UL << 9),  // 引脚 9  输出数据位
    GPIO_ODR_PIN10 = (1UL << 10), // 引脚 10 输出数据位
    GPIO_ODR_PIN11 = (1UL << 11), // 引脚 11 输出数据位
    GPIO_ODR_PIN12 = (1UL << 12), // 引脚 12 输出数据位
    GPIO_ODR_PIN13 = (1UL << 13), // 引脚 13 输出数据位
    GPIO_ODR_PIN14 = (1UL << 14), // 引脚 14 输出数据位
    GPIO_ODR_PIN15 = (1UL << 15)  // 引脚 15 输出数据位
} dri_ll_gpio_odr_bits;

/* bsrr 位设置/复位寄存器 */
typedef enum
{
    GPIO_BSRR_BS0  = (1UL << 0),  // 引脚 0  位置位
    GPIO_BSRR_BS1  = (1UL << 1),  // 引脚 1  位置位
    GPIO_BSRR_BS2  = (1UL << 2),  // 引脚 2  位置位
    GPIO_BSRR_BS3  = (1UL << 3),  // 引脚 3  位置位
    GPIO_BSRR_BS4  = (1UL << 4),  // 引脚 4  位置位
    GPIO_BSRR_BS5  = (1UL << 5),  // 引脚 5  位置位
    GPIO_BSRR_BS6  = (1UL << 6),  // 引脚 6  位置位
    GPIO_BSRR_BS7  = (1UL << 7),  // 引脚 7  位置位
    GPIO_BSRR_BS8  = (1UL << 8),  // 引脚 8  位置位
    GPIO_BSRR_BS9  = (1UL << 9),  // 引脚 9  位置位
    GPIO_BSRR_BS10 = (1UL << 10), // 引脚 10 位置位
    GPIO_BSRR_BS11 = (1UL << 11), // 引脚 11 位置位
    GPIO_BSRR_BS12 = (1UL << 12), // 引脚 12 位置位
    GPIO_BSRR_BS13 = (1UL << 13), // 引脚 13 位置位
    GPIO_BSRR_BS14 = (1UL << 14), // 引脚 14 位置位
    GPIO_BSRR_BS15 = (1UL << 15)  // 引脚 15 位置位
} dri_ll_gpio_bsrr_bits;

/* brr 位复位寄存器 */
typedef enum
{
    GPIO_BRR_BR0  = (1UL << 0),  // 引脚 0  位复位
    GPIO_BRR_BR1  = (1UL << 1),  // 引脚 1  位复位
    GPIO_BRR_BR2  = (1UL << 2),  // 引脚 2  位复位
    GPIO_BRR_BR3  = (1UL << 3),  // 引脚 3  位复位
    GPIO_BRR_BR4  = (1UL << 4),  // 引脚 4  位复位
    GPIO_BRR_BR5  = (1UL << 5),  // 引脚 5  位复位
    GPIO_BRR_BR6  = (1UL << 6),  // 引脚 6  位复位
    GPIO_BRR_BR7  = (1UL << 7),  // 引脚 7  位复位
    GPIO_BRR_BR8  = (1UL << 8),  // 引脚 8  位复位
    GPIO_BRR_BR9  = (1UL << 9),  // 引脚 9  位复位
    GPIO_BRR_BR10 = (1UL << 10), // 引脚 10 位复位
    GPIO_BRR_BR11 = (1UL << 11), // 引脚 11 位复位
    GPIO_BRR_BR12 = (1UL << 12), // 引脚 12 位复位
    GPIO_BRR_BR13 = (1UL << 13), // 引脚 13 位复位
    GPIO_BRR_BR14 = (1UL << 14), // 引脚 14 位复位
    GPIO_BRR_BR15 = (1UL << 15)  // 引脚 15 位复位
} dri_ll_gpio_brr_bits;

/* lckr 配置锁定寄存器位定义 */
typedef enum
{
    GPIO_LCKR_LCK0  = (1UL << 0),  // 引脚 0  配置锁定
    GPIO_LCKR_LCK1  = (1UL << 1),  // 引脚 1  配置锁定
    GPIO_LCKR_LCK2  = (1UL << 2),  // 引脚 2  配置锁定
    GPIO_LCKR_LCK3  = (1UL << 3),  // 引脚 3  配置锁定
    GPIO_LCKR_LCK4  = (1UL << 4),  // 引脚 4  配置锁定
    GPIO_LCKR_LCK5  = (1UL << 5),  // 引脚 5  配置锁定
    GPIO_LCKR_LCK6  = (1UL << 6),  // 引脚 6  配置锁定
    GPIO_LCKR_LCK7  = (1UL << 7),  // 引脚 7  配置锁定
    GPIO_LCKR_LCK8  = (1UL << 8),  // 引脚 8  配置锁定
    GPIO_LCKR_LCK9  = (1UL << 9),  // 引脚 9  配置锁定
    GPIO_LCKR_LCK10 = (1UL << 10), // 引脚 10 配置锁定
    GPIO_LCKR_LCK11 = (1UL << 11), // 引脚 11 配置锁定
    GPIO_LCKR_LCK12 = (1UL << 12), // 引脚 12 配置锁定
    GPIO_LCKR_LCK13 = (1UL << 13), // 引脚 13 配置锁定
    GPIO_LCKR_LCK14 = (1UL << 14), // 引脚 14 配置锁定
    GPIO_LCKR_LCK15 = (1UL << 15), // 引脚 15 配置锁定
    GPIO_LCKR_LCKK  = (1UL << 16)  // 锁定键位
} dri_ll_gpio_reg_t;

/* ---------- 参数 ---------- */

/* GPIO 端口枚举 */
typedef enum
{
    GPIO_PORT_A = 0U, // 端口 A
    GPIO_PORT_B = 1U, // 端口 B
    GPIO_PORT_C = 2U, // 端口 C
    GPIO_PORT_D = 3U, // 端口 D
    GPIO_PORT_E = 4U, // 端口 E
} dri_ll_gpio_port_t;

/* GPIO 引脚编号枚举 */
typedef enum
{
    GPIO_PIN_0  = 0U,  // 引脚 0
    GPIO_PIN_1  = 1U,  // 引脚 1
    GPIO_PIN_2  = 2U,  // 引脚 2
    GPIO_PIN_3  = 3U,  // 引脚 3
    GPIO_PIN_4  = 4U,  // 引脚 4
    GPIO_PIN_5  = 5U,  // 引脚 5
    GPIO_PIN_6  = 6U,  // 引脚 6
    GPIO_PIN_7  = 7U,  // 引脚 7
    GPIO_PIN_8  = 8U,  // 引脚 8
    GPIO_PIN_9  = 9U,  // 引脚 9
    GPIO_PIN_10 = 10U, // 引脚 10
    GPIO_PIN_11 = 11U, // 引脚 11
    GPIO_PIN_12 = 12U, // 引脚 12
    GPIO_PIN_13 = 13U, // 引脚 13
    GPIO_PIN_14 = 14U, // 引脚 14
    GPIO_PIN_15 = 15U, // 引脚 15
} dri_ll_gpio_pin_t;

/* GPIO 输出电平枚举 */
typedef enum
{
    GPIO_LEVEL_LOW  = 0U, // 低电平
    GPIO_LEVEL_HIGH = 1U, // 高电平
} dri_ll_gpio_level_t;    // GPIO 电平类型

/* GPIO 模式与输出速度枚举 */
typedef enum
{
    GPIO_MODE_INPUT     = 0U, // 输入模式
    GPIO_MODE_OUTPUT_10 = 1U, // 输出模式，10MHz
    GPIO_MODE_OUTPUT_2  = 2U, // 输出模式，2MHz
    GPIO_MODE_OUTPUT_50 = 3U, // 输出模式，50MHz
} dri_ll_gpio_mode_t;

typedef enum
{
    GPIO_CNF_INPUT_ANALOG = 0U, // 模拟输入
    GPIO_CNF_INPUT_FLOAT  = 1U, // 浮空输入
    GPIO_CNF_INPUT_PU_PD  = 2U, // 上拉/下拉输入

    GPIO_CNF_OUTPUT_PP    = 0U, // 推挽输出
    GPIO_CNF_OUTPUT_OD    = 1U, // 开漏输出
    GPIO_CNF_OUTPUT_AF_PP = 2U, // 复用推挽输出
    GPIO_CNF_OUTPUT_AF_OD = 3U, // 复用开漏输出
} dri_ll_gpio_cnf_t;

/* GPIO 初始化返回值枚举 */
typedef enum
{
    GPIO_INIT_SUCCESS = 0U, // 初始化成功
    GPIO_INIT_ERROR   = 1U

} dri_ll_gpio_init_ret;

/* ========== 对外接口 ========== */

/* GPIO 初始化函数 */
#endif /* __DRI_LL_GPIO_H__ */
