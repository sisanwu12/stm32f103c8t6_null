/**
 * @file dri_ll_gpio.h
 * @author BEAM
 * @brief STM32F103 GPIO 底层驱动对外接口，基于寄存器直接访问方式实现。
 * @note 本模块只负责 GPIO 本身的地址映射、模式配置与电平读写，不负责 RCC 时钟开启。
 *       在调用初始化接口或其他 GPIO 访问接口之前，需要先由外部 RCC 模块打开对应 GPIO 端口时钟。
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef DRI_LL_GPIO_H // 防止头文件被重复包含
#define DRI_LL_GPIO_H // 定义头文件保护宏

#include <stddef.h> // 提供 NULL 定义
#include <stdint.h> // 提供标准整数类型

#ifdef __cplusplus // 兼容 C++ 编译器
extern "C"
{      // 使用 C 链接方式导出接口
#endif // 结束 C++ 兼容判断

    /* ==================== 地址定义层 ==================== */ // GPIO 地址与寄存器映射定义层

#define DRI_LL_PERIPH_BASE_ADDR      (0x40000000UL) // 片上外设总线基地址
#define DRI_LL_APB2_PERIPH_BASE_ADDR (0x40010000UL) // APB2 外设总线基地址

#define DRI_LL_AFIO_BASE_ADDR (0x40010000UL) // AFIO 起始地址
#define DRI_LL_AFIO_END_ADDR  (0x400103FFUL) // AFIO 终止地址
#define DRI_LL_EXTI_BASE_ADDR (0x40010400UL) // EXTI 起始地址
#define DRI_LL_EXTI_END_ADDR  (0x400107FFUL) // EXTI 终止地址

#define DRI_LL_GPIO_PORT_SPAN  (0x00000400UL) // 单个 GPIO 端口的地址跨度
#define DRI_LL_GPIOA_BASE_ADDR (0x40010800UL) // GPIOA 起始地址
#define DRI_LL_GPIOA_END_ADDR  (0x40010BFFUL) // GPIOA 终止地址
#define DRI_LL_GPIOB_BASE_ADDR (0x40010C00UL) // GPIOB 起始地址
#define DRI_LL_GPIOB_END_ADDR  (0x40010FFFUL) // GPIOB 终止地址
#define DRI_LL_GPIOC_BASE_ADDR (0x40011000UL) // GPIOC 起始地址
#define DRI_LL_GPIOC_END_ADDR  (0x400113FFUL) // GPIOC 终止地址
#define DRI_LL_GPIOD_BASE_ADDR (0x40011400UL) // GPIOD 起始地址
#define DRI_LL_GPIOD_END_ADDR  (0x400117FFUL) // GPIOD 终止地址
#define DRI_LL_GPIOE_BASE_ADDR (0x40011800UL) // GPIOE 起始地址
#define DRI_LL_GPIOE_END_ADDR  (0x40011BFFUL) // GPIOE 终止地址

#define DRI_LL_GPIO_CRL_OFFSET  (0x00UL) // GPIO CRL 寄存器偏移
#define DRI_LL_GPIO_CRH_OFFSET  (0x04UL) // GPIO CRH 寄存器偏移
#define DRI_LL_GPIO_IDR_OFFSET  (0x08UL) // GPIO IDR 寄存器偏移
#define DRI_LL_GPIO_ODR_OFFSET  (0x0CUL) // GPIO ODR 寄存器偏移
#define DRI_LL_GPIO_BSRR_OFFSET (0x10UL) // GPIO BSRR 寄存器偏移
#define DRI_LL_GPIO_BRR_OFFSET  (0x14UL) // GPIO BRR 寄存器偏移
#define DRI_LL_GPIO_LCKR_OFFSET (0x18UL) // GPIO LCKR 寄存器偏移

#define DRI_LL_GPIO_PIN_COUNT (16U) // 每个 GPIO 端口拥有 16 个引脚

    typedef struct // GPIO 寄存器映射结构体
    {
        volatile uint32_t CRL;  // 引脚 0~7 配置寄存器
        volatile uint32_t CRH;  // 引脚 8~15 配置寄存器
        volatile uint32_t IDR;  // 输入数据寄存器
        volatile uint32_t ODR;  // 输出数据寄存器
        volatile uint32_t BSRR; // 位置位/复位寄存器
        volatile uint32_t BRR;  // 端口位复位寄存器
        volatile uint32_t LCKR; // 配置锁定寄存器
    } dri_ll_gpio_reg_t;        // GPIO 寄存器映射类型

