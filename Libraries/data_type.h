/**
 * @file data_type.h
 * @author sisanwu12
 * @brief 为主流数据类型提供简写别名
 * @version 0.1
 * @date 2026-03-25
 *
 */

#ifndef __DATA_TYPE_H__
#define __DATA_TYPE_H__

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

/* ========== 基本数据类型 ========== */
/* 无符号整形 */
typedef uint8_t         u8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef uint64_t        u64;
typedef uintptr_t       uptr;
typedef const uint8_t   cu8;
typedef const uint16_t  cu16;
typedef const uint32_t  cu32;
typedef const uint64_t  cu64;
typedef const uintptr_t cuptr;

/* 有符号整形 */
typedef int8_t         s8;
typedef int16_t        s16;
typedef int32_t        s32;
typedef int64_t        s64;
typedef intptr_t       sptr;
typedef const int8_t   cs8;
typedef const int16_t  cs16;
typedef const int32_t  cs32;
typedef const int64_t  cs64;
typedef const intptr_t csptr;

/* 二值型数据类型 */
#define ENABLE  true
#define DISABLE false
typedef bool isENABLE;

#define SET   true
#define RESET false
typedef bool isSET;

typedef bool isREADY;
typedef bool isERROR;

#endif /* __DATA_TYPE_H__ */