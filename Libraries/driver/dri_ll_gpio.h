/**
 * @file dri_ll_gpio.h
 * @author BEAM, sisanwu12
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

/* GPIO 初始化结构体 */
typedef struct
{
    dri_ll_gpio_port_t  port;
    dri_ll_gpio_pin_t   pin;
    dri_ll_gpio_mode_t  mode;
    dri_ll_gpio_cnf_t   cnf;
    dri_ll_gpio_level_t level;
} dri_ll_gpio_init_t;

/* GPIO 初始化返回值枚举 */
typedef enum
{
    GPIO_INIT_SUCCESS           = 0U, // 初始化成功
    GPIO_INIT_ERROR_INVALID_CFG = 1U, // 配置指针无效错误
} dri_ll_gpio_init_ret;

/* ========== 对外接口 ========== */

/* GPIO 初始化函数 */
dri_ll_gpio_init_ret dri_ll_gpio_init(const dri_ll_gpio_init_t* cfg);
/* GPIO 引脚操作函数 */
void dri_ll_gpio_set_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin);
/* GPIO 引脚复位函数 */
void dri_ll_gpio_reset_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin);
/* GPIO 引脚写入函数 */
void dri_ll_gpio_write_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin,
                           dri_ll_gpio_level_t level);
/* GPIO 引脚读取函数 */
dri_ll_gpio_level_t dri_ll_gpio_read_input(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin);
/* GPIO 引脚输出读取函数 */
dri_ll_gpio_level_t dri_ll_gpio_read_output(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin);
/* GPIO 引脚翻转函数 */
void dri_ll_gpio_toggle_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin);

#endif /* __DRI_LL_GPIO_H__ */
