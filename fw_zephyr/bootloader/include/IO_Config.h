/*
 * DAPLink IO configuration for the Zephyr bootloader.
 *
 * Pins come from the devicetree (common/boards overlay), see src/gpio.c.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __IO_CONFIG_H__
#define __IO_CONFIG_H__

#include <soc.h>
#include "daplink.h"

COMPILER_ASSERT(DAPLINK_HIC_ID == DAPLINK_HIC_ID_STM32F103XB);

#endif
