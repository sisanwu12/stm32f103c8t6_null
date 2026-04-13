/**
 * @file os.h
 * @author Yukikaze
 * @brief RTOS 顶层 umbrella 公共头文件。
 * @version 0.1
 * @date 2026-03-26
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本文件只负责汇总 stable public headers。
 *       具体模块语义由各自头文件承载，本文件本身不再额外声明模块接口。
 *       应用代码通常只需要 include "os.h" 即可获取当前阶段全部 stable public API。
 */

#ifndef __OS_H__
#define __OS_H__

#include "os_diag.h"
#include "os_kernel.h"
#include "os_mutex.h"
#include "os_queue.h"
#include "os_sem.h"
#include "os_task.h"
#include "os_timer.h"

#endif /* __OS_H__ */