#define DRI_LL_GPIOA ((dri_ll_gpio_reg_t*)DRI_LL_GPIOA_BASE_ADDR) // GPIOA 寄存器入口
#define DRI_LL_GPIOB ((dri_ll_gpio_reg_t*)DRI_LL_GPIOB_BASE_ADDR) // GPIOB 寄存器入口
#define DRI_LL_GPIOC ((dri_ll_gpio_reg_t*)DRI_LL_GPIOC_BASE_ADDR) // GPIOC 寄存器入口
#define DRI_LL_GPIOD ((dri_ll_gpio_reg_t*)DRI_LL_GPIOD_BASE_ADDR) // GPIOD 寄存器入口
#define DRI_LL_GPIOE ((dri_ll_gpio_reg_t*)DRI_LL_GPIOE_BASE_ADDR) // GPIOE 寄存器入口

    /* ==================== 基础操作层 ==================== */ // GPIO 基础寄存器操作层

    typedef enum // GPIO 端口枚举
    {
        DRI_LL_GPIO_PORT_A = 0U, // 端口 A
        DRI_LL_GPIO_PORT_B = 1U, // 端口 B
        DRI_LL_GPIO_PORT_C = 2U, // 端口 C
        DRI_LL_GPIO_PORT_D = 3U, // 端口 D
        DRI_LL_GPIO_PORT_E = 4U, // 端口 E
    } dri_ll_gpio_port_t;        // GPIO 端口类型

    typedef enum // GPIO 引脚编号枚举
    {
        DRI_LL_GPIO_PIN_0  = 0U,  // 引脚 0
        DRI_LL_GPIO_PIN_1  = 1U,  // 引脚 1
        DRI_LL_GPIO_PIN_2  = 2U,  // 引脚 2
        DRI_LL_GPIO_PIN_3  = 3U,  // 引脚 3
        DRI_LL_GPIO_PIN_4  = 4U,  // 引脚 4
        DRI_LL_GPIO_PIN_5  = 5U,  // 引脚 5
        DRI_LL_GPIO_PIN_6  = 6U,  // 引脚 6
        DRI_LL_GPIO_PIN_7  = 7U,  // 引脚 7
        DRI_LL_GPIO_PIN_8  = 8U,  // 引脚 8
        DRI_LL_GPIO_PIN_9  = 9U,  // 引脚 9
        DRI_LL_GPIO_PIN_10 = 10U, // 引脚 10
        DRI_LL_GPIO_PIN_11 = 11U, // 引脚 11
        DRI_LL_GPIO_PIN_12 = 12U, // 引脚 12
        DRI_LL_GPIO_PIN_13 = 13U, // 引脚 13
        DRI_LL_GPIO_PIN_14 = 14U, // 引脚 14
        DRI_LL_GPIO_PIN_15 = 15U, // 引脚 15
    } dri_ll_gpio_pin_t;          // GPIO 引脚类型

    dri_ll_gpio_reg_t*
             dri_ll_gpio_get_reg(dri_ll_gpio_port_t port);       // 根据端口编号获取 GPIO 寄存器入口
    uint16_t dri_ll_gpio_get_pin_mask(dri_ll_gpio_pin_t pin);    // 根据引脚编号生成位掩码
    void     dri_ll_gpio_config_pin_raw(dri_ll_gpio_port_t port, // 直接写入指定引脚的 4bit 配置字段
                                        dri_ll_gpio_pin_t pin, uint32_t cfg_bits);
    void     dri_ll_gpio_write_mask_raw(dri_ll_gpio_port_t port, // 通过 BSRR 原子写入置位和复位掩码
                                        uint16_t set_mask, uint16_t reset_mask);
    uint16_t dri_ll_gpio_read_input_raw(dri_ll_gpio_port_t port);  // 读取整个端口的 IDR 原始值
    uint16_t dri_ll_gpio_read_output_raw(dri_ll_gpio_port_t port); // 读取整个端口的 ODR 原始值
    void     dri_ll_gpio_write_output_raw(dri_ll_gpio_port_t port, // 直接写入整个端口的 ODR 值
                                          uint16_t           value);

    /* ==================== 用户接口层 ==================== */ // GPIO 对外使用接口层

    typedef enum // GPIO 输出电平枚举
    {
        DRI_LL_GPIO_LEVEL_LOW  = 0U, // 低电平
        DRI_LL_GPIO_LEVEL_HIGH = 1U, // 高电平
    } dri_ll_gpio_level_t;           // GPIO 电平类型

    typedef enum // GPIO 上下拉方向枚举
    {
        DRI_LL_GPIO_PULL_DOWN = 0U, // 下拉输入
        DRI_LL_GPIO_PULL_UP   = 1U, // 上拉输入
    } dri_ll_gpio_pull_t;           // GPIO 上下拉类型

    typedef enum // STM32F1 GPIO 4bit 配置字段枚举
    {
        DRI_LL_GPIO_MODE_ANALOG          = 0x0U, // 模拟输入
        DRI_LL_GPIO_MODE_OUTPUT_PP_10MHZ = 0x1U, // 通用推挽输出 10MHz
        DRI_LL_GPIO_MODE_OUTPUT_PP_2MHZ  = 0x2U, // 通用推挽输出 2MHz
        DRI_LL_GPIO_MODE_OUTPUT_PP_50MHZ = 0x3U, // 通用推挽输出 50MHz
        DRI_LL_GPIO_MODE_INPUT_FLOATING  = 0x4U, // 浮空输入
        DRI_LL_GPIO_MODE_OUTPUT_OD_10MHZ = 0x5U, // 通用开漏输出 10MHz
        DRI_LL_GPIO_MODE_OUTPUT_OD_2MHZ  = 0x6U, // 通用开漏输出 2MHz
        DRI_LL_GPIO_MODE_OUTPUT_OD_50MHZ = 0x7U, // 通用开漏输出 50MHz
        DRI_LL_GPIO_MODE_INPUT_PULL      = 0x8U, // 上拉/下拉输入
        DRI_LL_GPIO_MODE_AF_PP_10MHZ     = 0x9U, // 复用推挽输出 10MHz
        DRI_LL_GPIO_MODE_AF_PP_2MHZ      = 0xAU, // 复用推挽输出 2MHz
        DRI_LL_GPIO_MODE_AF_PP_50MHZ     = 0xBU, // 复用推挽输出 50MHz
        DRI_LL_GPIO_MODE_RESERVED_INPUT  = 0xCU, // 保留输入配置位型，初始化接口禁止使用
        DRI_LL_GPIO_MODE_AF_OD_10MHZ     = 0xDU, // 复用开漏输出 10MHz
        DRI_LL_GPIO_MODE_AF_OD_2MHZ      = 0xEU, // 复用开漏输出 2MHz
        DRI_LL_GPIO_MODE_AF_OD_50MHZ     = 0xFU, // 复用开漏输出 50MHz
    } dri_ll_gpio_mode_t;                        // GPIO 模式类型

    typedef struct // GPIO 初始化参数结构体
    {
        dri_ll_gpio_port_t  port;          // 目标 GPIO 端口
        dri_ll_gpio_pin_t   pin;           // 目标 GPIO 引脚
        dri_ll_gpio_mode_t  mode;          // 目标 GPIO 模式
        dri_ll_gpio_pull_t  pull;          // 输入上下拉方向
        dri_ll_gpio_level_t initial_level; // 输出初始电平
    } dri_ll_gpio_init_t;                  // GPIO 初始化参数类型

    void
         dri_ll_gpio_init(const dri_ll_gpio_init_t*
                              config); // 初始化一个 GPIO 引脚，调用前需先由外部 RCC 模块打开端口时钟
    void dri_ll_gpio_write_pin(dri_ll_gpio_port_t port, // 写一个 GPIO 引脚电平
                               dri_ll_gpio_pin_t pin, dri_ll_gpio_level_t level);
    void dri_ll_gpio_set_pin(dri_ll_gpio_port_t port,
                             dri_ll_gpio_pin_t  pin); // 将一个 GPIO 引脚置高
    void dri_ll_gpio_reset_pin(dri_ll_gpio_port_t port,
                               dri_ll_gpio_pin_t  pin); // 将一个 GPIO 引脚置低
    void dri_ll_gpio_toggle_pin(dri_ll_gpio_port_t port,
                                dri_ll_gpio_pin_t  pin); // 翻转一个 GPIO 引脚输出状态
    dri_ll_gpio_level_t
    dri_ll_gpio_read_input_pin(dri_ll_gpio_port_t port, // 读取一个 GPIO 引脚输入电平
                               dri_ll_gpio_pin_t  pin);
    dri_ll_gpio_level_t
             dri_ll_gpio_read_output_pin(dri_ll_gpio_port_t port, // 读取一个 GPIO 引脚输出电平
                                         dri_ll_gpio_pin_t  pin);
    uint16_t dri_ll_gpio_read_input_port(dri_ll_gpio_port_t port);            // 读取整个端口输入值
    uint16_t dri_ll_gpio_read_output_port(dri_ll_gpio_port_t port);           // 读取整个端口输出值
    void     dri_ll_gpio_write_port(dri_ll_gpio_port_t port, uint16_t value); // 写整个端口输出值

#ifdef __cplusplus // 兼容 C++ 编译器
} // 结束 C 链接方式导出
#endif // 结束 C++ 兼容判断

#endif // 结束头文件保护
