#!/usr/bin/env python3
"""
Load a defconfig (minimal diff) into .config (full config).
Usage: python3 tools/lincli_load_defconfig.py <defconfig_path>
"""
import sys
import os

try:
    from kconfiglib import Kconfig
except ImportError:
    print("Error: kconfiglib is not installed.")
    print("Please run:  sudo apt install python3-kconfiglib")
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        print("Usage: lincli_load_defconfig.py <defconfig_path>")
        sys.exit(1)

    defconfig_path = sys.argv[1]
    if not os.path.exists(defconfig_path):
        print(f"Error: {defconfig_path} not found")
        sys.exit(1)

    kconfig = Kconfig("Kconfig")
    # load_config with replace=True resets all symbols to defaults,
    # then applies the values from the defconfig file.
    kconfig.load_config(defconfig_path)
    kconfig.write_config(".config")
    print(f"Loaded {defconfig_path} into .config")


if __name__ == "__main__":
    main()
