# DAPLink sources

Unmodified copies from [ARMmbed/DAPLink](https://github.com/ARMmbed/DAPLink)
commit `c782a5ba`, Apache-2.0 (see [LICENSE](LICENSE)).
Shared by `app` (interface) and `bootloader`.

| Directory / file   | Upstream path                                   | Used by |
|--------------------|-------------------------------------------------|---------|
| cmsis-dap/         | source/daplink/cmsis-dap/ (DAP, SW_DP, SWO)     | IF      |
| cmsis-driver/      | source/hic_hal/cmsis-driver/                    | IF      |
| drag-n-drop/       | source/daplink/drag-n-drop/ (vfs, flash decoder)| BL      |
| settings/          | source/daplink/settings/                        | BL      |
| bootloader/        | source/daplink/bootloader/ (daplink.c, header)  | BL      |
| target/            | source/target/                                  | BL      |
| usb/               | source/usb/msc/usbd_msc.c, source/usb/usb_msc.h | BL      |
| hic_hal/           | source/hic_hal/*.h, stm32f103xb/daplink_addr.h  | BL      |
| *.c, *.h           | source/daplink/ (info, util, crc32, error, ...) | BL      |

Zephyr glue is in `../../app/{include,src}` and `../../bootloader/{include,src}`.
