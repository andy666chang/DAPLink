# DAPLink Zephyr

Port of [DAPLink](https://github.com/ARMmbed/DAPLink) (stm32f103xb HIC) to Zephyr RTOS,
for the DAPLink board in `hardware/daplink`. Independent of `firmware/DAPLink`:
the reused DAPLink sources are copied to [common/daplink](common/daplink/README.md).

- Zephyr: v3.7.1
- Board: `stm32_min_dev@blue` (STM32F103CB, 8 MHz HSE)

## Layout

```
fw_zephyr/
├── app/            interface firmware (IF)  -> stm32f103xb_if
├── bootloader/     bootloader (BL)          -> stm32f103xb_bl
├── common/
│   ├── boards/     shared board overlay (pins, flash partitions, cfgram)
│   ├── daplink/    DAPLink sources (unmodified copies)
│   ├── scripts/    post_build.py (DAPLink header + CRC, *_crc.hex/.bin)
│   └── daplink.cmake
└── tools/
    ├── compile.py  build script, like DAPLink tools/progen_compile.py
    ├── script/     install / setup / release
    └── west.yml
```

## Memory map

Same as DAPLink `source/hic_hal/stm32/stm32f103xb/daplink_addr.h`, images are
interchangeable with the original DAPLink BL / IF.

| Region      | Address    | Size   |
|-------------|------------|--------|
| bootloader  | 0x08000000 | 47 KB  |
| interface   | 0x0800C000 | 79 KB  |
| config user | 0x0801FC00 | 1 KB   |
| RAM         | 0x20000000 | 19.75 KB |
| cfgram      | 0x20004F00 | 256 B, shared BL <-> IF (hold in bootloader, assert) |

## Features

| Function            | DAPLink stm32f103xb          | Zephyr port                                |
|---------------------|------------------------------|--------------------------------------------|
| BL: MAINTENANCE MSC | yes                          | yes, DAPLink vfs + usbd_msc.c as-is        |
| BL: IF update       | drag-n-drop .bin / .hex      | same                                       |
| IF: CMSIS-DAP v2    | WinUSB bulk                  | WinUSB bulk (MS OS 2.0 descriptor)         |
| IF: CMSIS-DAP v1    | HID                          | no (USB PMA 512 B limit)                   |
| IF: USB CDC <-> UART| USART2                       | USART2 (`daplink,target-uart`)             |
| IF: MSC drag-n-drop | no (removed in projects.yaml)| no                                         |
| IF: SWO UART        | USART1 RX                    | USART1 RX (`daplink,swo-uart`), 4.5 Mbaud  |
| IF: SWD             | yes                          | yes (raw GPIO register bit-bang)           |
| IF: JTAG            | no                           | no                                         |

## Pin map

| Pin  | Function                  |
|------|---------------------------|
| PB13 | TGT_SWCLK                 |
| PB14 | TGT_SWDIO (out)           |
| PB12 | TGT_SWDIO (in, via R5)    |
| PB0  | TGT_RST (open drain), BL: hold in bootloader button (RST1) |
| PA2  | USART2 TX -> TGT_RX       |
| PA3  | USART2 RX <- TGT_TX       |
| PA10 | USART1 RX <- TGT_SWO      |
| PB6  | LED blue, IF: connected   |
| PB5  | LED green, IF: CDC activity, BL: MSC activity |
| PA9  | LED red, IF: DAP activity |
| PA11/PA12 | USB D-/D+, D+ fixed 1K5 pull-up (R15) |

Defined in [common/boards/stm32_min_dev_stm32f103xb.overlay](common/boards/stm32_min_dev_stm32f103xb.overlay).

## Zephyr toolchain
Zephyr SDK 0.16.8: [link](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.16.8)

Set `ZEPHYR_SDK_INSTALL_DIR` before `setup`, default is `/home/andy/Desktop/toolchain/zephyr-sdk-0.16.8`.

## SDK install
```shell
cd fw_zephyr
source tools/script/install.sh
# Windows
tools\script\install.bat
```

## Setup
```shell
source tools/script/setup.sh
# Windows
tools\script\setup.bat
```

## Build
Same usage as DAPLink `tools/progen_compile.py`:
```shell
python tools/compile.py stm32f103xb_bl
python tools/compile.py stm32f103xb_if

# both, clean, copy to firmware/
python tools/compile.py --clean --release
source tools/script/release.sh

# interface @ 0x08000000 without bootloader
python tools/compile.py stm32f103xb_if_standalone
```

Output: `build/<project>/<project>_crc.hex/.bin/.txt` (DAPLink header in the
reserved vectors + CRC32 at the end of the region, like DAPLink `post_build_script.py`).

With west directly:
```shell
west build -p -b stm32_min_dev@blue -d build/bl bootloader
west build -p -b stm32_min_dev@blue -d build/if app
west build -p -b stm32_min_dev@blue -d build/if app -- -DEXTRA_CONF_FILE=standalone.conf
west build -p -b stm32_min_dev@blue -d build/if app -- -DEXTRA_CONF_FILE=debug.conf   # RTT log
```

Kconfig options (`app/Kconfig`): `CONFIG_APP_USB2UART`, `CONFIG_APP_SWO_UART`,
`CONFIG_APP_SWO_BUFFER_SIZE`, `CONFIG_APP_DAP_PACKET_COUNT`.

## Flash
```shell
# First time, SWD probe on J1 (IF_SWDIO / IF_SWCLK)
pyocd flash -t stm32f103rc build/stm32f103xb_bl/stm32f103xb_bl_crc.hex
pyocd flash -t stm32f103rc build/stm32f103xb_if/stm32f103xb_if_crc.hex
```

Update the interface with the bootloader:
1. Enter bootloader: hold RST1 while plugging USB, or send CMSIS-DAP vendor
   command `ID_DAP_Vendor31` (0x9F) from the interface
2. `MAINTENANCE` drive appears, copy `stm32f103xb_if_crc.bin` (or `.hex`) to it
3. The bootloader programs 0x0800C000 and restarts into the interface,
   `FAIL.TXT` is shown on error
