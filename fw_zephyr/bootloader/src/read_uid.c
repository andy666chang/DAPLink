/*
 * HIC unique ID (DAPLink hic_hal read_uid.h).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/drivers/hwinfo.h>

#include "read_uid.h"

void read_unique_id(uint32_t *id)
{
    uint32_t uid[3] = {0};

    /* STM32F1: 96 bit UID at 0x1FFFF7E8 */
    hwinfo_get_device_id((uint8_t *)uid, sizeof(uid));

    id[0] = uid[0];
    id[1] = uid[1];
    id[2] = uid[2];
    id[3] = 0xA5A5A5A5;
}
