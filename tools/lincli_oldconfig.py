#!/usr/bin/env python3
"""
LinCLI oldconfig frontend.
Updates an existing .config against the current Kconfig tree,
assigning defaults to any new symbols.
"""
import os
import sys

try:
    from kconfiglib import Kconfig
except ImportError:
    print("Error: kconfiglib is not installed.")
    print("Please run:  sudo apt install python3-kconfiglib")
    sys.exit(1)


def main():
    kconfig = Kconfig("Kconfig")

    dotconfig = ".config"
    if os.path.exists(dotconfig):
        kconfig.load_config(dotconfig)
        print(f"Loaded existing config: {dotconfig}")
    else:
        print(f"No existing {dotconfig} found, using defaults.")

    # Write back: existing values are preserved, missing symbols get defaults
    kconfig.write_config(dotconfig)
    print(f"Updated {dotconfig} with defaults for any new symbols")


if __name__ == "__main__":
    main()
