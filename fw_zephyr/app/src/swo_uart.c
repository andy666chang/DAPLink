/*
 * Minimal CMSIS-Driver USART (receive only) for SWO UART capture.
 *
 * Zephyr port of DAPLink source/hic_hal/stm32/stm32f103xb/swo_uart.c:
 * the USART is still accessed through registers, clock / pinctrl / IRQ
 * come from the "daplink,swo-uart" chosen devicetree node.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/pinctrl.h>

#include "DAP_config.h"
#include "Driver_USART.h"

#if (SWO_UART != 0)

#define SWO_NODE                DT_CHOSEN(daplink_swo_uart)
#define SWO_USART               ((USART_TypeDef *)DT_REG_ADDR(SWO_NODE))
#define SWO_USART_IRQn          DT_IRQN(SWO_NODE)

PINCTRL_DT_DEFINE(SWO_NODE);

static const struct stm32_pclken swo_pclken = {
    .bus = DT_CLOCKS_CELL(SWO_NODE, bus),
    .enr = DT_CLOCKS_CELL(SWO_NODE, bits),
};

static const struct device *const clk_dev = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

static uint32_t swo_pclk;

static ARM_USART_SignalEvent_t cb_event;
static uint8_t *volatile rx_buf;
static volatile uint32_t rx_num;
static volatile uint32_t rx_cnt;
static volatile uint8_t  rx_busy;
static volatile uint8_t  rx_overflow;

static void swo_usart_isr(const void *arg);

static ARM_DRIVER_VERSION GetVersion(void)
{
    ARM_DRIVER_VERSION v = { ARM_USART_API_VERSION, ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0) };
    return v;
}

static ARM_USART_CAPABILITIES GetCapabilities(void)
{
    ARM_USART_CAPABILITIES cap = { 0 };
    cap.asynchronous = 1;
    cap.event_rx_timeout = 0;
    return cap;
}

static int32_t Initialize(ARM_USART_SignalEvent_t cb)
{
    cb_event = cb;
    rx_busy = 0;
    return ARM_DRIVER_OK;
}

static int32_t Uninitialize(void)
{
    cb_event = 0;
    return ARM_DRIVER_OK;
}

static int32_t PowerControl(ARM_POWER_STATE state)
{
    switch (state) {
        case ARM_POWER_FULL:
            if (!device_is_ready(clk_dev) ||
                clock_control_on(clk_dev, (clock_control_subsys_t)&swo_pclken) != 0 ||
                clock_control_get_rate(clk_dev, (clock_control_subsys_t)&swo_pclken,
                                       &swo_pclk) != 0) {
                return ARM_DRIVER_ERROR;
            }
            if (pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SWO_NODE),
                                    PINCTRL_STATE_DEFAULT) != 0) {
                return ARM_DRIVER_ERROR;
            }
            SWO_USART->CR1 = 0;
            NVIC_ClearPendingIRQ(SWO_USART_IRQn);
            irq_enable(SWO_USART_IRQn);
            return ARM_DRIVER_OK;
        case ARM_POWER_OFF:
            irq_disable(SWO_USART_IRQn);
            SWO_USART->CR1 = 0;
            clock_control_off(clk_dev, (clock_control_subsys_t)&swo_pclken);
            rx_busy = 0;
            return ARM_DRIVER_OK;
        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
}

static int32_t Send(const void *data, uint32_t num)
{
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t Receive(void *data, uint32_t num)
{
    unsigned int key;

    if ((data == 0) || (num == 0)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    if (rx_busy) {
        return ARM_DRIVER_ERROR_BUSY;
    }
    key = irq_lock();
    rx_buf = (uint8_t *)data;
    rx_num = num;
    rx_cnt = 0;
    rx_busy = 1;
    irq_unlock(key);
    return ARM_DRIVER_OK;
}

static int32_t Transfer(const void *data_out, void *data_in, uint32_t num)
{
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static uint32_t GetTxCount(void)
{
    return 0;
}

static uint32_t GetRxCount(void)
{
    return rx_cnt;
}

static int32_t Control(uint32_t control, uint32_t arg)
{
    uint32_t div;

    switch (control & ARM_USART_CONTROL_Msk) {
        case ARM_USART_MODE_ASYNCHRONOUS:
            if (((control & ARM_USART_DATA_BITS_Msk) != ARM_USART_DATA_BITS_8) ||
                ((control & ARM_USART_PARITY_Msk) != ARM_USART_PARITY_NONE) ||
                ((control & ARM_USART_STOP_BITS_Msk) != ARM_USART_STOP_BITS_1)) {
                return ARM_USART_ERROR_DATA_BITS;
            }
            if ((arg == 0) || (arg > SWO_UART_MAX_BAUDRATE)) {
                return ARM_USART_ERROR_BAUDRATE;
            }
            // 16x oversampling: BRR = mantissa<<4 | fraction, i.e. round(PCLK / baud)
            div = (swo_pclk + (arg / 2)) / arg;
            if (div < 16) {
                return ARM_USART_ERROR_BAUDRATE;
            }
            SWO_USART->CR1 &= ~USART_CR1_UE;
            SWO_USART->CR2 = 0;
            SWO_USART->CR3 = 0;
            SWO_USART->BRR = div;
            SWO_USART->CR1 |= USART_CR1_UE;
            return ARM_DRIVER_OK;

        case ARM_USART_CONTROL_RX:
            if (arg) {
                // Flush stale data/errors before enabling the receiver
                (void)SWO_USART->SR;
                (void)SWO_USART->DR;
                rx_overflow = 0;
                SWO_USART->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_RXNEIE;
            } else {
                SWO_USART->CR1 &= ~(USART_CR1_RE | USART_CR1_RXNEIE);
            }
            return ARM_DRIVER_OK;

        case ARM_USART_ABORT_RECEIVE:
            rx_busy = 0;
            return ARM_DRIVER_OK;

        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
}

static ARM_USART_STATUS GetStatus(void)
{
    ARM_USART_STATUS status = { 0 };
    status.rx_busy = rx_busy;
    status.rx_overflow = rx_overflow;
    return status;
}

static int32_t SetModemControl(ARM_USART_MODEM_CONTROL control)
{
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static ARM_USART_MODEM_STATUS GetModemStatus(void)
{
    ARM_USART_MODEM_STATUS status = { 0 };
    return status;
}

static void swo_usart_isr(const void *arg)
{
    uint32_t sr = SWO_USART->SR;
    uint32_t event = 0;
    uint8_t data;

    ARG_UNUSED(arg);

    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        // Reading DR after SR clears RXNE and the error flags
        data = (uint8_t)SWO_USART->DR;

        if (sr & USART_SR_ORE) {
            rx_overflow = 1;
            event |= ARM_USART_EVENT_RX_OVERFLOW;
        }
        if (sr & USART_SR_FE) {
            event |= ARM_USART_EVENT_RX_FRAMING_ERROR;
        }
        if (sr & USART_SR_PE) {
            event |= ARM_USART_EVENT_RX_PARITY_ERROR;
        }

        if (rx_busy) {
            rx_buf[rx_cnt++] = data;
            if (rx_cnt == rx_num) {
                rx_busy = 0;
                event |= ARM_USART_EVENT_RECEIVE_COMPLETE;
            }
        } else if (sr & USART_SR_RXNE) {
            rx_overflow = 1;
            event |= ARM_USART_EVENT_RX_OVERFLOW;
        }
    }

    if (event && cb_event) {
        cb_event(event);
    }
}

static int swo_uart_irq_init(void)
{
    IRQ_CONNECT(DT_IRQN(SWO_NODE), DT_IRQ(SWO_NODE, priority), swo_usart_isr, NULL, 0);
    return 0;
}

SYS_INIT(swo_uart_irq_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

ARM_DRIVER_USART Driver_USART1 = {
    GetVersion,
    GetCapabilities,
    Initialize,
    Uninitialize,
    PowerControl,
    Send,
    Receive,
    Transfer,
    GetTxCount,
    GetRxCount,
    Control,
    GetStatus,
    SetModemControl,
    GetModemStatus,
};

#endif
