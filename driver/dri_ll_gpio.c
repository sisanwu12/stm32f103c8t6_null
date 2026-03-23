/**
 * @file dri_ll_gpio.c
 * @author BEAM (you@domain.com)
 * @brief
 * @note
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "dri_ll_gpio.h" // 引入 GPIO LL 对外接口

static uint8_t dri_ll_gpio_is_valid_port(dri_ll_gpio_port_t port) // 判断端口编号是否合法
{
    return (uint8_t)(port <= DRI_LL_GPIO_PORT_E); // 端口范围限制在 A~E
}

static uint8_t dri_ll_gpio_is_valid_pin(dri_ll_gpio_pin_t pin) // 判断引脚编号是否合法
{
    return (uint8_t)(pin < DRI_LL_GPIO_PIN_COUNT); // 引脚范围限制在 0~15
}

static uint32_t dri_ll_gpio_get_cfg_shift(dri_ll_gpio_pin_t pin) // 计算引脚配置字段的位移量
{
    return (((uint32_t)pin & 0x7UL) * 4UL); // 每个引脚占用 4 个配置位
}

static volatile uint32_t*
dri_ll_gpio_get_cfg_reg(dri_ll_gpio_reg_t* gpio, // 获取 CRL 或 CRH 的入口地址
                        dri_ll_gpio_pin_t  pin)
{
    return ((pin < DRI_LL_GPIO_PIN_8) ? &gpio->CRL
                                      : &gpio->CRH); // 低 8 位引脚使用 CRL，高 8 位引脚使用 CRH
}

static uint8_t dri_ll_gpio_mode_is_output(dri_ll_gpio_mode_t mode) // 判断模式是否属于输出模式
{
    return (uint8_t)(((uint32_t)mode & 0x3UL) != 0UL); // MODE[1:0] 非 00 时表示输出模式
}

static uint8_t
dri_ll_gpio_mode_is_input_pull(dri_ll_gpio_mode_t mode) // 判断模式是否为上拉/下拉输入
{
    return (uint8_t)(mode == DRI_LL_GPIO_MODE_INPUT_PULL); // 仅 0x8 对应上拉/下拉输入
}

dri_ll_gpio_reg_t* dri_ll_gpio_get_reg(dri_ll_gpio_port_t port) // 根据端口编号获取 GPIO 寄存器入口
{
    switch (port) // 按端口编号选择寄存器基址
    {
    case DRI_LL_GPIO_PORT_A: // GPIOA 分支
        return DRI_LL_GPIOA; // 返回 GPIOA 寄存器入口

    case DRI_LL_GPIO_PORT_B: // GPIOB 分支
        return DRI_LL_GPIOB; // 返回 GPIOB 寄存器入口

    case DRI_LL_GPIO_PORT_C: // GPIOC 分支
        return DRI_LL_GPIOC; // 返回 GPIOC 寄存器入口

    case DRI_LL_GPIO_PORT_D: // GPIOD 分支
        return DRI_LL_GPIOD; // 返回 GPIOD 寄存器入口

    case DRI_LL_GPIO_PORT_E: // GPIOE 分支
        return DRI_LL_GPIOE; // 返回 GPIOE 寄存器入口

    default:         // 非法端口分支
        return NULL; // 返回空指针表示无效
    }
}

uint16_t dri_ll_gpio_get_pin_mask(dri_ll_gpio_pin_t pin) // 根据引脚编号生成位掩码
{
    if (dri_ll_gpio_is_valid_pin(pin) == 0U) // 检查引脚编号是否越界
    {
        return 0U; // 非法引脚直接返回空掩码
    }

    return (uint16_t)(1UL << (uint32_t)pin); // 将引脚编号转换成单 bit 掩码
}

void dri_ll_gpio_clock_enable_raw(dri_ll_gpio_port_t port) // 直接打开指定 GPIO 端口时钟
{
    uint32_t clock_mask = 0UL; // 保存当前端口对应的 RCC 时钟位

    switch (port) // 根据端口选择 RCC APB2ENR 中的时钟位
    {
    case DRI_LL_GPIO_PORT_A:                        // GPIOA 分支
        clock_mask = DRI_LL_RCC_APB2ENR_IOPAEN_Msk; // 选择 GPIOA 时钟位
        break;                                      // 结束当前分支

    case DRI_LL_GPIO_PORT_B:                        // GPIOB 分支
        clock_mask = DRI_LL_RCC_APB2ENR_IOPBEN_Msk; // 选择 GPIOB 时钟位
        break;                                      // 结束当前分支

    case DRI_LL_GPIO_PORT_C:                        // GPIOC 分支
        clock_mask = DRI_LL_RCC_APB2ENR_IOPCEN_Msk; // 选择 GPIOC 时钟位
        break;                                      // 结束当前分支

    case DRI_LL_GPIO_PORT_D:                        // GPIOD 分支
        clock_mask = DRI_LL_RCC_APB2ENR_IOPDEN_Msk; // 选择 GPIOD 时钟位
        break;                                      // 结束当前分支

    case DRI_LL_GPIO_PORT_E:                        // GPIOE 分支
        clock_mask = DRI_LL_RCC_APB2ENR_IOPEEN_Msk; // 选择 GPIOE 时钟位
        break;                                      // 结束当前分支

    default:   // 非法端口分支
        break; // 保持掩码为 0
    }

    if (clock_mask == 0UL) // 如果没有匹配到合法时钟位
    {
        return; // 直接返回避免误写寄存器
    }

    DRI_LL_RCC->APB2ENR |= clock_mask; // 打开对应 GPIO 端口时钟
    (void)DRI_LL_RCC->APB2ENR;         // 通过一次回读保证写入已经生效
}

void dri_ll_gpio_config_pin_raw(dri_ll_gpio_port_t port, // 直接写入指定引脚的 4bit 配置字段
                                dri_ll_gpio_pin_t pin, uint32_t cfg_bits)
{
    dri_ll_gpio_reg_t* gpio    = NULL; // 保存目标 GPIO 寄存器入口
    volatile uint32_t* cfg_reg = NULL; // 保存目标配置寄存器入口
    uint32_t           shift   = 0UL;  // 保存目标引脚在寄存器中的位移

    if ((dri_ll_gpio_is_valid_port(port) == 0U) ||
        (dri_ll_gpio_is_valid_pin(pin) == 0U)) // 校验端口和引脚是否合法
    {
        return; // 任一参数非法时直接返回
    }

    gpio = dri_ll_gpio_get_reg(port); // 获取目标端口的寄存器入口

    if (gpio == NULL) // 检查寄存器入口是否为空
    {
        return; // 无法获取目标端口时直接返回
    }

    cfg_reg = dri_ll_gpio_get_cfg_reg(gpio, pin); // 根据引脚位置选择 CRL 或 CRH
    shift   = dri_ll_gpio_get_cfg_shift(pin);     // 计算当前引脚的 4bit 字段偏移量

    *cfg_reg &= ~(0xFUL << shift);             // 先清除该引脚原有的 4bit 配置字段
    *cfg_reg |= ((cfg_bits & 0xFUL) << shift); // 再写入新的 4bit 配置字段
}

void dri_ll_gpio_write_mask_raw(dri_ll_gpio_port_t port, // 通过 BSRR 原子写入置位和复位掩码
                                uint16_t set_mask, uint16_t reset_mask)
{
    dri_ll_gpio_reg_t* gpio = NULL; // 保存目标 GPIO 寄存器入口

    gpio = dri_ll_gpio_get_reg(port); // 获取目标端口的寄存器入口

    if (gpio == NULL) // 检查端口是否有效
    {
        return; // 非法端口时直接返回
    }

    gpio->BSRR = ((uint32_t)set_mask |
                  ((uint32_t)reset_mask << 16UL)); // 使用 BSRR 在一次写操作中同时置位和复位
}

uint16_t dri_ll_gpio_read_input_raw(dri_ll_gpio_port_t port) // 读取整个端口的 IDR 原始值
{
    dri_ll_gpio_reg_t* gpio = NULL; // 保存目标 GPIO 寄存器入口

    gpio = dri_ll_gpio_get_reg(port); // 获取目标端口的寄存器入口

    if (gpio == NULL) // 检查端口是否有效
    {
        return 0U; // 非法端口时返回 0
    }

    return (uint16_t)(gpio->IDR & 0xFFFFUL); // 只返回低 16 位有效输入值
}

uint16_t dri_ll_gpio_read_output_raw(dri_ll_gpio_port_t port) // 读取整个端口的 ODR 原始值
{
    dri_ll_gpio_reg_t* gpio = NULL; // 保存目标 GPIO 寄存器入口

    gpio = dri_ll_gpio_get_reg(port); // 获取目标端口的寄存器入口

    if (gpio == NULL) // 检查端口是否有效
    {
        return 0U; // 非法端口时返回 0
    }

    return (uint16_t)(gpio->ODR & 0xFFFFUL); // 只返回低 16 位有效输出值
}

void dri_ll_gpio_write_output_raw(dri_ll_gpio_port_t port, // 直接写入整个端口的 ODR 值
                                  uint16_t           value)
{
    dri_ll_gpio_reg_t* gpio = NULL; // 保存目标 GPIO 寄存器入口

    gpio = dri_ll_gpio_get_reg(port); // 获取目标端口的寄存器入口

    if (gpio == NULL) // 检查端口是否有效
    {
        return; // 非法端口时直接返回
    }

    gpio->ODR = (uint32_t)value; // 使用给定的 16 位值覆盖整个端口输出
}

void dri_ll_gpio_port_clock_enable(dri_ll_gpio_port_t port) // 打开指定端口时钟
{
    dri_ll_gpio_clock_enable_raw(port); // 复用基础操作层的时钟开启逻辑
}

void dri_ll_gpio_init(const dri_ll_gpio_init_t* config) // 初始化一个 GPIO 引脚
{
    uint16_t pin_mask = 0U; // 保存目标引脚的位掩码

    if (config == NULL) // 检查初始化参数指针是否为空
    {
        return; // 空指针时直接返回
    }

    pin_mask = dri_ll_gpio_get_pin_mask(config->pin); // 根据引脚编号生成位掩码

    if (pin_mask == 0U) // 检查引脚是否有效
    {
        return; // 非法引脚时直接返回
    }

    dri_ll_gpio_clock_enable_raw(config->port); // 初始化前先打开目标 GPIO 端口时钟

    if (dri_ll_gpio_mode_is_input_pull(config->mode) != 0U) // 如果当前模式为上拉/下拉输入
    {
        if (config->pull == DRI_LL_GPIO_PULL_UP) // 判断当前输入模式是否需要上拉
        {
            dri_ll_gpio_write_mask_raw(config->port, pin_mask, 0U); // 通过 ODR 对应位选择上拉
        }
        else // 其余情况统一按下拉处理
        {
            dri_ll_gpio_write_mask_raw(config->port, 0U, pin_mask); // 通过 ODR 对应位选择下拉
        }
    }
    else if (dri_ll_gpio_mode_is_output(config->mode) != 0U) // 如果当前模式属于输出模式
    {
        if (config->initial_level == DRI_LL_GPIO_LEVEL_HIGH) // 判断输出初始电平是否为高
        {
            dri_ll_gpio_write_mask_raw(config->port, pin_mask,
                                       0U); // 先写高电平再切换到输出模式以减少毛刺
        }
        else // 其余情况统一按低电平初始化
        {
            dri_ll_gpio_write_mask_raw(config->port, 0U,
                                       pin_mask); // 先写低电平再切换到输出模式以减少毛刺
        }
    }
    else // 其余输入模式无需预装载 ODR
    {
    }

    dri_ll_gpio_config_pin_raw(config->port, config->pin,
                               (uint32_t)config->mode); // 最后把模式写入 CRL 或 CRH
}

void dri_ll_gpio_write_pin(dri_ll_gpio_port_t port, // 写一个 GPIO 引脚电平
                           dri_ll_gpio_pin_t pin, dri_ll_gpio_level_t level)
{
    uint16_t pin_mask = 0U; // 保存目标引脚的位掩码

    pin_mask = dri_ll_gpio_get_pin_mask(pin); // 根据引脚编号生成位掩码

    if (pin_mask == 0U) // 检查引脚是否有效
    {
        return; // 非法引脚时直接返回
    }

    if (level == DRI_LL_GPIO_LEVEL_HIGH) // 判断目标电平是否为高
    {
        dri_ll_gpio_write_mask_raw(port, pin_mask, 0U); // 通过 BSRR 将对应引脚置高
    }
    else // 其余情况统一按低电平处理
    {
        dri_ll_gpio_write_mask_raw(port, 0U, pin_mask); // 通过 BSRR 将对应引脚置低
    }
}

void dri_ll_gpio_set_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin) // 将一个 GPIO 引脚置高
{
    dri_ll_gpio_write_pin(port, pin, DRI_LL_GPIO_LEVEL_HIGH); // 复用单引脚写接口输出高电平
}

void dri_ll_gpio_reset_pin(dri_ll_gpio_port_t port, dri_ll_gpio_pin_t pin) // 将一个 GPIO 引脚置低
{
    dri_ll_gpio_write_pin(port, pin, DRI_LL_GPIO_LEVEL_LOW); // 复用单引脚写接口输出低电平
}

void dri_ll_gpio_toggle_pin(dri_ll_gpio_port_t port,
                            dri_ll_gpio_pin_t  pin) // 翻转一个 GPIO 引脚输出状态
{
    uint16_t pin_mask       = 0U; // 保存目标引脚的位掩码
    uint16_t current_output = 0U; // 保存当前端口输出寄存器快照

    pin_mask = dri_ll_gpio_get_pin_mask(pin); // 根据引脚编号生成位掩码

    if (pin_mask == 0U) // 检查引脚是否有效
    {
        return; // 非法引脚时直接返回
    }

    current_output = dri_ll_gpio_read_output_raw(port); // 读取当前端口输出寄存器状态

    if ((current_output & pin_mask) != 0U) // 判断当前引脚是否已经为高电平
    {
        dri_ll_gpio_write_mask_raw(port, 0U, pin_mask); // 当前为高时将其复位为低
    }
    else // 当前不为高电平时进入此分支
    {
        dri_ll_gpio_write_mask_raw(port, pin_mask, 0U); // 当前为低时将其置位为高
    }
}

dri_ll_gpio_level_t
dri_ll_gpio_read_input_pin(dri_ll_gpio_port_t port, // 读取一个 GPIO 引脚输入电平
                           dri_ll_gpio_pin_t  pin)
{
    uint16_t pin_mask = 0U; // 保存目标引脚的位掩码

    pin_mask = dri_ll_gpio_get_pin_mask(pin); // 根据引脚编号生成位掩码

    if (pin_mask == 0U) // 检查引脚是否有效
    {
        return DRI_LL_GPIO_LEVEL_LOW; // 非法引脚默认返回低电平
    }

    return (((dri_ll_gpio_read_input_raw(port) & pin_mask) != 0U)
                ? DRI_LL_GPIO_LEVEL_HIGH
                : DRI_LL_GPIO_LEVEL_LOW); // 将原始输入位转换成逻辑电平
}

dri_ll_gpio_level_t
dri_ll_gpio_read_output_pin(dri_ll_gpio_port_t port, // 读取一个 GPIO 引脚输出电平
                            dri_ll_gpio_pin_t  pin)
{
    uint16_t pin_mask = 0U; // 保存目标引脚的位掩码

    pin_mask = dri_ll_gpio_get_pin_mask(pin); // 根据引脚编号生成位掩码

    if (pin_mask == 0U) // 检查引脚是否有效
    {
        return DRI_LL_GPIO_LEVEL_LOW; // 非法引脚默认返回低电平
    }

    return (((dri_ll_gpio_read_output_raw(port) & pin_mask) != 0U)
                ? DRI_LL_GPIO_LEVEL_HIGH
                : DRI_LL_GPIO_LEVEL_LOW); // 将原始输出位转换成逻辑电平
}

uint16_t dri_ll_gpio_read_input_port(dri_ll_gpio_port_t port) // 读取整个端口输入值
{
    return dri_ll_gpio_read_input_raw(port); // 直接返回整个端口的输入寄存器值
}

uint16_t dri_ll_gpio_read_output_port(dri_ll_gpio_port_t port) // 读取整个端口输出值
{
    return dri_ll_gpio_read_output_raw(port); // 直接返回整个端口的输出寄存器值
}

void dri_ll_gpio_write_port(dri_ll_gpio_port_t port, uint16_t value) // 写整个端口输出值
{
    dri_ll_gpio_write_output_raw(port, value); // 直接写入整个端口的输出寄存器
}
