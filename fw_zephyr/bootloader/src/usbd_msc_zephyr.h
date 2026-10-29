/*
 * DAPLink MSC class on the Zephyr legacy USB stack.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef USBD_MSC_ZEPHYR_H
#define USBD_MSC_ZEPHYR_H

#include <stdbool.h>
#include <zephyr/kernel.h>

/* Bus reset, in addition to USBD_EVT_OUT / USBD_EVT_IN */
#define USBD_MSC_EVT_RESET      (1 << 0)

/* signal is raised from the USB ISR when an event is pending */
int usbd_msc_zephyr_init(struct k_poll_signal *signal);

/* Process pending MSC events, from the main task (DAPLink USBD_Handler) */
void usbd_msc_process(void);

/* DAPLink usbd_configured() / usbd_connect() */
bool usbd_configured(void);
void usbd_connect(bool connect);

#endif
