/*
 * CMSIS-DAP hardware configuration for the Zephyr port.
 *
 * Ported from DAPLink source/hic_hal/stm32/stm32f103xb/DAP_config.h
 * HAL macros are replaced by Zephyr devicetree / Kconfig, the SWD pin
 * access stays on raw GPIO registers for speed.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __DAP_CONFIG_H__
#define __DAP_CONFIG_H__

#include <stdint.h>
#include <zephyr/kernel.h>
#include "IO_Config.h"

#define CPU_CLOCK               CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC ///< Specifies the CPU Clock in Hz

#define IO_PORT_WRITE_CYCLES    2U              ///< I/O Cycles: 2=default, 1=Cortex-M0+ fast I/0

#define DAP_SWD                 1               ///< SWD Mode:  1 = available, 0 = not available

#define DAP_JTAG                0               ///< JTAG Mode: 1 = available, 0 = not available.

#define DAP_JTAG_DEV_CNT        0               ///< Maximum number of JTAG devices on scan chain

#define DAP_DEFAULT_PORT        1               ///< Default JTAG/SWJ Port Mode: 1 = SWD, 2 = JTAG.

#define DAP_DEFAULT_SWJ_CLOCK   5000000         ///< Default SWD/JTAG clock frequency in Hz.

/// Full-speed WinUSB bulk endpoint
#define DAP_PACKET_SIZE         64              ///< Specifies Packet Size in bytes.

#define DAP_PACKET_COUNT        CONFIG_APP_DAP_PACKET_COUNT ///< Buffers: 64 = Full-Speed, 4 = High-Speed.

#if defined(CONFIG_APP_SWO_UART)
#define SWO_UART                1               ///< SWO UART:  1 = available, 0 = not available
#else
#define SWO_UART                0               ///< SWO UART:  1 = available, 0 = not available
#endif

#define SWO_UART_DRIVER         1               ///< USART Driver instance number (Driver_USART#).

#define SWO_UART_MAX_BAUDRATE   4500000U        ///< SWO UART Maximum Baudrate in Hz (PCLK2 / 16)

#define SWO_MANCHESTER          0               ///< SWO Manchester:  1 = available, 0 = not available.

#define SWO_BUFFER_SIZE         COND_CODE_1(CONFIG_APP_SWO_UART, (CONFIG_APP_SWO_BUFFER_SIZE), (4096U)) ///< SWO Trace Buffer Size in bytes (must be 2^n).

#define SWO_STREAM              0               ///< SWO Streaming Trace: 1 = available, 0 = not available.

#define TIMESTAMP_CLOCK         1000000U        ///< Timestamp clock in Hz (0 = timestamps not supported).

#define DAP_UART                0               ///< DAP UART:  1 = available, 0 = not available.

#define DAP_UART_USB_COM_PORT   0               ///< USB COM Port:  1 = available, 0 = not available.

#define TARGET_FIXED            0               ///< Target: 1 = known, 0 = unknown;


/* STM32F1 CRL/CRH 4-bit pin config: MODE[1:0] CNF[1:0] */
#define PIN_CFG_OUT_PP          0x3U            ///< Output push-pull, 50MHz
#define PIN_CFG_OUT_OD          0x7U            ///< Output open-drain, 50MHz
#define PIN_CFG_IN_PULL         0x8U            ///< Input pull-up/down
#define PIN_CFG_IN_ANALOG       0x0U            ///< Analog (high-Z)

__STATIC_FORCEINLINE void pin_cfg(GPIO_TypeDef *GPIOx, uint8_t pin_bit, uint32_t cfg)
{
    volatile uint32_t *cr = (pin_bit >= 8) ? &GPIOx->CRH : &GPIOx->CRL;
    uint32_t shift = (pin_bit & 7U) << 2;

    *cr = (*cr & ~(0xFU << shift)) | (cfg << shift);
}

__STATIC_FORCEINLINE void pin_out_init(GPIO_TypeDef *GPIOx, uint8_t pin_bit)
{
    pin_cfg(GPIOx, pin_bit, PIN_CFG_OUT_PP);
}

__STATIC_FORCEINLINE void pin_out_od_init(GPIO_TypeDef *GPIOx, uint8_t pin_bit)
{
    pin_cfg(GPIOx, pin_bit, PIN_CFG_OUT_OD);
}

/* mode: 0 = high-Z, 1 = pull-up, 2 = pull-down */
__STATIC_FORCEINLINE void pin_in_init(GPIO_TypeDef *GPIOx, uint8_t pin_bit, uint8_t mode)
{
    pin_cfg(GPIOx, pin_bit, mode ? PIN_CFG_IN_PULL : PIN_CFG_IN_ANALOG);
    if (mode == 1) {
        GPIOx->BSRR = (1U << pin_bit);
    } else if (mode == 2) {
        GPIOx->BRR = (1U << pin_bit);
    }
}

// Configure DAP I/O pins ------------------------------

__STATIC_INLINE void PORT_JTAG_SETUP(void)
{
}

