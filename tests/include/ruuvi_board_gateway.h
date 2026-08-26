#ifndef RUUVI_BOARD_GATEWAY_H
#define RUUVI_BOARD_GATEWAY_H
/**
 * @file ruuvi_board_gateway.h
 * @author Otso Jousimaa <otso@ojousima.net>
 * @copyright Ruuvi Innovations Ltd, License BSD-3-Clause.
 * @data 2020-06-05
 *
 * Common definitions for Ruuvi Gateway.
 */

#define RB_UART_NRF2ESP         RB_GWBUS_2 //!< UART NRF -> ESP
#define RB_UART_ESP2NRF         RB_GWBUS_1 //!< UART ESP -> NRF
#define RB_UART_ESP_RTS         RB_PIN_UNUSED
#define RB_UART_ESP_CTS         RB_PIN_UNUSED
#define RB_HWFC_ENABLED         (0U)
#define RB_PARITY_ENABLED       (0U)
#define RB_UART_BAUDRATE_9600   (0U)
#define RB_UART_BAUDRATE_115200 (1U)
#define RB_UART_BAUDRATE        RB_UART_BAUDRATE_115200

#define RB_PA_ENABLED     (1U)         //!< PA/LNA available.
#define RB_PA_CRX_PIN     RB_GWBUS_LNA //!< Shared PA/LNA control pin
#define RB_PA_CRX_TX_MODE (0U)         //!< PA/LNA is in TX mode when GPIO is low.
#define RB_PA_CRX_RX_MODE (1U)         //!< PA/LNA is in RX mode when GPIO is high.

#endif
