# LinCLI Kconfig / menuconfig 迁移开发计划

> **目标读者**：另一个 Kimi 会话（AI Agent）
> **目标**：在 LinCLI 项目中引入 Kconfig + Kconfiglib（Python），使 `make menuconfig` 可交互配置项目，CMake 构建时自动消费 `.config` 生成 C 头文件。
> **约束**：最小侵入现有代码；保留 `include/cli_config.h` 作为 fallback；版本号等静态宏不动。

---

## 1. 项目现状（关键路径）

| 路径 | 作用 |
|------|------|
| `include/cli_config.h` | 手写配置头文件，包含所有 `CLI_ENABLE_XXX`、`HISTORY_MAX` 等宏 |
| `CMakeLists.txt` | 顶层 CMake，定义 `option(LINCLI_BUILD_UNITY_TESTS ...)` 等 |
| `src/CMakeLists.txt` | 编译 `lincli_obj` + `lincli` INTERFACE 库 |
| `Makefile` | 仅封装 `cmake -S . -B build && make -C build` |
| `src/cli/*.c` | CLI 核心源码，依赖 `cli_config.h` 中的宏做条件编译 |
| `src/lib/*.c` | 工具库源码 |
| `src/init/*.c` | 初始化源码 |

现有配置宏清单（需全部迁移到 Kconfig）：
- `CLI_ENABLE_USER`、`CLI_ENABLE_ENV`、`CLI_ENABLE_VAR`
- `CLI_ENABLE_ADVANCED_COMPLETION`
- `CLI_ENABLE_HELP`
- `CLI_ENABLE_CMD_CHAIN`
- `CLI_ENABLE_AUTO_RUN`
- `HISTORY_MAX` (int)
- `DISPLAY_MAX_COWS` (int)
- `CLI_CMD_BUF_SIZE` (int)
- `CLI_MPOOL_COUNT` (int)
- `CLI_MPOOL_SIZE`（恒等于 `CLI_CMD_BUF_SIZE`，不独立配置）
- `CLI_ENABLE_TESTS`（原 CMake `LINCLI_BUILD_TEST_COMMANDS` 的 C 侧开关）
- `INLINE_TEST_EN`（原 CMake `LINCLI_ENABLE_INLINE_TEST` 的 C 侧开关）

**不迁移的宏**（保留在 `cli_config.h` 中）：
- `CLI_VERSION_MAJOR`、`CLI_VERSION_MINOR`、`CLI_VERSION_PATCH`
- 版权头、文件卫士宏

---

## 2. 最终架构

```text
用户执行 make menuconfig
        ↓
Python 脚本 tools/menuconfig.py 调用 Kconfiglib
        ↓
读取 Kconfig（顶层 + src/cli/Kconfig + src/init/Kconfig + src/lib/Kconfig）
        ↓
交互式 TUI（和 Linux/RT-Thread 同款）
        ↓
生成 .config（项目根目录）
        ↓
用户执行 make build（即 cmake + make）
        ↓
CMake 检测到 .config 存在
        ↓
调用 tools/config_to_header.py 生成 build/include/cli_kconfig.h
        ↓
定义 -DCLI_KCONFIG_ENABLED，并把 build/include 加入 include path
        ↓
include/cli_config.h 在 CLI_KCONFIG_ENABLED 下 #include "cli_kconfig.h"
        ↓
源码编译，宏生效
```

---

## 3. 新建文件

### 3.1 顶层 Kconfig（`Kconfig`，放项目根目录）

```kconfig
mainmenu "LinCLI Configuration"

# 引入 src 子目录的 Kconfig
source "src/Kconfig"
```

### 3.2 src 汇总 Kconfig（`src/Kconfig`）

```kconfig
menu "LinCLI Core"

source "src/cli/Kconfig"
source "src/lib/Kconfig"
source "src/init/Kconfig"

endmenu
```

### 3.3 src/cli/Kconfig（`src/cli/Kconfig`）

```kconfig
menu "CLI Features"

config CLI_ENABLE_USER
    bool "Enable user system"
    default y
    help
      Enable user management system.

config CLI_ENABLE_ENV
    bool "Enable environment variables"
    default y
    help
      Enable environment variable get/set.

config CLI_ENABLE_VAR
    bool "Enable variable system"
    default y
    help
      Enable internal variable get/set.

config CLI_ENABLE_ADVANCED_COMPLETION
    bool "Enable advanced tab completion"
    default y
    help
      Option completion, candidate values, highlight loop.
      If disabled, only basic prefix matching + list print remains.

config CLI_ENABLE_HELP
    bool "Enable help system (--help auto-generation)"
    default y

config CLI_ENABLE_CMD_CHAIN
    bool "Enable command chaining (&&)"
    default y

config CLI_ENABLE_AUTO_RUN
    bool "Enable auto-run commands (CLI_AUTO_CMD)"
    default y

config HISTORY_MAX
    int "Command history entries"
    default 4
    range 1 64
    help
      Embedded targets should keep this small.

config DISPLAY_MAX_COWS
    int "Max columns for tab completion display"
    default 50
    range 10 200
    help
      If your terminal window is narrower than this value,
      tab completion output may be truncated.

endmenu
```

