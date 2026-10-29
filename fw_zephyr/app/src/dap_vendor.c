/*
 * CMSIS-DAP vendor commands.
 *
 * Subset of DAPLink source/daplink/cmsis-dap/DAP_vendor.c:
 *   ID_DAP_Vendor0   GetUniqueID (DAPLink)
 *   ID_DAP_Vendor31  Enter bootloader (Zephyr port: replaces the
 *                    START_BL.ACT file of the DAPLink MSC drive)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "DAP_config.h"
#include "DAP.h"
#include "info.h"
#include "bootloader.h"

#define ID_DAP_GetUniqueID              ID_DAP_Vendor0
#define ID_DAP_EnterBootloader          ID_DAP_Vendor31

uint32_t DAP_ProcessVendorCommand(const uint8_t *request, uint8_t *response)
{
    uint32_t num = (1U << 16) | 1U;

    *response++ = *request;        // copy Command ID

    switch (*request++) {          // first byte in request is Command ID
    case ID_DAP_GetUniqueID: {
        const char *id_str = info_get_unique_id();
        uint8_t len = strlen(id_str);
        *response++ = len;
        memcpy(response, id_str, len);
        num += (len + 1); // increment response count by ID length + length byte
        break;
    }
    case ID_DAP_EnterBootloader:
        bootloader_enter();
        break;
    default:
        *(response - 1) = ID_DAP_Invalid;
        break;
    }

    return (num);
}
