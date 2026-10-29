/*
 * Status LEDs, similar to DAPLink main_blink_*_led().
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "DAP_config.h"
#include "led.h"

#define LED_FLASH_MS 30

static const struct gpio_dt_spec led_connected = GPIO_DT_SPEC_GET(DT_NODELABEL(led_connected), gpios);

static const struct gpio_dt_spec leds[LED_COUNT] = {
    [LED_DAP] = GPIO_DT_SPEC_GET(DT_NODELABEL(led_dap), gpios),
    [LED_CDC] = GPIO_DT_SPEC_GET(DT_NODELABEL(led_cdc), gpios),
};

static atomic_t led_busy;
static struct k_work_delayable led_off_work[LED_COUNT];

static void led_off_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    led_id_t led = dwork - led_off_work;

    gpio_pin_set_dt(&leds[led], 0);
    atomic_clear_bit(&led_busy, led);
}

int led_init(void)
{
    int ret = 0;

    if (!gpio_is_ready_dt(&led_connected)) {
        return -ENODEV;
    }
    ret |= gpio_pin_configure_dt(&led_connected, GPIO_OUTPUT_INACTIVE);

    for (int i = 0; i < LED_COUNT; i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            return -ENODEV;
        }
        ret |= gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        k_work_init_delayable(&led_off_work[i], led_off_handler);
    }

    return ret ? -EIO : 0;
}

void led_flash(led_id_t led)
{
    if (atomic_test_and_set_bit(&led_busy, led)) {
        return;
    }
    gpio_pin_set_dt(&leds[led], 1);
    k_work_schedule(&led_off_work[led], K_MSEC(LED_FLASH_MS));
}

void LED_CONNECTED_OUT(uint32_t bit)
{
    gpio_pin_set_dt(&led_connected, bit & 1);
}

void LED_RUNNING_OUT(uint32_t bit)
{
    (void)bit;             // Not available
}
