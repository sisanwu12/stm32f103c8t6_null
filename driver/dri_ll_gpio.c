/**
 * @file dri_ll_gpio.c
 * @author BEAM
 * @brief STM32F103 GPIO 底层驱动实现文件。
 * @note 本实现遵循 STM32F1 的 CRL/CRH 配置模型，用户层采用输入模式 / 输出模式 / 输出速度三枚举组合配置。
 *       在 Cortex-M 目标上，GPIO 翻转操作会在短临界区内完成，以降低与中断并发访问时的状态竞争风险。
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "dri_ll_gpio.h" // 引入 GPIO LL 对外接口

#define DRI_LL_GPIO_CFG_RESERVED_INPUT (0xCU) // STM32F1 手册中的保留输入位型，raw 接口禁止写入

static uint8_t dri_ll_gpio_is_valid_port(dri_ll_gpio_port_t port) // 判断端口编号是否合法
{
    int32_t port_value = (int32_t)port; // 将端口枚举转换成有符号整数便于做边界检查

    return (uint8_t)((port_value >= (int32_t)DRI_LL_GPIO_PORT_A) && // 端口下界限制为 A
                     (port_value <= (int32_t)DRI_LL_GPIO_PORT_E));  // 端口上界限制为 E
}

static uint8_t dri_ll_gpio_is_valid_pin(dri_ll_gpio_pin_t pin) // 判断引脚编号是否合法
{
    int32_t pin_value = (int32_t)pin; // 将引脚枚举转换成有符号整数便于做边界检查

    return (uint8_t)((pin_value >= (int32_t)DRI_LL_GPIO_PIN_0) &&   // 引脚下界限制为 0
                     (pin_value < (int32_t)DRI_LL_GPIO_PIN_COUNT)); // 引脚上界限制为 15
}

static uint8_t
dri_ll_gpio_is_valid_input_mode(dri_ll_gpio_input_mode_t input_mode) // 判断 GPIO 输入模式是否合法
{
    int32_t input_mode_value = (int32_t)input_mode; // 将输入模式枚举转换成有符号整数便于做边界检查

    return (uint8_t)((input_mode_value >= (int32_t)DRI_LL_GPIO_INPUT_MODE_NONE) && // 输入模式下界限制为 NONE
                     (input_mode_value <=
                      (int32_t)DRI_LL_GPIO_INPUT_MODE_PULL_DOWN)); // 输入模式上界限制为下拉输入
}

static uint8_t
dri_ll_gpio_is_valid_output_mode(dri_ll_gpio_output_mode_t output_mode) // 判断 GPIO 输出模式是否合法
{
    int32_t output_mode_value = (int32_t)output_mode; // 将输出模式枚举转换成有符号整数便于做边界检查

    return (uint8_t)((output_mode_value >=
                      (int32_t)DRI_LL_GPIO_OUTPUT_MODE_NONE) && // 输出模式下界限制为 NONE
                     (output_mode_value <=
                      (int32_t)DRI_LL_GPIO_OUTPUT_MODE_AF_OD)); // 输出模式上界限制为复用开漏
}

static uint8_t dri_ll_gpio_is_valid_speed(dri_ll_gpio_speed_t speed) // 判断 GPIO 输出速度是否合法
{
    int32_t speed_value = (int32_t)speed; // 将速度枚举转换成有符号整数便于做边界检查

    return (uint8_t)((speed_value >= (int32_t)DRI_LL_GPIO_SPEED_NONE) && // 速度下界限制为 NONE
                     (speed_value <= (int32_t)DRI_LL_GPIO_SPEED_50MHZ)); // 速度上界限制为 50MHz
}

static uint8_t
dri_ll_gpio_is_valid_level(dri_ll_gpio_level_t level) // 判断 GPIO 输出电平是否合法
{
    int32_t level_value = (int32_t)level; // 将电平枚举转换成有符号整数便于做边界检查

    return (uint8_t)((level_value >= (int32_t)DRI_LL_GPIO_LEVEL_LOW) && // 电平下界限制为低电平
                     (level_value <= (int32_t)DRI_LL_GPIO_LEVEL_HIGH)); // 电平上界限制为高电平
}

static uint8_t dri_ll_gpio_is_valid_cfg_bits(uint32_t cfg_bits) // 判断基础层 4bit 配置字段是否合法
{
    return (uint8_t)((cfg_bits <= 0xFUL) && // 配置字段必须落在 4bit 范围内
                     (cfg_bits != DRI_LL_GPIO_CFG_RESERVED_INPUT)); // 禁止将保留模式 0xC 写入寄存器
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

static uint8_t
dri_ll_gpio_encode_speed_bits(dri_ll_gpio_speed_t speed,
                              uint32_t*          speed_bits) // 将速度枚举编码成 STM32F1 MODE[1:0]
{
    if ((speed_bits == NULL) || (dri_ll_gpio_is_valid_speed(speed) == 0U)) // 校验输出参数和速度枚举是否合法
    {
        return 0U; // 非法输入时返回失败
    }

    switch (speed) // 按用户层速度枚举转换成 MODE[1:0]
    {
    case DRI_LL_GPIO_SPEED_NONE: // NONE 仅用于输入模式，不可编码成输出速度
        return 0U;               // 直接返回失败

    case DRI_LL_GPIO_SPEED_2MHZ: // 2MHz 输出速度
        *speed_bits = 0x2U;      // MODE[1:0] = 10
        return 1U;               // 编码成功

    case DRI_LL_GPIO_SPEED_10MHZ: // 10MHz 输出速度
        *speed_bits = 0x1U;       // MODE[1:0] = 01
        return 1U;                // 编码成功

    case DRI_LL_GPIO_SPEED_50MHZ: // 50MHz 输出速度
        *speed_bits = 0x3U;       // MODE[1:0] = 11
        return 1U;                // 编码成功

    default:       // 非法速度分支
        return 0U; // 未覆盖的速度值一律判定失败
    }
}

static uint8_t dri_ll_gpio_encode_user_mode(
    const dri_ll_gpio_init_t* config,
    uint32_t*                 cfg_bits) // 将用户层组合式配置编码成 STM32F1 4bit 配置字段
{
    uint32_t speed_bits    = 0U; // 保存编码后的 MODE[1:0] 速度位
    uint8_t  input_active  = 0U; // 标记当前是否启用了输入模式
    uint8_t  output_active = 0U; // 标记当前是否启用了输出模式

    if ((config == NULL) || (cfg_bits == NULL)) // 检查输入参数和输出参数是否为空
    {
        return 0U; // 空指针时直接返回失败
    }

    if ((dri_ll_gpio_is_valid_input_mode(config->input_mode) == 0U) ||   // 检查输入模式是否合法
        (dri_ll_gpio_is_valid_output_mode(config->output_mode) == 0U)) // 检查输出模式是否合法
    {
        return 0U; // 任一模式枚举非法时直接返回失败
    }

    input_active  = (uint8_t)(config->input_mode != DRI_LL_GPIO_INPUT_MODE_NONE);  // 判断是否启用了输入模式
    output_active = (uint8_t)(config->output_mode != DRI_LL_GPIO_OUTPUT_MODE_NONE); // 判断是否启用了输出模式

    if (input_active == output_active) // 输入和输出必须二选一，不能同时启用也不能同时关闭
    {
        return 0U; // 歧义配置直接返回失败
    }

    if (input_active != 0U) // 输入模式分支
    {
        switch (config->input_mode) // 按输入模式选择对应 CNF/MODE 组合
        {
        case DRI_LL_GPIO_INPUT_MODE_NONE: // NONE 不应进入输入激活分支
            return 0U;                    // 直接返回失败

        case DRI_LL_GPIO_INPUT_MODE_ANALOG: // 模拟输入
            *cfg_bits = 0x0U;               // CNF[1:0]=00, MODE[1:0]=00
            return 1U;                      // 编码成功

        case DRI_LL_GPIO_INPUT_MODE_FLOATING: // 浮空输入
            *cfg_bits = 0x4U;                 // CNF[1:0]=01, MODE[1:0]=00
            return 1U;                        // 编码成功

        case DRI_LL_GPIO_INPUT_MODE_PULL_UP: // 上拉输入
        case DRI_LL_GPIO_INPUT_MODE_PULL_DOWN: // 下拉输入
            *cfg_bits = 0x8U; // CNF[1:0]=10, MODE[1:0]=00
            return 1U;        // 编码成功

        default:       // 未覆盖的输入模式分支
            return 0U; // 直接返回失败
        }
    }

    if ((dri_ll_gpio_is_valid_level(config->initial_level) == 0U) ||        // 检查初始电平是否合法
        (dri_ll_gpio_encode_speed_bits(config->speed, &speed_bits) == 0U)) // 编码输出速度
    {
        return 0U; // 任一输出参数非法时直接返回失败
    }

    switch (config->output_mode) // 根据输出模式选择普通输出或复用输出的 CNF 位
    {
    case DRI_LL_GPIO_OUTPUT_MODE_NONE: // NONE 不应进入输出激活分支
        return 0U;                     // 直接返回失败

    case DRI_LL_GPIO_OUTPUT_MODE_GPIO_PP: // 通用推挽输出
        *cfg_bits = speed_bits;           // 组合得到 0x1/0x2/0x3
        return 1U;                        // 编码成功

    case DRI_LL_GPIO_OUTPUT_MODE_GPIO_OD: // 通用开漏输出
        *cfg_bits = (uint32_t)(speed_bits | 0x4U); // 组合得到 0x5/0x6/0x7
        return 1U;                                 // 编码成功

    case DRI_LL_GPIO_OUTPUT_MODE_AF_PP: // 复用推挽输出
        *cfg_bits = (uint32_t)(speed_bits | 0x8U); // 组合得到 0x9/0xA/0xB
        return 1U;                                 // 编码成功

    case DRI_LL_GPIO_OUTPUT_MODE_AF_OD: // 复用开漏输出
        *cfg_bits = (uint32_t)(speed_bits | 0xCU); // 组合得到 0xD/0xE/0xF
        return 1U;                                 // 编码成功

    default:       // 未覆盖的输出模式分支
        return 0U; // 直接返回失败
    }
}

static uint32_t dri_ll_gpio_enter_critical(void) // 保存中断状态并进入临界区
{
#if defined(__GNUC__) && \
    (defined(__arm__) || defined(__thumb__)) // 仅在 GNU ARM 目标上启用关中断保护
    uint32_t primask = 0UL;                  // 保存进入临界区前的 PRIMASK 状态

    __asm volatile("mrs %0, primask" : "=r"(primask) : : "memory"); // 读取当前 PRIMASK 状态
    __asm volatile("cpsid i" : : : "memory");                       // 关闭全局中断保护读改写窗口

    return primask; // 返回原始中断状态供退出时恢复
#else               // 非 ARM 目标仅保留空实现以便宿主机做语法检查
    return 0UL; // 在宿主机环境中返回默认值
#endif              // 结束临界区实现选择
}

static void dri_ll_gpio_exit_critical(uint32_t primask) // 恢复进入临界区前的中断状态
{
#if defined(__GNUC__) && (defined(__arm__) || defined(__thumb__)) // 仅在 GNU ARM 目标上恢复 PRIMASK
    __asm volatile("msr primask, %0"
                   :
                   : "r"(primask)
                   : "memory"); // 恢复进入临界区前的 PRIMASK 状态
#else                           // 非 ARM 目标仅抑制未使用参数警告
    (void)primask; // 宿主机环境下不需要真正恢复中断状态
#endif                          // 结束临界区实现选择
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

void dri_ll_gpio_config_pin_raw(dri_ll_gpio_port_t port, // 直接写入指定引脚的 4bit 配置字段
                                dri_ll_gpio_pin_t pin, uint32_t cfg_bits)
{
    dri_ll_gpio_reg_t* gpio    = NULL; // 保存目标 GPIO 寄存器入口
    volatile uint32_t* cfg_reg = NULL; // 保存目标配置寄存器入口
    uint32_t           shift   = 0UL;  // 保存目标引脚在寄存器中的位移

    if ((dri_ll_gpio_is_valid_port(port) == 0U) ||       // 校验端口是否合法
        (dri_ll_gpio_is_valid_pin(pin) == 0U) ||         // 校验引脚是否合法
        (dri_ll_gpio_is_valid_cfg_bits(cfg_bits) == 0U)) // 校验配置字段是否合法且不是保留模式
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

void dri_ll_gpio_init(const dri_ll_gpio_init_t* config) // 初始化一个 GPIO 引脚
{
    uint16_t pin_mask = 0U; // 保存目标引脚的位掩码
    uint32_t cfg_bits = 0U; // 保存编码后的 4bit GPIO 配置字段

    if (config == NULL) // 检查初始化参数指针是否为空
    {
        return; // 空指针时直接返回
    }

    if ((dri_ll_gpio_is_valid_port(config->port) == 0U) ||       // 检查端口是否合法
        (dri_ll_gpio_is_valid_pin(config->pin) == 0U) ||         // 检查引脚是否合法
        (dri_ll_gpio_encode_user_mode(config, &cfg_bits) == 0U)) // 检查用户层组合字段并编码成 4bit 配置
    {
        return; // 任一配置参数非法时直接返回
    }

    pin_mask = dri_ll_gpio_get_pin_mask(config->pin); // 根据引脚编号生成位掩码

    if (pin_mask == 0U) // 检查引脚是否有效
    {
        return; // 非法引脚时直接返回
    }

    if (config->input_mode == DRI_LL_GPIO_INPUT_MODE_PULL_UP) // 上拉输入需要先通过 ODR 对应位选择上拉
    {
        dri_ll_gpio_write_mask_raw(config->port, pin_mask, 0U); // 通过 ODR 对应位选择上拉
    }
    else if (config->input_mode == DRI_LL_GPIO_INPUT_MODE_PULL_DOWN) // 下拉输入需要先通过 ODR 对应位选择下拉
    {
        dri_ll_gpio_write_mask_raw(config->port, 0U, pin_mask); // 通过 ODR 对应位选择下拉
    }
    else if (config->output_mode != DRI_LL_GPIO_OUTPUT_MODE_NONE) // 任意输出模式都需要预装载输出值
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
    else // 模拟输入和浮空输入无需预装载 ODR
    {
    }

    dri_ll_gpio_config_pin_raw(config->port, config->pin,
                               cfg_bits); // 最后把组合式配置编码后的 4bit 字段写入 CRL 或 CRH
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
    dri_ll_gpio_reg_t* gpio           = NULL; // 保存目标 GPIO 寄存器入口
    uint16_t           pin_mask       = 0U;   // 保存目标引脚的位掩码
    uint16_t           current_output = 0U;   // 保存当前端口输出寄存器快照
    uint32_t           primask        = 0UL;  // 保存进入临界区前的中断状态

    pin_mask = dri_ll_gpio_get_pin_mask(pin); // 根据引脚编号生成位掩码

    if ((dri_ll_gpio_is_valid_port(port) == 0U) || (pin_mask == 0U)) // 检查端口和引脚是否有效
    {
        return; // 非法引脚时直接返回
    }

    gpio = dri_ll_gpio_get_reg(port); // 获取目标端口的寄存器入口

    if (gpio == NULL) // 检查端口寄存器入口是否有效
    {
        return; // 无法获取目标端口时直接返回
    }

    primask        = dri_ll_gpio_enter_critical();     // 进入临界区保护翻转过程不被中断打断
    current_output = (uint16_t)(gpio->ODR & 0xFFFFUL); // 在临界区内读取当前端口输出寄存器状态

    if ((current_output & pin_mask) != 0U) // 判断当前引脚是否已经为高电平
    {
        gpio->BSRR = ((uint32_t)pin_mask << 16UL); // 当前为高时直接通过 BSRR 原子复位为低
    }
    else // 当前不为高电平时进入此分支
    {
        gpio->BSRR = (uint32_t)pin_mask; // 当前为低时直接通过 BSRR 原子置位为高
    }

    dri_ll_gpio_exit_critical(primask); // 退出临界区并恢复进入前的中断状态
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
