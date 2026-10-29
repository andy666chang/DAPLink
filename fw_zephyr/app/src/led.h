/*
 * Status LEDs, similar to DAPLink main_blink_*_led().
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LED_H
#define LED_H

typedef enum {
    LED_DAP = 0,
    LED_CDC,
    LED_COUNT,
} led_id_t;

int led_init(void);

/* One short flash on activity */
void led_flash(led_id_t led);

#endif