__STATIC_INLINE void PORT_SWD_SETUP(void)
{
    // Set SWCLK HIGH
    pin_out_init(SWCLK_TCK_PIN_PORT, SWCLK_TCK_PIN_Bit);
    SWCLK_TCK_PIN_PORT->BSRR = SWCLK_TCK_PIN;
    // Set SWDIO HIGH
    pin_out_init(SWDIO_OUT_PIN_PORT, SWDIO_OUT_PIN_Bit);
    SWDIO_OUT_PIN_PORT->BSRR = SWDIO_OUT_PIN;

    pin_in_init(SWDIO_IN_PIN_PORT, SWDIO_IN_PIN_Bit, 1);
    // Set RESET HIGH
    pin_out_od_init(nRESET_PIN_PORT, nRESET_PIN_Bit);
    nRESET_PIN_PORT->BSRR = nRESET_PIN;
}

__STATIC_INLINE void PORT_OFF(void)
{
    pin_in_init(SWCLK_TCK_PIN_PORT, SWCLK_TCK_PIN_Bit, 0);
    pin_in_init(SWDIO_OUT_PIN_PORT, SWDIO_OUT_PIN_Bit, 0);
    pin_in_init(SWDIO_IN_PIN_PORT, SWDIO_IN_PIN_Bit, 0);
}

// SWCLK/TCK I/O pin -------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_SWCLK_TCK_IN(void)
{
    return ((SWCLK_TCK_PIN_PORT->ODR & SWCLK_TCK_PIN) ? 1 : 0);
}

__STATIC_FORCEINLINE void PIN_SWCLK_TCK_SET(void)
{
    SWCLK_TCK_PIN_PORT->BSRR = SWCLK_TCK_PIN;
}

__STATIC_FORCEINLINE void PIN_SWCLK_TCK_CLR(void)
{
    SWCLK_TCK_PIN_PORT->BRR = SWCLK_TCK_PIN;
}

// SWDIO/TMS Pin I/O --------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_SWDIO_TMS_IN(void)
{
    return ((SWDIO_IN_PIN_PORT->IDR & SWDIO_IN_PIN) ? 1 : 0);
}

__STATIC_FORCEINLINE void PIN_SWDIO_TMS_SET(void)
{
    SWDIO_OUT_PIN_PORT->BSRR = SWDIO_OUT_PIN;
}

__STATIC_FORCEINLINE void PIN_SWDIO_TMS_CLR(void)
{
    SWDIO_OUT_PIN_PORT->BRR = SWDIO_OUT_PIN;
}

__STATIC_FORCEINLINE uint32_t PIN_SWDIO_IN(void)
{
    return ((SWDIO_IN_PIN_PORT->IDR & SWDIO_IN_PIN) ? 1 : 0);
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT(uint32_t bit)
{
    if (bit & 1) {
        SWDIO_OUT_PIN_PORT->BSRR = SWDIO_OUT_PIN;
    } else {
        SWDIO_OUT_PIN_PORT->BRR = SWDIO_OUT_PIN;
    }
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT_ENABLE(void)
{
    pin_out_init(SWDIO_OUT_PIN_PORT, SWDIO_OUT_PIN_Bit);
    SWDIO_OUT_PIN_PORT->BRR = SWDIO_OUT_PIN;
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT_DISABLE(void)
{
    pin_in_init(SWDIO_OUT_PIN_PORT, SWDIO_OUT_PIN_Bit, 0);
    SWDIO_OUT_PIN_PORT->BSRR = SWDIO_OUT_PIN;
}

// TDI / TDO / nTRST: not available ------------------------

__STATIC_FORCEINLINE uint32_t PIN_TDI_IN(void)
{
    return (0);
}

__STATIC_FORCEINLINE void PIN_TDI_OUT(uint32_t bit)
{
    (void)bit;
}

__STATIC_FORCEINLINE uint32_t PIN_TDO_IN(void)
{
    return (0);
}

__STATIC_FORCEINLINE uint32_t PIN_nTRST_IN(void)
{
    return (0);
}

__STATIC_FORCEINLINE void PIN_nTRST_OUT(uint32_t bit)
{
    (void)bit;
}

// nRESET Pin I/O------------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_nRESET_IN(void)
{
    return ((nRESET_PIN_PORT->IDR >> nRESET_PIN_Bit) & 1);
}

__STATIC_FORCEINLINE void PIN_nRESET_OUT(uint32_t bit)
{
    if (bit & 1) {
        nRESET_PIN_PORT->BSRR = nRESET_PIN;
    } else {
        nRESET_PIN_PORT->BRR = nRESET_PIN;
    }
}

// LEDs (implemented in led.c with Zephyr GPIO API) --------

void LED_CONNECTED_OUT(uint32_t bit);
void LED_RUNNING_OUT(uint32_t bit);

// Timestamp ---------------------------------------------

__STATIC_INLINE uint32_t TIMESTAMP_GET(void)
{
    return (DWT->CYCCNT) / (CPU_CLOCK / TIMESTAMP_CLOCK);
}

// Initialization ----------------------------------------

/* Implemented in dap_gpio.c */
void DAP_SETUP(void);

__STATIC_INLINE uint32_t RESET_TARGET(void)
{
    return (0);              // change to '1' when a device reset sequence is implemented
}

#endif /* __DAP_CONFIG_H__ */
