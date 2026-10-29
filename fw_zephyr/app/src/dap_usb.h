/*
 * CMSIS-DAP v2 (WinUSB bulk) USB interface.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DAP_USB_H
#define DAP_USB_H

int dap_usb_init(void);

/* Block for the next request, execute it and send the response */
int dap_usb_process(void);

#endif