### 3.4 src/lib/Kconfig（`src/lib/Kconfig`）

```kconfig
menu "Library & Memory"

config CLI_CMD_BUF_SIZE
    int "Command argument shared buffer size"
    default 128
    range 64 1024
    help
      Increase this if you see "command argument missing bytes" errors.

config CLI_MPOOL_COUNT
    int "Memory pool block count"
    default 6
    range 1 32

config CLI_MPOOL_SIZE
    int
    default CLI_CMD_BUF_SIZE
    help
      Currently fixed equal to CLI_CMD_BUF_SIZE.

endmenu
```

### 3.5 src/init/Kconfig（`src/init/Kconfig`）

```kconfig
menu "Init & Test"

config CLI_ENABLE_INLINE_TEST
    bool "Enable scheduler inline test mode"
    default n
    help
      Prints tick count every 50 scheduler loops.
      Equivalent to old CMake -DLINCLI_ENABLE_INLINE_TEST=ON.

config CLI_ENABLE_TESTS
    bool "Enable test/demo commands"
    default y
    help
      Build interactive test commands (led, log, motor, etc.).
      Equivalent to old CMake -DLINCLI_BUILD_TEST_COMMANDS=ON.

endmenu
```

### 3.6 tools/menuconfig.py（`tools/menuconfig.py`，新建）

```python
#!/usr/bin/env python3
"""
LinCLI menuconfig frontend.
Requires: pip install kconfiglib
"""
import os
import sys

# 让脚本能在未安装 kconfiglib 时给出友好提示
try:
    from kconfiglib import Kconfig, menuconfig
except ImportError as e:
    print("Error: kconfiglib is not installed.")
    print("Please run:  pip3 install kconfiglib")
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
```

### 3.7 tools/config_to_header.py（`tools/config_to_header.py`，新建）

```python
#!/usr/bin/env python3
"""
Convert .config (key=value) to a C header.
Strips the CONFIG_ prefix so existing source macros match.
Usage: python3 tools/config_to_header.py .config build/include/cli_kconfig.h
"""
import re
import sys
import os


def convert(dotconfig_path: str, header_path: str):
    if not os.path.exists(dotconfig_path):
        print(f"Error: {dotconfig_path} not found.")
        sys.exit(1)

    lines = []
    with open(dotconfig_path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            # 格式: # CONFIG_XXX is not set
            if line.startswith("#") and "is not set" in line:
                m = re.match(r"#\s*CONFIG_(\w+)\s+is not set", line)
                if m:
                    lines.append(f"/* #undef {m.group(1)} */")
                continue

            # 格式: CONFIG_XXX=value
            m = re.match(r"CONFIG_(\w+)=(.+)", line)
            if not m:
                continue
            name, val = m.group(1), m.group(2)

            if val == "y":
                lines.append(f"#define {name} 1")
            elif val == "n":
                lines.append(f"/* #undef {name} */")
            else:
                # int, hex, string
                lines.append(f"#define {name} {val}")

    os.makedirs(os.path.dirname(header_path), exist_ok=True)

    with open(header_path, "w", encoding="utf-8") as f:
        f.write("/* Auto-generated by config_to_header.py, DO NOT EDIT */\n")
        f.write("#ifndef _CLI_KCONFIG_H_\n")
        f.write("#define _CLI_KCONFIG_H_\n\n")
        for ln in lines:
            f.write(ln + "\n")
        f.write("\n#endif /* _CLI_KCONFIG_H_ */\n")

    print(f"Generated {header_path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: config_to_header.py <.config> <output.h>")
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
```

---

## 4. 修改文件

### 4.1 Makefile

在现有 target 下方追加：

```makefile
# Kconfig / menuconfig targets
.PHONY: menuconfig oldconfig

menuconfig:
	@python3 tools/menuconfig.py

oldconfig:
	@echo "oldconfig not yet implemented (run menuconfig to generate .config)"
```

> **注意**：实际写入时 `Makefile` 中命令前必须是 **Tab**，不是空格。

### 4.2 顶层 CMakeLists.txt

在 `project(cli_project)` 之后、`add_subdirectory(src)` **之前**插入以下块：

