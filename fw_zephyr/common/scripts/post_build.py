#!/usr/bin/env python3
#
# DAPLink image post processing for the Zephyr port.
#
# Zephyr fills the reserved Cortex-M vector slots with 0. DAPLink stores
# its image info there (source/hic_hal/.../startup_*.S):
#   0x1C  vector checksum      (post_build_script.py)
#   0x20  DAPLINK_BUILD_KEY    (BL / IF)
#   0x24  DAPLINK_HIC_ID
#   0x28  DAPLINK_VERSION
#   0x34  &g_board_info
# then the whole image is CRC32'ed into its last 4 bytes, like
# DAPLink tools/post_build_script.py, producing <output>.hex/.bin/.txt.
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import binascii
import struct

import intelhex
from elftools.elf.elffile import ELFFile

VECTOR_FMT = "<7I"
CHECKSUM_OFFSET = 0x1C
DAPLINK_INFO_OFFSET = 0x20
BOARD_INFO_OFFSET = 13 * 4


def find_symbol(elf_path, name):
    with open(elf_path, "rb") as f:
        elf = ELFFile(f)
        symtab = elf.get_section_by_name(".symtab")
        for sym in symtab.iter_symbols():
            if sym.name == name:
                return sym["st_value"]
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hex", required=True, help="zephyr.hex input")
    parser.add_argument("--elf", required=True, help="zephyr.elf, for g_board_info")
    parser.add_argument("--output", required=True, help="output file name without extension")
    parser.add_argument("--build-key", required=True, type=lambda x: int(x, 0))
    parser.add_argument("--hic-id", required=True, type=lambda x: int(x, 0))
    parser.add_argument("--version", required=True, type=lambda x: int(x, 0))
    parser.add_argument("--size", type=lambda x: int(x, 0),
                        help="pad the image to the full region size (CRC at the region end)")
    args = parser.parse_args()

    ih = intelhex.IntelHex()
    ih.padding = 0xFF
    ih.fromfile(args.hex, format="hex")

    start = ih.minaddr()
    end = ih.maxaddr()
    if args.size is not None:
        if end - start + 1 > args.size:
            raise SystemExit("image (0x%x) larger than region (0x%x)" % (end - start + 1, args.size))
        end = start + args.size - 1
        # Make the padding part of the image (filled with 0xFF)
        ih[end] = 0xFF
    else:
        # CRC is appended after the image
        end = end + 4

    # DAPLink info in the reserved vectors
    board_info = find_symbol(args.elf, "g_board_info") or 0
    ih.puts(start + DAPLINK_INFO_OFFSET,
            struct.pack("<3I", args.build_key, args.hic_id, args.version))
    ih.puts(start + BOARD_INFO_OFFSET, struct.pack("<I", board_info))

    # Checksum the first 7 vectors
    vectors = struct.unpack(VECTOR_FMT, ih.tobinarray(start=start, size=struct.calcsize(VECTOR_FMT)))
    checksum = (~sum(vectors) + 1) & 0xFFFFFFFF
    ih.puts(start + CHECKSUM_OFFSET, struct.pack("<I", checksum))

    # CRC the entire image, stored in the last 4 bytes
    size = end - start + 1
    data = ih.tobinarray(start=start, size=size - 4)
    crc32 = binascii.crc32(data) & 0xFFFFFFFF
    ih.puts(end - 3, struct.pack("<I", crc32))

    ih.tofile(args.output + ".hex", "hex")
    ih.tobinfile(args.output + ".bin", start=start, size=size)
    with open(args.output + ".txt", "w") as f:
        f.write("0x%08x\r\n" % crc32)

    print("%s: 0x%08x-0x%08x crc32 0x%08x g_board_info 0x%08x" %
          (args.output, start, end, crc32, board_info))


if __name__ == "__main__":
    main()
