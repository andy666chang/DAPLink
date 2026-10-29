/*
 * Interface -> bootloader handover.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

/* Reset into the bootloader MAINTENANCE drive, does not return */
void bootloader_enter(void);

#endif
