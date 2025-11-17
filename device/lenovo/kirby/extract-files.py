#!/usr/bin/env python3
#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

import os
import sys
from pathlib import Path

def main():
    vendor = Path(__file__).parent / "proprietary-files.txt"
    if not vendor.exists():
        print(f"Error: {vendor} not found", file=sys.stderr)
        sys.exit(1)

    with open(vendor, "r", encoding="utf-8") as f:
        files = [line.strip() for line in f.readlines() if line.strip() and not line.startswith("#")]

    if not files:
        print("No files to extract", file=sys.stderr)
        sys.exit(1)

    vendor_path = Path(__file__).parent.parent.parent.parent / "vendor" / "lenovo" / "kirby"
    vendor_path.mkdir(parents=True, exist_ok=True)

    for file in files:
        if file.startswith("-"):
            continue
        if "|" in file:
            file = file.split("|")[0]
        file = file.strip()
        if not file:
            continue

        dest = vendor_path / file
        dest.parent.mkdir(parents=True, exist_ok=True)

        print(f"Extracting {file}...")
        # 尝试多个可能的路径
        result = os.system(f"adb pull /system/{file} {dest} 2>/dev/null")
        if result != 0:
            result = os.system(f"adb pull /vendor/{file} {dest} 2>/dev/null")
        if result != 0:
            result = os.system(f"adb pull /system/vendor/{file} {dest} 2>/dev/null")
        if result != 0:
            result = os.system(f"adb pull /product/{file} {dest} 2>/dev/null")
        if result != 0:
            result = os.system(f"adb pull /system_ext/{file} {dest} 2>/dev/null")
        if result != 0:
            print(f"  ⚠️  无法提取: {file}")

if __name__ == "__main__":
    main()

