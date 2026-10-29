# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(DAPLINK_IMAGE IF)
include(${CMAKE_CURRENT_SOURCE_DIR}/../common/daplink.cmake)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(daplink_interface)

daplink_setup()

zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${DAPLINK_DIR}/cmsis-dap
  ${DAPLINK_DIR}/cmsis-driver
)

# CMSIS-DAP core, reused as-is from DAPLink
target_sources(app PRIVATE
  ${DAPLINK_DIR}/cmsis-dap/DAP.c
  ${DAPLINK_DIR}/cmsis-dap/SW_DP.c
)

# Zephyr port
target_sources(app PRIVATE
  src/main.c
  src/dap_gpio.c
  src/dap_usb.c
  src/info.c
  src/led.c
  src/bootloader.c
  src/dap_vendor.c
)
target_sources_ifdef(CONFIG_APP_USB2UART app PRIVATE src/usb2uart.c)
target_sources_ifdef(CONFIG_APP_SWO_UART app PRIVATE
  ${DAPLINK_DIR}/cmsis-dap/SWO.c
  src/swo_uart.c
)
