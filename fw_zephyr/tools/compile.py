#!/usr/bin/env python3
#
# Build DAPLink Zephyr projects, like DAPLink tools/progen_compile.py.
#
#   python tools/compile.py                      # all projects
#   python tools/compile.py stm32f103xb_bl
#   python tools/compile.py stm32f103xb_if
#   python tools/compile.py --clean --release stm32f103xb_bl stm32f103xb_if
#
# Output (same names as DAPLink projectfiles/<tool>/<project>/build/):
#   build/<project>/<project>_crc.hex/.bin/.txt
#   --release: copied to <release-folder>/
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import shutil
import subprocess
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
WORKSPACE = os.path.dirname(TOOLS_DIR)

BOARD = "stm32_min_dev@blue"

# project name -> (source dir, extra cmake args)
PROJECTS = {
    "stm32f103xb_bl": ("bootloader", []),
    "stm32f103xb_if": ("app", []),
    # interface @ 0x08000000 without bootloader
    "stm32f103xb_if_standalone": ("app", ["-DEXTRA_CONF_FILE=standalone.conf"]),
}

DEFAULT_PROJECTS = ["stm32f103xb_bl", "stm32f103xb_if"]


def build(project, clean, verbose):
    src, extra = PROJECTS[project]
    build_dir = os.path.join(WORKSPACE, "build", project)
    cmd = ["west", "build", "-b", BOARD, "-d", build_dir, os.path.join(WORKSPACE, src)]
    if clean:
        cmd.insert(2, "-p")
    if extra:
        cmd += ["--"] + extra
    if verbose:
        print(" ".join(cmd))
    ret = subprocess.call(cmd, cwd=WORKSPACE)
    if ret != 0:
        return None
    # post_build.py names the output after the image type (bl / if)
    image = "stm32f103xb_bl" if src == "bootloader" else "stm32f103xb_if"
    return os.path.join(build_dir, image + "_crc")


def main():
    parser = argparse.ArgumentParser(
        description="DAPLink Zephyr compile support",
        epilog="List of supported projects (%d): %s" % (len(PROJECTS), ", ".join(PROJECTS)))
    parser.add_argument("projects", nargs="*", default=[],
                        help="Selectively compile only the firmware specified otherwise "
                             + ", ".join(DEFAULT_PROJECTS))
    parser.add_argument("--clean", action="store_true",
                        help="Rebuild or delete build folder before compile")
    parser.add_argument("--release", action="store_true",
                        help="Copy the images to the release folder")
    parser.add_argument("--release-folder", default="firmware",
                        help="Directory to create and place files in (default: firmware)")
    parser.add_argument("--ignore-failures", action="store_true",
                        help="Continue build even in case of failures")
    parser.add_argument("-v", dest="verbose", action="store_true")
    args = parser.parse_args()

    if shutil.which("west") is None:
        sys.exit("west not found, run: source tools/script/setup.sh")

    projects = args.projects or DEFAULT_PROJECTS
    for p in projects:
        if p not in PROJECTS:
            sys.exit("Unknown project '%s' (options: %s)" % (p, ", ".join(PROJECTS)))

    outputs = []
    for p in projects:
        out = build(p, args.clean, args.verbose)
        if out is None:
            print("Error building project %s" % p)
            if not args.ignore_failures:
                sys.exit(-1)
            continue
        outputs.append((p, out))

    if args.release:
        release_dir = os.path.join(WORKSPACE, args.release_folder)
        os.makedirs(release_dir, exist_ok=True)
        for p, out in outputs:
            for ext in (".hex", ".bin"):
                dst = os.path.join(release_dir, p + "_crc" + ext)
                shutil.copy(out + ext, dst)

    print()
    for p, out in outputs:
        print("%-28s %s.hex" % (p, os.path.relpath(out, WORKSPACE)))
    if args.release:
        print("release: %s/" % args.release_folder)


if __name__ == "__main__":
    main()
