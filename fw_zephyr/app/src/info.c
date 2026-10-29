/*
 * Device identification, replaces DAPLink daplink/info.c.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <app_version.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/util.h>

#include "info.h"
#include "target_board.h"

/* No target database in the Zephyr port */
const board_info_t g_board_info = {0};

/* STM32F1 UID is 96 bit */
#define UID_BYTES 12

static char unique_id[UID_BYTES * 2 + 1];

void info_init(void)
{
    uint8_t uid[UID_BYTES] = {0};
    ssize_t len;

    len = hwinfo_get_device_id(uid, sizeof(uid));
    if (len <= 0) {
        len = 0;
    }
    bin2hex(uid, len, unique_id, sizeof(unique_id));
}

const char *info_get_unique_id(void)
{
    return unique_id;
}

const char *info_get_version(void)
{
    return APP_VERSION_STRING;
}

/* USB iSerialNumber: same string as DAP_GetSerNumString() */
uint8_t *usb_update_sn_string_descriptor(void)
{
    static uint8_t sn[sizeof(CONFIG_USB_DEVICE_SN)];

    if (unique_id[0] == '\0') {
        info_init();
    }
    strncpy((char *)sn, unique_id, sizeof(sn) - 1);

    return sn;
}
