/*
 * DAPLink interface firmware, Zephyr port.
 *
 * Board: stm32_min_dev/stm32f103xb/blue (DAPLink HIC: hardware/daplink)
 *
 * Features:
 *   - CMSIS-DAP v2 over WinUSB bulk (DAP.c / SW_DP.c from DAPLink)
 *   - USB CDC-ACM <-> target UART bridge (usb2uart)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>

#include "DAP_config.h"
#include "DAP.h"
#include "info.h"
#include "led.h"
#include "dap_usb.h"
#include "usb2uart.h"

int dap_gpio_init(void);

int main(void)
{
    int ret;

    info_init();

    ret = led_init();
    if (ret) {
        return ret;
    }

    ret = dap_gpio_init();
    if (ret) {
        return ret;
    }

    DAP_Setup();

    ret = dap_usb_init();
    if (ret) {
        return ret;
    }

#if defined(CONFIG_APP_USB2UART)
    ret = usb2uart_init();
    if (ret) {
        return ret;
    }
#endif

    ret = usb_enable(NULL);
    if (ret) {
        return ret;
    }

    /* CMSIS-DAP command loop (DAPLink: main_task / hid & bulk events) */
    while (1) {
        dap_usb_process();
    }

    return 0;
}
