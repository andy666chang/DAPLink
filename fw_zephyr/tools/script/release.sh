
# Build bootloader + interface and copy *_crc.hex/.bin to firmware/
python tools/compile.py --clean --release stm32f103xb_bl stm32f103xb_if || return 1

ls -l firmware
