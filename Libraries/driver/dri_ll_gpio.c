/**
 * @file dri_ll_gpio.c
 * @author BEAM
 * @brief STM32F103 GPIO 底层驱动实现文件。
 * @note 本实现遵循 STM32F1 的 CRL/CRH 配置模型，用户层采用单个模式枚举加输出速度配置。 在 Cortex-M
 * 目标上，GPIO 翻转操作会在短临界区内完成，以降低与中断并发访问时的状态竞争风险。
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "dri_ll_gpio.h" // 引入 GPIO LL 对外接口
