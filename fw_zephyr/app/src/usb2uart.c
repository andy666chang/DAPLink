/*
 * USB CDC-ACM <-> target UART bridge.
 *
 * Replaces DAPLink usb/cdc + hic_hal/stm32/stm32f103xb/uart.c: the line
 * coding set by the host on the virtual COM port is applied to the
 * target UART (daplink,target-uart chosen node).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/sys/ring_buffer.h>

#include "led.h"
#include "usb2uart.h"

#define USB2UART_BUF_SIZE CONFIG_APP_USB2UART_BUF_SIZE

static const struct device *const cdc_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
static const struct device *const tgt_dev = DEVICE_DT_GET(DT_CHOSEN(daplink_target_uart));

struct bridge {
    const struct device *dev;
    struct bridge *peer;
    struct ring_buf rb;     /* data waiting to be sent on dev */
    uint8_t buf[USB2UART_BUF_SIZE];
};

static struct bridge cdc_bridge;
static struct bridge tgt_bridge;

static void bridge_isr(const struct device *dev, void *user_data)
{
    struct bridge *self = user_data;
    struct bridge *peer = self->peer;
    uint8_t *data;
    uint32_t len;
    int n;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            len = ring_buf_put_claim(&peer->rb, &data, USB2UART_BUF_SIZE);
            if (len == 0) {
                /* Peer buffer full, drop the oldest data */
                uint8_t dummy;

                n = uart_fifo_read(dev, &dummy, 1);
            } else {
                n = uart_fifo_read(dev, data, len);
                ring_buf_put_finish(&peer->rb, MAX(n, 0));
                if (n > 0) {
                    uart_irq_tx_enable(peer->dev);
                }
            }
            if (n > 0) {
                led_flash(LED_CDC);
            }
        }

        if (uart_irq_tx_ready(dev)) {
            len = ring_buf_get_claim(&self->rb, &data, USB2UART_BUF_SIZE);
            if (len == 0) {
                uart_irq_tx_disable(dev);
                continue;
            }
            n = uart_fifo_fill(dev, data, len);
            ring_buf_get_finish(&self->rb, MAX(n, 0));
        }
    }
}

/* Follow the host line coding (CDC SET_LINE_CODING), like DAPLink
 * uart_set_configuration(). Called from the USB stack, apply it from
 * the system work queue.
 */
static uint32_t line_coding_baudrate;

static void line_coding_handler(struct k_work *work)
{
    struct uart_config cfg;

    ARG_UNUSED(work);

    if (uart_config_get(tgt_dev, &cfg) != 0) {
        return;
    }
    cfg.baudrate = line_coding_baudrate;
    uart_configure(tgt_dev, &cfg);
}

static K_WORK_DEFINE(line_coding_work, line_coding_handler);

static void cdc_dte_rate_cb(const struct device *dev, uint32_t rate)
{
    ARG_UNUSED(dev);

    if (rate == 0) {
        return;
    }
    line_coding_baudrate = rate;
    k_work_submit(&line_coding_work);
}

int usb2uart_init(void)
{
    if (!device_is_ready(cdc_dev) || !device_is_ready(tgt_dev)) {
        return -ENODEV;
    }

    cdc_bridge.dev = cdc_dev;
    cdc_bridge.peer = &tgt_bridge;
    ring_buf_init(&cdc_bridge.rb, sizeof(cdc_bridge.buf), cdc_bridge.buf);

    tgt_bridge.dev = tgt_dev;
    tgt_bridge.peer = &cdc_bridge;
    ring_buf_init(&tgt_bridge.rb, sizeof(tgt_bridge.buf), tgt_bridge.buf);

    uart_irq_callback_user_data_set(cdc_dev, bridge_isr, &cdc_bridge);
    uart_irq_callback_user_data_set(tgt_dev, bridge_isr, &tgt_bridge);

    uart_irq_rx_enable(cdc_dev);
    uart_irq_rx_enable(tgt_dev);

    cdc_acm_dte_rate_callback_set(cdc_dev, cdc_dte_rate_cb);

    return 0;
}
