/**
 * @file os_queue.h
 * @author Yukikaze
 * @brief RTOS 最小消息队列接口定义文件。
 * @version 0.1
 * @date 2026-04-12
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件声明当前阶段使用的最小静态消息队列公共接口。
 */

#ifndef __OS_QUEUE_H__
#define __OS_QUEUE_H__

#include <stdint.h>
#include "os_list.h"
#include "os_types.h"

#define OS_QUEUE_MAGIC 0x51554531UL // "QUE1"，用于识别合法消息队列对象

typedef struct os_queue {
    uint32_t magic;          // 队列魔数，用于识别初始化状态
    uint8_t *buffer;         // 调用方提供的底层字节缓冲区起始地址
    uint32_t msg_size;       // 单条消息大小，单位为字节
    uint32_t capacity;       // 队列容量，表示最多可容纳多少条固定大小消息
    uint32_t count;          // 当前已经写入队列、尚未被接收的消息数量
    uint32_t head_index;     // 下一次接收应当读取的槽位下标
    uint32_t tail_index;     // 下一次发送应当写入的槽位下标
    list_t   send_wait_list; // 队列满时的发送等待链表
    list_t   recv_wait_list; // 队列空时的接收等待链表
} os_queue_t; // 最小静态消息队列对象定义

os_status_t os_queue_init(os_queue_t *queue, void *buffer, uint32_t msg_size, uint32_t capacity);
os_status_t os_queue_send(os_queue_t *queue, const void *msg, os_tick_t timeout_ticks);
os_status_t os_queue_recv(os_queue_t *queue, void *msg, os_tick_t timeout_ticks);
os_status_t os_queue_send_from_isr(os_queue_t *queue, const void *msg);
os_status_t os_queue_recv_from_isr(os_queue_t *queue, void *msg);

#endif /* __OS_QUEUE_H__ */