```cmake
# ============================================================
# Kconfig integration
# ============================================================
find_program(PYTHON3_EXECUTABLE python3)
set(KCONFIG_DOT_CONFIG "${CMAKE_SOURCE_DIR}/.config")
set(KCONFIG_HEADER_DIR "${CMAKE_BINARY_DIR}/include")
set(KCONFIG_HEADER "${KCONFIG_HEADER_DIR}/cli_kconfig.h")

if(PYTHON3_EXECUTABLE AND EXISTS ${KCONFIG_DOT_CONFIG})
    message(STATUS "Found .config, generating cli_kconfig.h")
    execute_process(
        COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/config_to_header.py
                ${KCONFIG_DOT_CONFIG} ${KCONFIG_HEADER}
        RESULT_VARIABLE KCONFIG_GEN_RESULT
    )
    if(NOT KCONFIG_GEN_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate cli_kconfig.h from .config")
    endif()
    add_compile_definitions(CLI_KCONFIG_ENABLED)
    include_directories(${KCONFIG_HEADER_DIR})
elseif(NOT PYTHON3_EXECUTABLE)
    message(WARNING "python3 not found, Kconfig support disabled")
else()
    message(STATUS "No .config found, using default cli_config.h values")
endif()
```

### 4.3 include/cli_config.h

把现有内容重构为：

1. **保留**版本号、`CLI_MPOOL_SIZE` 的 `#if` 错误检查、文件卫士宏。
2. **把 Kconfig 管理的宏全部挪进 `#else` 分支**（当 `CLI_KCONFIG_ENABLED` 未定义时使用）。
3. **在 `#ifdef CLI_KCONFIG_ENABLED` 分支里只放 `#include "cli_kconfig.h"`**。

重构后的骨架如下（保留原文件的版权头和卫士宏）：

```c
/*
 * LinCLI - Framework-wide configuration macros.
 * ...（保留原有版权声明）
 */

#ifndef _CLI_CONFIG_H_
#define _CLI_CONFIG_H_

#ifdef CLI_KCONFIG_ENABLED
/* 由 Kconfig / menuconfig 生成的配置头文件 */
#include "cli_kconfig.h"
#else
/* ----------------------------------------------------------
 * 以下宏为默认值，仅在未使用 Kconfig 时生效。
 * 若使用 menuconfig，这些值会被 cli_kconfig.h 覆盖。
 * ---------------------------------------------------------- */

#ifndef CLI_ENABLE_TESTS
#define CLI_ENABLE_TESTS 1
#endif

#ifndef INLINE_TEST_EN
#define INLINE_TEST_EN 0
#endif

#define CLI_ENABLE_USER 1
#define CLI_ENABLE_ENV 1
#define CLI_ENABLE_VAR 1
#define CLI_ENABLE_ADVANCED_COMPLETION 1
#define CLI_ENABLE_HELP 1
#define CLI_ENABLE_CMD_CHAIN 1
#define CLI_ENABLE_AUTO_RUN 1

#define HISTORY_MAX 4
#define DISPLAY_MAX_COWS 50
#define CLI_CMD_BUF_SIZE 128

#define CLI_MPOOL_COUNT 6
#define CLI_MPOOL_SIZE CLI_CMD_BUF_SIZE

#endif /* CLI_KCONFIG_ENABLED */

/* ==========================================================
 * 以下宏与 Kconfig 无关，始终保留。
 * ========================================================== */
#define CLI_VERSION_MAJOR 1
#define CLI_VERSION_MINOR 12
#define CLI_VERSION_PATCH 0

#if defined(CLI_KCONFIG_ENABLED)
/* 当使用 Kconfig 时，CLI_MPOOL_SIZE 已由 cli_kconfig.h 定义，
   但保留静态检查逻辑： */
#if CLI_MPOOL_COUNT > 32
#error "CLI_MPOOL_COUNT must not exceed 32"
#endif
#else
#if CLI_MPOOL_COUNT > 32
#error "CLI_MPOOL_COUNT must not exceed 32"
#endif
#endif

#endif /* _CLI_CONFIG_H_ */
```

> **关键原则**：`CLI_KCONFIG_ENABLED` 被 CMake 定义后，`cli_kconfig.h` 会提供 `CLI_ENABLE_USER`、`HISTORY_MAX` 等宏。当用户没有 `.config` 时，fallback 到 `#else` 分支的默认值，确保项目**始终可编译**。

### 4.4 .gitignore

追加：

```gitignore
# Kconfig generated files
.config
.config.old
```

---

## 5. 可选：清理 CMake 冗余 option

原 `CMakeLists.txt` 中有：

```cmake
option(LINCLI_BUILD_UNITY_TESTS "Build Unity unit tests" ON)
option(LINCLI_BUILD_TEST_COMMANDS "Build interactive test commands (led, log, motor, etc.)" ON)
option(LINCLI_ENABLE_INLINE_TEST "Enable scheduler inline test mode (prints tick count every 50 loops)" OFF)
```

