/**
 * @file startup_stm32f103c8t6.c
 * @author sisanwu12
 * @brief stm32f103c8t6 的启动文件。
 * @note
 * 1. 声明中断向量表；
 * 2. 实现 Reset_Handler 函数；
 * 3. 对所有中断函数实现一个空的弱实现
 *
 * @version 0.1
 * @date 2026-03-23
 *
 */

#include "data_type.h"
#include "system_stm32f103c8t6.h"

extern uptr _estack; // RAM 中栈顶地址
extern uptr _etext;  // FLASH 中程序正文结束地址

/* .data 段，包含有初始值的全局变量或静态变量 */
extern uptr _sidata; // FLASH 中 .data 的起始地址
extern uptr _sdata;  // RAM 中 .data 的起始地址
extern uptr _edata;  // RAM 中 .data 的结束地址

/* .bss 段，包含未初始化的全局变量或静态变量 */
extern uptr _sbss; // RAM 中 .bss 的开始地址
extern uptr _ebss; // RAM 中 .bss 的结束地址

int  main(void);
void Reset_Handler(void);
void Default_Handler(void);

#define WEAK_ALIAS(x) void x(void) __attribute__((weak, alias("Default_Handler")))

void Default_Handler(void)
{
    while (1)
    {
        // 只需等待
    }
}

WEAK_ALIAS(NMI_Handler);
WEAK_ALIAS(HardFault_Handler);
WEAK_ALIAS(MemManage_Handler);
WEAK_ALIAS(BusFault_Handler);
WEAK_ALIAS(UsageFault_Handler);
WEAK_ALIAS(SVC_Handler);
WEAK_ALIAS(DebugMon_Handler);
WEAK_ALIAS(PendSV_Handler);
WEAK_ALIAS(SysTick_Handler);
WEAK_ALIAS(WWDG_IRQHandler);
WEAK_ALIAS(PVD_IRQHandler);
WEAK_ALIAS(TAMPER_IRQHandler);
WEAK_ALIAS(RTC_IRQHandler);
WEAK_ALIAS(FLASH_IRQHandler);
WEAK_ALIAS(RCC_IRQHandler);
WEAK_ALIAS(EXTI0_IRQHandler);
WEAK_ALIAS(EXTI1_IRQHandler);
WEAK_ALIAS(EXTI2_IRQHandler);
WEAK_ALIAS(EXTI3_IRQHandler);
WEAK_ALIAS(EXTI4_IRQHandler);
WEAK_ALIAS(DMA1_Channel1_IRQHandler);
WEAK_ALIAS(DMA1_Channel2_IRQHandler);
WEAK_ALIAS(DMA1_Channel3_IRQHandler);
WEAK_ALIAS(DMA1_Channel4_IRQHandler);
WEAK_ALIAS(DMA1_Channel5_IRQHandler);
WEAK_ALIAS(DMA1_Channel6_IRQHandler);
WEAK_ALIAS(DMA1_Channel7_IRQHandler);
WEAK_ALIAS(ADC1_2_IRQHandler);
WEAK_ALIAS(USB_HP_CAN1_TX_IRQHandler);
WEAK_ALIAS(USB_LP_CAN1_RX0_IRQHandler);
WEAK_ALIAS(CAN1_RX1_IRQHandler);
WEAK_ALIAS(CAN1_SCE_IRQHandler);
WEAK_ALIAS(EXTI9_5_IRQHandler);
WEAK_ALIAS(TIM1_BRK_IRQHandler);
WEAK_ALIAS(TIM1_UP_IRQHandler);
WEAK_ALIAS(TIM1_TRG_COM_IRQHandler);
WEAK_ALIAS(TIM1_CC_IRQHandler);
WEAK_ALIAS(TIM2_IRQHandler);
WEAK_ALIAS(TIM3_IRQHandler);
WEAK_ALIAS(TIM4_IRQHandler);
WEAK_ALIAS(I2C1_EV_IRQHandler);
WEAK_ALIAS(I2C1_ER_IRQHandler);
WEAK_ALIAS(I2C2_EV_IRQHandler);
WEAK_ALIAS(I2C2_ER_IRQHandler);
WEAK_ALIAS(SPI1_IRQHandler);
WEAK_ALIAS(SPI2_IRQHandler);
WEAK_ALIAS(USART1_IRQHandler);
WEAK_ALIAS(USART2_IRQHandler);
WEAK_ALIAS(USART3_IRQHandler);
WEAK_ALIAS(EXTI15_10_IRQHandler);
WEAK_ALIAS(RTC_Alarm_IRQHandler);
WEAK_ALIAS(USBWakeUp_IRQHandler);

