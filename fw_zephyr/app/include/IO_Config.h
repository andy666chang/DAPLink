/*
 * DAPLink IO configuration for the Zephyr port.
 *
 * Pin assignment comes from the devicetree "zephyr,user" node
 * (see boards/ overlay); register access is done directly on the
 * STM32F1 GPIO block for SWD bit-bang speed, as in DAPLink.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __IO_CONFIG_H__
#define __IO_CONFIG_H__

#include <zephyr/devicetree.h>
#include <soc.h>

#define DAP_USER_NODE                DT_PATH(zephyr_user)

#define DAP_GPIO_PORT(prop)          ((GPIO_TypeDef *)DT_REG_ADDR(DT_GPIO_CTLR(DAP_USER_NODE, prop)))
#define DAP_GPIO_BIT(prop)           DT_GPIO_PIN(DAP_USER_NODE, prop)

//SWD
#define SWCLK_TCK_PIN_PORT           DAP_GPIO_PORT(swclk_gpios)
#define SWCLK_TCK_PIN_Bit            DAP_GPIO_BIT(swclk_gpios)
#define SWCLK_TCK_PIN                (1U << SWCLK_TCK_PIN_Bit)

#define SWDIO_OUT_PIN_PORT           DAP_GPIO_PORT(swdio_out_gpios)
#define SWDIO_OUT_PIN_Bit            DAP_GPIO_BIT(swdio_out_gpios)
#define SWDIO_OUT_PIN                (1U << SWDIO_OUT_PIN_Bit)

#define SWDIO_IN_PIN_PORT            DAP_GPIO_PORT(swdio_in_gpios)
#define SWDIO_IN_PIN_Bit             DAP_GPIO_BIT(swdio_in_gpios)
#define SWDIO_IN_PIN                 (1U << SWDIO_IN_PIN_Bit)

// nRESET OUT Pin
#define nRESET_PIN_PORT              DAP_GPIO_PORT(nreset_gpios)
#define nRESET_PIN_Bit               DAP_GPIO_BIT(nreset_gpios)
#define nRESET_PIN                   (1U << nRESET_PIN_Bit)

#endif