在 Kconfig 引入后，`LINCLI_BUILD_TEST_COMMANDS` 和 `LINCLI_ENABLE_INLINE_TEST` 已对应 `CLI_ENABLE_TESTS` 和 `CLI_ENABLE_INLINE_TEST`。为保持一致性，可以：

- **短期**：不动 CMake option，保持兼容。
- **长期**（本计划暂不执行）：把 `LINCLI_BUILD_TEST_COMMANDS` 和 `LINCLI_ENABLE_INLINE_TEST` 的默认值改为读取 `.config` 或从 Kconfig 头文件推导。

**本计划阶段**：保持 CMake option 不动，`CLI_ENABLE_TESTS` / `CLI_ENABLE_INLINE_TEST` 在 C 侧生效即可。若 CMake 侧要联动，可在 `src/CMakeLists.txt` 中加条件判断。

---

## 6. 实施顺序（必须按此顺序）

1. **安装依赖**：`pip3 install kconfiglib`
2. **新建 Kconfig 文件**：按 3.1~3.5 创建 `Kconfig`、`src/Kconfig`、`src/cli/Kconfig`、`src/lib/Kconfig`、`src/init/Kconfig`
3. **新建 Python 脚本**：按 3.6、3.7 创建 `tools/menuconfig.py`、`tools/config_to_header.py`，并 `chmod +x`
4. **修改 Makefile**：按 4.1 添加 `menuconfig` target
5. **修改 CMakeLists.txt**：按 4.2 插入 Kconfig 集成块
6. **修改 include/cli_config.h**：按 4.3 重构
7. **修改 .gitignore**：按 4.4 追加
8. **验证**：
   - `make menuconfig` → 应出现 TUI，可浏览/修改/保存
   - 检查项目根目录是否生成了 `.config`
   - `make build` → CMake 阶段应打印 `Found .config, generating cli_kconfig.h`
   - 检查 `build/include/cli_kconfig.h` 是否存在且内容正确
   - 编译成功无错误
   - `./build/bin/a.out` 运行正常
9. **提交并推送**：
   ```bash
   git add -A
   git commit -m "feat: add Kconfig + menuconfig support via Kconfiglib"
   git push origin dev
   ```

---

## 7. 验证清单

| 检查项 | 预期结果 |
|--------|----------|
| `make menuconfig` 启动 | 出现 ncurses TUI，有菜单树 |
| 修改 `HISTORY_MAX` 为 8，保存 | 根目录生成 `.config`，内有 `CONFIG_HISTORY_MAX=8` |
| `make build` | 编译通过，`build/include/cli_kconfig.h` 里有 `#define HISTORY_MAX 8` |
| 删除 `.config` 后再 `make build` | 编译通过，使用 `cli_config.h` 里的 fallback 默认值 |
| `grep CLI_ENABLE_USER build/include/cli_kconfig.h` | 输出 `#define CLI_ENABLE_USER 1` |
| 关闭 `CLI_ENABLE_ADVANCED_COMPLETION` 后编译 | 源码中 `#if CLI_ENABLE_ADVANCED_COMPLETION` 块被跳过 |

---

## 8. 已知坑点 & 处理

1. **Kconfiglib 未安装**：`tools/menuconfig.py` 已做 try/except，会提示 `pip3 install kconfiglib`。
2. **Tab/Space 问题**：`Makefile` 里的命令缩进必须是 Tab。若从本 Markdown 复制粘贴，需确认没有变成空格。
3. **CMake 缓存**：如果先 `make build` 再 `make menuconfig` 修改 `.config`，再次 `make build` 时 CMake 可能不重新运行（因为输入文件没变）。需要 `touch CMakeLists.txt` 或 `rm -rf build` 后重新构建。短期可接受，长期可加 `configure_file` 让 CMake 监控 `.config` 时间戳。
4. **宏名一致性**：Kconfig 里符号叫 `CONFIG_HISTORY_MAX`，Python 脚本生成头文件时**去掉 `CONFIG_` 前缀**，所以 C 源码看到的仍是 `HISTORY_MAX`，无需改动任何 `.c` 文件。
5. **`.config` 不应入仓**：`.gitignore` 已处理。

---

## 9. 后续可扩展（不在本次计划内）

- `make savedefconfig`：导出最小差异配置。
- 多平台 `defconfig`：如 `configs/stm32_defconfig`、`configs/pc_defconfig`，通过 `make defconfig` 加载。
- CMake option 与 Kconfig 双向同步。
- `src/CMakeLists.txt` 里按 Kconfig 开关裁剪源文件列表（如 `if(CLI_ENABLE_USER)` 才编译 `cli_user.c`）。
