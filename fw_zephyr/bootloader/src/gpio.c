/*
 * Bootloader GPIO: MSC LED and reset button (DAPLink hic_hal gpio.h).
 *
 * Ported from hic_hal/stm32/stm32f103xb/gpio.c. The target nRESET line
 * (TGT_RST, with the RST1 button) is the "hold in bootloader" button.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "gpio.h"

static const struct gpio_dt_spec msc_led = GPIO_DT_SPEC_GET(DT_NODELABEL(led_msc), gpios);
static const struct gpio_dt_spec reset_btn =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), nreset_gpios);

void gpio_init(void)
{
    gpio_pin_configure_dt(&msc_led, GPIO_OUTPUT_INACTIVE);
    /* reset button configured as input with a pullup */
    gpio_pin_configure_dt(&reset_btn, GPIO_INPUT | GPIO_PULL_UP);

    // Let the voltage rails stabilize.  This is especailly important
    // during software resets, since the target's 3.3v rail can take
    // 20-50ms to drain.  During this time the target could be driving
    // the reset pin low, causing the bootloader to think the reset
    // button is pressed.
    k_busy_wait(85 * USEC_PER_MSEC);
}

void gpio_set_board_power(bool powerEnabled)
{
}

void gpio_set_hid_led(gpio_led_state_t state)
{
}

void gpio_set_cdc_led(gpio_led_state_t state)
{
}

void gpio_set_msc_led(gpio_led_state_t state)
{
    gpio_pin_set_dt(&msc_led, state == GPIO_LED_ON);
}

uint8_t gpio_get_reset_btn_no_fwrd(void)
{
    /* Raw level: low = pressed */
    return gpio_pin_get_raw(reset_btn.port, reset_btn.pin) ? 0 : 1;
}

uint8_t gpio_get_reset_btn_fwrd(void)
{
    return 0;
}