/***************** 中断向量表 *****************/
/* 中断向量表，放在 .isr_vector 段中 */
/* 在链接脚本中 .isr_vector 会被强制加载到 Flash 的最开始 */
__attribute__((section(".isr_vector"))) cuptr vector_table[] = {
    (uptr)&_estack,                   // 栈顶地址
    (uptr)Reset_Handler,              // 复位处理函数地址
    (uptr)NMI_Handler,                // NMI 处理函数地址
    (uptr)HardFault_Handler,          // HardFault 处理函数地址
    (uptr)MemManage_Handler,          // MemManage 处理函数地址
    (uptr)BusFault_Handler,           // BusFault 处理函数地址
    (uptr)UsageFault_Handler,         // UsageFault 处理函数地址
    0,                                // 保留
    0,                                // 保留
    0,                                // 保留
    0,                                // 保留
    (uptr)SVC_Handler,                // SVC 处理函数地址
    (uptr)DebugMon_Handler,           // Debug Monitor 处理函数地址
    0,                                // 保留
    (uptr)PendSV_Handler,             // PendSV 处理函数地址
    (uptr)SysTick_Handler,            // SysTick 处理函数地址
    (uptr)WWDG_IRQHandler,            // Window Watchdog 处理函数地址
    (uptr)PVD_IRQHandler,             // PVD 处理函数地址
    (uptr)TAMPER_IRQHandler,          // TAMPER 处理函数地址
    (uptr)RTC_IRQHandler,             // RTC 处理函数地址
    (uptr)FLASH_IRQHandler,           // FLASH 处理函数地址
    (uptr)RCC_IRQHandler,             // RCC 处理函数地址
    (uptr)EXTI0_IRQHandler,           // EXTI Line0 处理函数地址
    (uptr)EXTI1_IRQHandler,           // EXTI Line1 处理函数地址
    (uptr)EXTI2_IRQHandler,           // EXTI Line2 处理函数地址
    (uptr)EXTI3_IRQHandler,           // EXTI Line3 处理函数地址
    (uptr)EXTI4_IRQHandler,           // EXTI Line4 处理函数地址
    (uptr)DMA1_Channel1_IRQHandler,   // DMA1 Channel1 处理函数地址
    (uptr)DMA1_Channel2_IRQHandler,   // DMA1 Channel2 处理函数地址
    (uptr)DMA1_Channel3_IRQHandler,   // DMA1 Channel3 处理函数地址
    (uptr)DMA1_Channel4_IRQHandler,   // DMA1 Channel4 处理函数地址
    (uptr)DMA1_Channel5_IRQHandler,   // DMA1 Channel5 处理函数地址
    (uptr)DMA1_Channel6_IRQHandler,   // DMA1 Channel6 处理函数地址
    (uptr)DMA1_Channel7_IRQHandler,   // DMA1 Channel7 处理函数地址
    (uptr)ADC1_2_IRQHandler,          // ADC1 and ADC2 处理函数地址
    (uptr)USB_HP_CAN1_TX_IRQHandler,  // USB High Priority or CAN1 TX 处理函数地址
    (uptr)USB_LP_CAN1_RX0_IRQHandler, // USB Low Priority or CAN1 RX0 处理函数地址
    (uptr)CAN1_RX1_IRQHandler,        // CAN1 RX1 处理函数地址
    (uptr)CAN1_SCE_IRQHandler,        // CAN1 SCE 处理函数地址
    (uptr)EXTI9_5_IRQHandler,         // EXTI Line[9:5] 处理函数地址
    (uptr)TIM1_BRK_IRQHandler,        // TIM1 Break 处理函数地址
    (uptr)TIM1_UP_IRQHandler,         // TIM1 Update 处理函数地址
    (uptr)TIM1_TRG_COM_IRQHandler,    // TIM1 Trigger and Commutation处理函数地址
    (uptr)TIM1_CC_IRQHandler,         // TIM1 Capture Compare 处理函数地址
    (uptr)TIM2_IRQHandler,            // TIM2 处理函数地址
    (uptr)TIM3_IRQHandler,            // TIM3 处理函数地址
    (uptr)TIM4_IRQHandler,            // TIM4 处理函数地址
    (uptr)I2C1_EV_IRQHandler,         // I2C1 Event 处理函数地址
    (uptr)I2C1_ER_IRQHandler,         // I2C1 Error 处理函数地址
    (uptr)I2C2_EV_IRQHandler,         // I2C2 Event 处理函数地址
    (uptr)I2C2_ER_IRQHandler,         // I2C2 Error 处理函数地址
    (uptr)SPI1_IRQHandler,            // SPI1 处理函数地址
    (uptr)SPI2_IRQHandler,            // SPI2 处理函数地址
    (uptr)USART1_IRQHandler,          // USART1 处理函数地址
    (uptr)USART2_IRQHandler,          // USART2 处理函数地址
    (uptr)USART3_IRQHandler,          // USART3 处理函数地址
    (uptr)EXTI15_10_IRQHandler,       // EXTI Line[15:10] 处理函数地址
    (uptr)RTC_Alarm_IRQHandler,       // RTC Alarm 处理函数地址
    (uptr)USBWakeUp_IRQHandler,       // USB Wakeup from suspend 处理函数地址
    0,                                // 保留
    0,                                // 保留
    0,                                // 保留
    0,                                // 保留
    0,                                // 保留
    0,                                // 保留
    0,                                // 保留
    (uptr)0xF108F85FU,                // BootRAM 处理函数地址
};

/* 实现 Reset_Handler 函数 */
void Reset_Handler(void)
{
    // 1. 将 .data 段从 FLASH 复制到 RAM
    uptr* src = &_sidata; // FLASH 中 .data 的起始地址
    uptr* dst = &_sdata;  // RAM 中 .data 的起始地址
    while (dst < &_edata)
    {
        *dst++ = *src++;
    }

    // 2. 将 .bss 段清零
    dst = &_sbss; // RAM 中 .bss 的开始地址
    while (dst < &_ebss)
    {
        *dst++ = 0;
    }

    // 3. 系统初始化，配置系统时钟等相关设置
    SystemInit();
    // 4. 跳转到 main 函数
    main();

    // 如果 main 返回了，进入死循环
    while (1)
    {
    }
}