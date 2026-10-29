# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(DAPLINK_IMAGE BL)
include(${CMAKE_CURRENT_SOURCE_DIR}/../common/daplink.cmake)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(daplink_bootloader)

daplink_setup()

# records/usb/usb-msc.yaml, records/daplink/drag-n-drop.yaml
zephyr_compile_definitions(MSC_ENDPOINT DRAG_N_DROP_SUPPORT)

# Zephyr glue first: overrides DAPLink IO_Config.h / device.h / cmsis_os2.h
zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${DAPLINK_DIR}
  ${DAPLINK_DIR}/bootloader
  ${DAPLINK_DIR}/drag-n-drop
  ${DAPLINK_DIR}/settings
  ${DAPLINK_DIR}/target
  ${DAPLINK_DIR}/hic_hal
  ${DAPLINK_DIR}/usb
)

# DAPLink bootloader (records/daplink/bootloader.yaml, drag-n-drop.yaml,
# settings.yaml, settings_rom_stub.yaml, target_board.yaml), as-is
target_sources(app PRIVATE
  ${DAPLINK_DIR}/bootloader/daplink.c
  ${DAPLINK_DIR}/crc32.c
  ${DAPLINK_DIR}/error.c
  ${DAPLINK_DIR}/info.c
  ${DAPLINK_DIR}/util.c
  ${DAPLINK_DIR}/validation.c
  ${DAPLINK_DIR}/drag-n-drop/file_stream.c
  ${DAPLINK_DIR}/drag-n-drop/flash_decoder.c
  ${DAPLINK_DIR}/drag-n-drop/flash_intf.c
  ${DAPLINK_DIR}/drag-n-drop/flash_manager.c
  ${DAPLINK_DIR}/drag-n-drop/iap_flash_intf.c
  ${DAPLINK_DIR}/drag-n-drop/intelhex.c
  ${DAPLINK_DIR}/drag-n-drop/vfs_manager.c
  ${DAPLINK_DIR}/drag-n-drop/vfs_user.c
  ${DAPLINK_DIR}/drag-n-drop/virtual_fs.c
  ${DAPLINK_DIR}/settings/settings.c
  ${DAPLINK_DIR}/settings/settings_rom_stub.c
  ${DAPLINK_DIR}/target/target_board.c
  ${DAPLINK_DIR}/usb/usbd_msc.c
)

set_source_files_properties(
  ${DAPLINK_DIR}/bootloader/daplink.c
  PROPERTIES COMPILE_OPTIONS -Wno-unused-variable)

# Zephyr port
target_sources(app PRIVATE
  src/main_bootloader.c
  src/board.c
  src/cmsis_os2.c
  src/flash.c
  src/gpio.c
  src/read_uid.c
  src/usbd_msc_zephyr.c
)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/usb/device)
