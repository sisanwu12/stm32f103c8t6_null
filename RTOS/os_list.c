/**
 * @file os_list.c
 * @author Yukikaze
 * @brief RTOS 双向链表实现文件。
 * @version 0.1
 * @date 2026-03-25
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件实现内核核心共用的基础链表操作。
 */
#include <stddef.h>
#include "os_list.h"

/**
 * @brief 初始化一个独立的链表节点。
 *
 * @param node 待初始化的链表节点指针。
 */
void list_node_init(list_node_t *node)
{
    node->prev = NULL;
    node->next = NULL;
}

/**
 * @brief 初始化一个空链表。
 *
 * @param list 待初始化的链表对象。
 */
void list_init(list_t *list)
{
    list->head       = NULL;
    list->tail       = NULL;
    list->item_count = 0U;
}

/**
 * @brief 将一个节点插入到链表头部。
 *
 * @param list 目标链表。
 * @param node 待插入的链表节点。
 */
void list_insert_head(list_t *list, list_node_t *node)
{
    node->prev = NULL;
    node->next = list->head;

    if (list->head != NULL)
    {
        list->head->prev = node;
    }
    else
    {
        list->tail = node;
    }

    list->head = node;
    list->item_count++;
}

/**
 * @brief 将一个节点插入到链表尾部。
 *
 * @param list 目标链表。
 * @param node 待插入的链表节点。
 */
void list_insert_tail(list_t *list, list_node_t *node)
{
    node->prev = list->tail;
    node->next = NULL;

    if (list->tail != NULL)
    {
        list->tail->next = node;
    }
    else
    {
        list->head = node;
    }

    list->tail = node;
    list->item_count++;
}

/**
 * @brief 将指定节点从链表中移除。
 *
 * @param list 目标链表。
 * @param node 待移除的链表节点。
 */
void list_remove(list_t *list, list_node_t *node)
{
    if (node->prev != NULL)
    {
        node->prev->next = node->next;
    }
    else
    {
        list->head = node->next;
    }

    if (node->next != NULL)
    {
        node->next->prev = node->prev;
    }
    else
    {
        list->tail = node->prev;
    }

    node->prev = NULL;
    node->next = NULL;

    if (list->item_count > 0U)
    {
        list->item_count--;
    }
}

/**
 * @brief 移除并返回链表头节点。
 *
 * @param list 目标链表。
 *
 * @return list_node_t* 被移除的头节点；若链表为空则返回 NULL。
 */
list_node_t *list_remove_head(list_t *list)
{
    list_node_t *node = list->head;

    if (node != NULL)
    {
        list_remove(list, node);
    }

    return node;
}

/**
 * @brief 判断链表是否为空。
 *
 * @param list 待判断的链表。
 *
 * @return uint8_t 非 0 表示空链表，0 表示非空。
 */
uint8_t list_is_empty(const list_t *list)
{
    return (uint8_t)(list->item_count == 0U);
}
