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
    # 使用 Linux 内核风格的蓝色背景 + 白色高亮主题
    # 基于 aquatic，把列表改为蓝底白字，选中项改为白底黑字
    os.environ["MENUCONFIG_STYLE"] = (
        "aquatic "
        "list=fg:white,bg:blue "
        "selection=fg:black,bg:white,bold "
        "inv-list=fg:white,bg:blue "
        "inv-selection=fg:black,bg:white "
        "show-help=fg:white,bg:blue "
        "text=fg:white,bg:blue"
    )

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
