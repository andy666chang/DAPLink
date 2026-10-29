/*
 * DAPLink bootloader, Zephyr port.
 *
 * Ported from DAPLink source/daplink/bootloader/main_bootloader.c:
 *   - jump to the interface (0x0800C000) when it is valid and the reset
 *     button is not pressed and the interface did not request
 *     "hold in bootloader"
 *   - otherwise expose the MAINTENANCE drive (DAPLink drag-n-drop vfs)
 *     over USB MSC to update the interface
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <cmsis_core.h>

#include "main_bootloader.h"
#include "gpio.h"
#include "validation.h"
#include "vfs_manager.h"
#include "rl_usb.h"
#include "settings.h"
#include "info.h"
#include "target_config.h"
#include "util.h"
#include "cortex_m.h"
#include "target_board.h"
#include "usbd_msc_zephyr.h"

//default msc led settings
#ifndef MSC_LED_DEF
#define MSC_LED_DEF GPIO_LED_ON
#endif

// Event flags for main task
// Timers events
#define FLAGS_MAIN_90MS         (1 << 0)
#define FLAGS_MAIN_30MS         (1 << 1)

// Timing constants (in 90mS ticks)
// USB busy time
#define USB_BUSY_TIME           (33)
// Delay before a USB device connect may occur
#define USB_CONNECT_DELAY       (11)
// Decrement to zero
#define DECZERO(x)              (x ? --x : 0)

// Global state of usb used in
main_usb_connect_t usb_state;

static uint8_t msc_led_usb_activity = 0;
static main_led_state_t msc_led_state = MAIN_LED_FLASH;

static main_usb_busy_t usb_busy;
static uint32_t usb_busy_count;

static struct k_poll_signal timer_signal = K_POLL_SIGNAL_INITIALIZER(timer_signal);
static struct k_poll_signal usb_signal = K_POLL_SIGNAL_INITIALIZER(usb_signal);
static atomic_t timer_flags;

void SystemReset(void)
{
    sys_reboot(SYS_REBOOT_COLD);
}

static void modify_stack_pointer_and_start_app(uint32_t r0_sp, uint32_t r1_pc)
{
    uint32_t z = 0;
    __ASM volatile (  "msr    control, %[z]   \n\t"
                      "isb                    \n\t"
                      "mov    sp, %[r0_sp]    \n\t"
                      "bx     %[r1_pc]"
                      :
                      :   [z] "l" (z),
                          [r0_sp] "l" (r0_sp),
                          [r1_pc] "l" (r1_pc)
                  );
}

__weak bool reset_button_pressed(void)
{
    return gpio_get_reset_btn();
}

__weak void board_bootloader_init(void)
{
    return;
}

// Timer task, set flags every 30mS and 90mS
static void timer_task_30mS(struct k_timer *timer)
{
    static uint32_t i = 0;
    uint32_t flags = FLAGS_MAIN_30MS;

    ARG_UNUSED(timer);

    if (!(i++ % 3)) {
        flags |= FLAGS_MAIN_90MS;
    }
    atomic_or(&timer_flags, flags);
    k_poll_signal_raise(&timer_signal, 0);
}

static K_TIMER_DEFINE(main_timer, timer_task_30mS, NULL);

// Flash MSC LED using 30mS tick
void main_blink_msc_led(main_led_state_t state)
{
    msc_led_usb_activity = 1;
    msc_led_state = state;
    usb_busy = MAIN_USB_ACTIVE;
    usb_busy_count = USB_BUSY_TIME;
    return;
}

static void main_task(void)
{
    // State processing
    uint32_t flags;
    // LED
    gpio_led_state_t msc_led_value = MSC_LED_DEF;
    // USB
    uint32_t usb_state_count;
    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &timer_signal),
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &usb_signal),
    };

    if (config_ram_get_initial_hold_in_bl()) {
        // Delay for 1 second for VMs
        k_msleep(100);
    }

    // Set LED defaults
    gpio_set_hid_led(GPIO_LED_OFF);
    gpio_set_cdc_led(GPIO_LED_OFF);
    gpio_set_msc_led(msc_led_value);
    // Update version information file
    info_init();
    // USB
    usbd_msc_zephyr_init(&usb_signal);
    usbd_msc_init();
    vfs_mngr_init(true);
    usbd_connect(0);
    usb_busy = MAIN_USB_IDLE;
    usb_busy_count = 0;
    usb_state = MAIN_USB_CONNECTING;
    usb_state_count = USB_CONNECT_DELAY;
    // Start timer tasks
    k_timer_start(&main_timer, K_MSEC(30), K_MSEC(30));

    while (1) {
        k_poll(events, ARRAY_SIZE(events), K_FOREVER);

        if (events[1].state == K_POLL_STATE_SIGNALED) {
            events[1].signal->signaled = 0;
            events[1].state = K_POLL_STATE_NOT_READY;
            // process usb events
            usbd_msc_process();
        }

        if (events[0].state != K_POLL_STATE_SIGNALED) {
            continue;
        }
        events[0].signal->signaled = 0;
        events[0].state = K_POLL_STATE_NOT_READY;
        flags = atomic_clear(&timer_flags);

        if (flags & FLAGS_MAIN_90MS) {
            vfs_mngr_periodic(90); // FLAGS_MAIN_90MS

            // Update USB busy status
            switch (usb_busy) {
                case MAIN_USB_ACTIVE:
                    if (DECZERO(usb_busy_count) == 0) {
                        usb_busy = MAIN_USB_IDLE;
                    }

                    break;

                case MAIN_USB_IDLE:
                default:
                    break;
            }

            // Update USB connect status
            switch (usb_state) {
                case MAIN_USB_DISCONNECTING:

                    // Wait until USB is idle before disconnecting
                    if (usb_busy == MAIN_USB_IDLE && (DECZERO(usb_state_count) == 0)) {
                        usbd_connect(0);
                        usb_state = MAIN_USB_DISCONNECTED;
                    }

                    break;

                case MAIN_USB_CONNECTING:

                    // Wait before connecting
                    if (DECZERO(usb_state_count) == 0) {
                        usbd_connect(1);
                        usb_state = MAIN_USB_CHECK_CONNECTED;
                    }

                    break;

                case MAIN_USB_CHECK_CONNECTED:
                    if (usbd_configured()) {
                        usb_state = MAIN_USB_CONNECTED;
                    }

                    break;

                case MAIN_USB_DISCONNECTED:
                    SystemReset();
                    break;

                case MAIN_USB_CONNECTED:
                default:
                    break;
            }
        }

        // 30mS tick used for flashing LED when USB is busy
        if (flags & FLAGS_MAIN_30MS) {
            if (msc_led_usb_activity) {

                if ((msc_led_state == MAIN_LED_FLASH) || (msc_led_state == MAIN_LED_FLASH_PERMANENT)) {
                    // Toggle LED value
                    msc_led_value = (GPIO_LED_ON == msc_led_value) ? GPIO_LED_OFF : GPIO_LED_ON;
                    // If in flash mode stop after one cycle but in bootloader LED stays on
                    if ((MSC_LED_DEF == msc_led_value) && (MAIN_LED_FLASH == msc_led_state)) {
                        msc_led_usb_activity = 0;
                        msc_led_state = MAIN_LED_DEF;
                    }

                }else{
                    //LED next state is MAIN_LED_DEF
                    msc_led_value = MSC_LED_DEF;
                    msc_led_usb_activity = 0;
                }

                // Update hardware
                gpio_set_msc_led(msc_led_value);
            }
        }
    }
}

int main(void)
{
    // init leds and button
    gpio_init();
    // init settings
    config_init();
    // Hook for custom boards initialisation
    board_bootloader_init();

    // check for invalid app image or rst button press. Should be checksum or CRC but NVIC validation is better than nothing.
    // If the interface has set the hold in bootloader setting don't jump to app
    if (!reset_button_pressed()
            && g_board_info.target_cfg
            && validate_bin_nvic((uint8_t *)g_board_info.target_cfg->flash_regions[0].start)
            && !config_ram_get_initial_hold_in_bl()) {
        uint32_t app = g_board_info.target_cfg->flash_regions[0].start;

        // Stop the kernel tick and all interrupts used by the bootloader
        __disable_irq();
        SysTick->CTRL = 0;
        for (int i = 0; i < ARRAY_SIZE(NVIC->ICER); i++) {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }
        // change to the new vector table
        SCB->VTOR = app; //bootloaders should only have one flash region for interface
        // Zephyr masks interrupts with BASEPRI, start the app unmasked like DAPLink
        __set_BASEPRI(0);
        __enable_irq();
        // modify stack pointer and start app
        modify_stack_pointer_and_start_app((*(uint32_t *)(app)),
                (*(uint32_t *)(app + 4)));
    }

    // either the rst pin was pressed or we have an empty app region
    main_task();

    return 0;
}
