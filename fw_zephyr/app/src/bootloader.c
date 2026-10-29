/*
 * Interface -> bootloader handover.
 *
 * DAPLink interface sets config_ram.hold_in_bl (settings.c) and resets;
 * the bootloader then stays in MAINTENANCE mode. This writes the same
 * packed record into the shared "cfgram" region (0x20004F00), read by
 * common/daplink/settings/settings.c in the bootloader.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#include "bootloader.h"

/* 'kvld' in hex - key valid, DAPLink settings.c CFG_KEY */
#define CFG_KEY             0x6b766c64
#define ALLOWED_HEXDUMP     16

/* Mirror of DAPLink settings.c cfg_ram_t, layout must not change */
typedef struct __attribute__((__packed__)) cfg_ram {
    uint32_t key;
    uint16_t size;
    uint8_t hold_in_bl;
    char assert_file_name[64 + 1];
    uint16_t assert_line;
    uint8_t assert_source;
    uint8_t  valid_dumps;
    uint32_t hexdump[ALLOWED_HEXDUMP];
    uint8_t disable_msd;
    uint8_t page_erase_enable;
} cfg_ram_t;

static cfg_ram_t config_ram __attribute__((section("cfgram")));

void bootloader_enter(void)
{
    if (config_ram.key != CFG_KEY) {
        memset(&config_ram, 0, sizeof(config_ram));
        config_ram.key = CFG_KEY;
        config_ram.size = sizeof(config_ram);
    }
    config_ram.hold_in_bl = 1;

    sys_reboot(SYS_REBOOT_COLD);
}
