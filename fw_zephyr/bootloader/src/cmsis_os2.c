/*
 * Minimal CMSIS-RTOS2 API on top of the Zephyr kernel.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmsis_os2.h"

/* vfs_manager.c creates a single mutex */
static struct k_mutex os_mutex_pool[1];
static uint32_t os_mutex_count;

osMutexId_t osMutexNew(const void *attr)
{
    struct k_mutex *mutex;

    ARG_UNUSED(attr);

    if (os_mutex_count >= ARRAY_SIZE(os_mutex_pool)) {
        return NULL;
    }
    mutex = &os_mutex_pool[os_mutex_count++];
    k_mutex_init(mutex);

    return mutex;
}
