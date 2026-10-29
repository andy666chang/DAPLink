/*
 * Minimal DAPLink target_board.h for the Zephyr port.
 *
 * The upstream dap_strings.h reads target/board strings from g_board_info.
 * The Zephyr port has no target database, so only the fields used by the
 * CMSIS-DAP core are kept.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TARGET_BOARD_H
#define TARGET_BOARD_H

typedef struct target_cfg {
    char *target_vendor;
    char *target_part_number;
} target_cfg_t;

typedef struct board_info {
    const target_cfg_t *target_cfg;
    char *board_vendor;
    char *board_name;
} board_info_t;

extern const board_info_t g_board_info;

#endif
