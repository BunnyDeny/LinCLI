#!/usr/bin/env python3
"""
LinCLI menuconfig frontend.
Requires: apt install python3-kconfiglib  (or: pip3 install kconfiglib)
"""
import os
import sys

# 让脚本能在未安装 kconfiglib 时给出友好提示
try:
    from kconfiglib import Kconfig
    from menuconfig import menuconfig
except ImportError as e:
    print("Error: kconfiglib or menuconfig is not installed.")
    print("Please run one of the following:")
    print("  sudo apt install python3-kconfiglib")
    print("  pip3 install kconfiglib")
    sys.exit(1)


def main():
    # 从项目根目录的 Kconfig 开始解析
    kconfig = Kconfig("Kconfig")

    dotconfig = ".config"
    if os.path.exists(dotconfig):
        kconfig.load_config(dotconfig)
        print(f"Loaded existing config: {dotconfig}")
    else:
        print("No existing .config found, using defaults.")

    # 启动交互式 TUI
    menuconfig(kconfig)

    # 保存
    kconfig.write_config(dotconfig)
    print(f"Configuration saved to {dotconfig}")


if __name__ == "__main__":
    main()
