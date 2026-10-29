/*
 * DAP I/O initialization (DAP_SETUP) for the Zephyr port.
 *
 * Ported from hic_hal/stm32/stm32f103xb DAP_config.h / gpio.c: pins are
 * claimed once through the Zephyr GPIO API (clock enable + mode), the
 * CMSIS-DAP core then drives them through raw registers.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "DAP_config.h"

static const struct gpio_dt_spec swclk = GPIO_DT_SPEC_GET(DAP_USER_NODE, swclk_gpios);
static const struct gpio_dt_spec swdio_out = GPIO_DT_SPEC_GET(DAP_USER_NODE, swdio_out_gpios);
static const struct gpio_dt_spec swdio_in = GPIO_DT_SPEC_GET(DAP_USER_NODE, swdio_in_gpios);
static const struct gpio_dt_spec nreset = GPIO_DT_SPEC_GET(DAP_USER_NODE, nreset_gpios);

int dap_gpio_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&swclk) || !gpio_is_ready_dt(&swdio_out) ||
        !gpio_is_ready_dt(&swdio_in) || !gpio_is_ready_dt(&nreset)) {
        return -ENODEV;
    }

    /* Enables the GPIO port clocks and sets the default pin state */
    ret = gpio_pin_configure_dt(&swclk, GPIO_OUTPUT_HIGH);
    ret |= gpio_pin_configure_dt(&swdio_out, GPIO_OUTPUT_HIGH);
    ret |= gpio_pin_configure_dt(&swdio_in, GPIO_INPUT);
    /* reset configured as open drain output with a pullup */
    ret |= gpio_pin_configure_dt(&nreset, GPIO_OUTPUT_HIGH);

    /* Cycle counter used by TIMESTAMP_GET() */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    return ret ? -EIO : 0;
}

void DAP_SETUP(void)
{
    pin_out_init(SWCLK_TCK_PIN_PORT, SWCLK_TCK_PIN_Bit);
    SWCLK_TCK_PIN_PORT->BSRR = SWCLK_TCK_PIN;

    pin_out_init(SWDIO_OUT_PIN_PORT, SWDIO_OUT_PIN_Bit);
    SWDIO_OUT_PIN_PORT->BSRR = SWDIO_OUT_PIN;

    pin_in_init(SWDIO_IN_PIN_PORT, SWDIO_IN_PIN_Bit, 1);

    pin_out_od_init(nRESET_PIN_PORT, nRESET_PIN_Bit);
    nRESET_PIN_PORT->BSRR = nRESET_PIN;

    LED_CONNECTED_OUT(0);
}
