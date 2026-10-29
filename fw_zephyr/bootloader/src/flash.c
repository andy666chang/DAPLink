/*
 * HIC internal flash programming (DAPLink FlashPrg.h / flash_hal.h).
 *
 * Replaces hic_hal/stm32/stm32f103xb/flash.c and daplink/flash_hal.c,
 * using the Zephyr flash driver.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>

#include "flash_hal.h"
#include "daplink_addr.h"
#include "compiler.h"

static const struct device *const flash_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));

uint32_t Init(uint32_t adr, uint32_t clk, uint32_t fnc)
{
    return device_is_ready(flash_dev) ? 0 : 1;
}

uint32_t UnInit(uint32_t fnc)
{
    return (0);
}

uint32_t EraseChip(void)
{
    /* Not used: iap_flash_intf erases the update region sector by sector */
    return (1);
}

uint32_t EraseSector(uint32_t adr)
{
    return flash_erase(flash_dev, adr - DAPLINK_ROM_START, DAPLINK_SECTOR_SIZE) ? 1 : 0;
}

uint32_t ProgramPage(uint32_t adr, uint32_t sz, uint32_t *buf)
{
    return flash_write(flash_dev, adr - DAPLINK_ROM_START, buf, sz) ? 1 : 0;
}

uint32_t flash_erase_sector(uint32_t addr)
{
    return EraseSector(addr);
}

uint32_t flash_program_page(uint32_t adr, uint32_t sz, uint8_t *buf)
{
    return ProgramPage(adr, sz, (uint32_t *)buf);
}

bool flash_is_readable(uint32_t addr, uint32_t length)
{
    uint32_t end_addr = addr + length - 1;
    return (addr >= DAPLINK_ROM_START && addr < (DAPLINK_ROM_START + DAPLINK_ROM_SIZE))
        && (end_addr >= DAPLINK_ROM_START && end_addr < (DAPLINK_ROM_START + DAPLINK_ROM_SIZE));
}
