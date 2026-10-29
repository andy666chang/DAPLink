/*
 * Minimal CMSIS-RTOS2 API on top of the Zephyr kernel.
 *
 * Only what DAPLink drag-n-drop (vfs_manager.c, file_stream.c) needs:
 * thread id and a mutex.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CMSIS_OS2_H_
#define CMSIS_OS2_H_

#include <stdint.h>
#include <zephyr/kernel.h>

typedef k_tid_t osThreadId_t;
typedef struct k_mutex *osMutexId_t;
typedef int32_t osStatus_t;

#define osOK            0
#define osError         (-1)
#define osWaitForever   0xFFFFFFFFU

static inline osThreadId_t osThreadGetId(void)
{
    return k_current_get();
}

osMutexId_t osMutexNew(const void *attr);

static inline osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout)
{
    k_timeout_t t = (timeout == osWaitForever) ? K_FOREVER : K_TICKS(timeout);

    return k_mutex_lock(mutex_id, t) ? osError : osOK;
}

static inline osStatus_t osMutexRelease(osMutexId_t mutex_id)
{
    return k_mutex_unlock(mutex_id) ? osError : osOK;
}

#endif
