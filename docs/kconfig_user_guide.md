# 🎛️ LinCLI Kconfig 配置指南

> 🎯 本文面向**第一次接触 Kconfig** 的 LinCLI 用户，介绍如何使用 Kconfig / `menuconfig` 来裁剪和配置 LinCLI 框架。

---

## 🧩 1. Kconfig 是什么？

**Kconfig** 是 Linux 内核使用的配置系统。它的核心思想是：

> 📋 用一套**声明式语法**描述"有哪些配置项、它们的类型、默认值、依赖关系"，再通过一个**交互式 TUI（文本用户界面）**让用户勾选或填写，最终生成一份**纯文本配置文件**（`.config`）。

LinCLI 引入 Kconfig 后，你不再需要手动编辑 `include/cli_config.h` 来开关功能或修改数值。🔧 所有配置都收敛到 Kconfig，编译时自动生成 C 头文件，源码零改动。

---

## ⚙️ 2. 实现原理（简要）

为了让第一次接触的你理解"背后发生了什么"，这里是整个流程的简要说明：

**🔹 步骤 1 — 用户操作**
- 🆕 第一次编译：什么都不用做，直接 `make`
- 🎨 想改配置：运行 `make menuconfig`

**🔹 步骤 2 — 默认配置**
- 📦 项目仓库预置了 `configs/lincli_defconfig`（默认配置快照）
- 🔄 CMake 检测到没有 `.config` → 自动复制 `configs/lincli_defconfig` → `.config`

**🔹 步骤 3 — 生成 C 头文件**
- 🐍 Python 脚本 `tools/config_to_header.py` 读取 `.config`
- 📝 转换成 C 宏，写入 `build/include/cli_kconfig.h`
- 💡 例如：`CONFIG_HISTORY_MAX=4` → `#define HISTORY_MAX 4`

**🔹 步骤 4 — 源码包含**
- 📂 `include/cli_config.h`（只剩版本号等静态内容）内部执行 `#include "cli_kconfig.h"`
- ✨ 于是所有源码看到的宏都来自自动生成的 `cli_kconfig.h`

**🔹 步骤 5 — 编译生效**
- 🔨 GCC 编译源码，宏生效，不用的模块被条件编译裁掉

### 📋 关键文件说明

| 📄 文件 | 🔍 作用 | 🏠 是否入仓 |
|------|------|:--------:|
| `Kconfig` | 配置树入口，定义菜单结构 | ✅ |
| `src/cli/Kconfig` | CLI 功能开关（用户系统、Tab 补全、帮助等） | ✅ |
| `src/lib/Kconfig` | 内存与缓冲区配置 | ✅ |
| `configs/lincli_defconfig` | 默认配置快照，新用户自动复制为 `.config` | ✅ |
| `.config` | 你当前生效的配置（由 Kconfig 生成） | ❌ |
| `tools/lincli_menuconfig.py` | 交互式 TUI 前端 | ✅ |
| `tools/config_to_header.py` | `.config` → `cli_kconfig.h` 转换器 | ✅ |
| `build/include/cli_kconfig.h` | 自动生成的 C 头文件 | ❌ |

> 💡 **为什么有两个配置文件？** `configs/lincli_defconfig` 是仓库的"出厂设置"，保证新用户 clone 下来就能直接编译。`.config` 是你个人工作区的配置，可以被 `make menuconfig` 修改，且已被 `.gitignore` 排除，不会污染仓库。

---

## 🚀 3. 第一次编译（零操作）

如果你刚 clone 仓库，**什么配置都不用做**，直接编译即可：

```bash
cd /path/to/LinCLI
make
```

CMake 会自动发现你没有 `.config`，于是把 `configs/lincli_defconfig` 复制一份作为 `.config`，再自动生成 `build/include/cli_kconfig.h`。整个过程你无感知。🎯

---

## 🎨 4. 交互式配置：make menuconfig

如果你想裁剪功能或修改参数（比如把历史记录从 4 条改成 10 条），运行：

```bash
make menuconfig
```

你会看到一个 ncurses TUI 界面，顶层菜单如下：

```
LinCLI Configuration
────────────────────
  LinCLI Core  --->
  Tests & Demos  --->
```

按 `Enter` 进入 `LinCLI Core` 后，可看到：

```
CLI Features  --->
Library & Memory  --->
```

### 🎮 基本操作

| ⌨️ 按键 | 🎯 作用 |
|------|------|
| `↑` / `↓` | 上下移动光标 |
| `Enter` | 进入子菜单 / 切换布尔值 |
| `Space` | 切换布尔值 `[ ]` ↔ `[*]` |
| `Esc` `Esc` 或 `Q` | 返回上一级 / 退出 |
| `?` | 查看当前选项的帮助说明 |
| `/` | 搜索配置项 |

### 📝 修改示例：调整历史记录条数

1. 🎨 运行 `make menuconfig`
2. ➡️ 光标选中 `LinCLI Core --->`，按 `Enter` 进入
3. ➡️ 光标选中 `CLI Features --->`，按 `Enter` 进入
4. ⬇️ 光标选中 `(4) Command history entries`
5. 📝 按 `Enter`，输入新数值（如 `10`），再按 `Enter` 确认
6. 🚪 连续按 `Q` 退出，提示保存时选 `Yes`
7. ✅ 根目录会生成/更新 `.config` 文件

### 🔨 使配置生效

`.config` 修改后，直接重新编译即可：

```bash
make
```

CMake 会自动检测 `.config` 的变化，重新生成 `cli_kconfig.h`，并只重新编译受影响的源文件。无需手动 `make clean`。

> 💡 **常用 Makefile 目标速查**
>
> | 🎯 目标 | 🔍 作用 |
> |------|------|
> | `make` | 编译项目 |
> | `make clean` | 删除编译产物（`build/`），**保留** `.config` |
> | `make mrproper` | 删除编译产物 **和** `.config`、`.config.old`，回到刚 clone 的状态 |
> | `make defconfig` | 从 `configs/lincli_defconfig` 恢复一份全新的默认配置 |
> | `make savedefconfig` | 把当前配置导出为最小差异的 `defconfig` 文件 |
> | `make menuconfig` | 打开交互式配置界面 |
>
> 📋 这个命名习惯与 **Linux 内核**一致：`clean` 只清编译产物，`mrproper` 才清配置。

---

## 🛠️ 5. 配置管理三件套：`defconfig` / `oldconfig` / `savedefconfig`

除了 `make menuconfig`，还有三个命令在日常开发中非常实用。下面用生活化的例子讲清楚。

### 📦 `make defconfig` — 加载默认配置

从仓库预置的 `configs/lincli_defconfig` 加载一份完整的默认配置。

```bash
make defconfig
make
```

**什么时候用？**
- 🔧 你的 `.config` 被改乱了，想一键回到仓库默认状态
- ✅ 想确认"默认配置下编译是否通过"

> 💡 注意：`make defconfig` 不会删除 `build/` 目录，建议之后执行 `make`（CMake 会自动刷新）。如果编译出错再考虑 `make clean && make`。

---

### 🔄 `make oldconfig` — 给旧配置"打补丁"

**场景**：你昨天配好了 `.config`，今天 `git pull` 拉到了新代码，作者新增了一个 Kconfig 配置项。

**问题**：旧的 `.config` 里没有这个新项，直接编译可能会出问题。

**`make oldconfig` 做的事**：

```bash
make oldconfig
# 输出：
# Loaded existing config: .config
# Updated .config with defaults for any new symbols
```

它把新旧合并：
- 📋 你原来配好的值（比如 `HISTORY_MAX=10`）**原封不动保留**
- 🆕 新增的配置项自动填上**默认值**

**说白了**：就是给旧的 `.config` "打补丁"，补上新出的配置项，但不会问你问题（有默认值就直接用）。

**什么时候用？**
- 🔄 `git pull` 后发现代码里多了新的 Kconfig 选项
- 📂 从别的分支复制了一份 `.config` 过来，想同步到当前 Kconfig 树
- 🔧 `.config` 手改乱了，想让它自动对齐当前 Kconfig 的合法状态

---

### 💾 `make savedefconfig` — 生成"最小差异配置"

**场景**：你调了 20 个配置项，最终 `.config` 有 50 行。但你其实只改了 3 个地方。

**问题**：你想把这份"个性化配置"保存下来发给同事，但直接发 50 行的 `.config` 太啰嗦了。

**`make savedefconfig` 做的事**：

```bash
make savedefconfig
cat defconfig
```

输出可能只有 2 行：

```text
CONFIG_HISTORY_MAX=10
# CONFIG_CLI_ENABLE_ADVANCED_COMPLETION is not set
```

**发现没有？只有 2 行！** ✨

它把 50 行的 `.config` 压缩成了 **只包含与默认值不同的项**。因为其他所有配置都和 `configs/lincli_defconfig` 一样，没必要重复写。

**说白了**：`savedefconfig` 就是生成一份"最小差异配置"，像 `git diff` 一样只记录你改了什么。

**什么时候用？**
- 💾 保存自己的个性化配置，方便备份或分享
- 🏭 给不同硬件平台做 `configs/pc_defconfig`、`configs/stm32_defconfig`
- 🔍 看看到底改了哪些配置，心里有个数

---

### 📂 `make defconfig DEFCONFIG=` — 加载指定配置

既然 `savedefconfig` 能生成一份"最小差异配置"，那自然也需要一种方式把它**加载回来**。

`make defconfig` 支持通过 `DEFCONFIG` 参数指定任意配置文件，它会将其**展开为一份完整的 `.config`**（缺失项自动填默认值）：

```bash
# 方式 A：直接 make xxx_defconfig，自动去 configs/ 目录查找
make my_board_defconfig

# 方式 B：显式指定路径（任意位置）
make defconfig DEFCONFIG=configs/my_board_defconfig

# 方式 C：加载当前目录的 defconfig
make defconfig DEFCONFIG=defconfig

make
```

> 💡 **关于 DEFCONFIG 文件的格式**：上述三种方式对文件格式没有严格限制。你可以传一份**差异配置**（只包含与默认值不同的项，如 `savedefconfig` 生成的格式），也可以传一份**完整配置**（直接就是 `.config` 改名）。两种方式都能正确处理——本质上是"先加载默认配置，再用指定文件逐条覆盖"。

**什么时候用？**
- 🔄 换了台机器，从同事那里拿到一份 `defconfig`，一键恢复个性化配置
- 📂 在不同硬件平台之间切换配置（如 PC ↔ STM32）
- ✅ 用 `make savedefconfig` 生成差异配置 → 备份 → `make xxx_defconfig` 恢复

---

### 📊 一张图看懂

```
configs/lincli_defconfig          .config（你的个性化配置）
     │                        │
     │    make oldconfig      │  ← 新代码新增了 Kconfig 项
     │        ──────►         │
     │    补上新项默认值       │
     │                        │
     │   make savedefconfig   │
     │        ──────►         │
     │                        ▼
     │                   defconfig（只有差异，2 行）
     │
     ▼
  仓库预置的"出厂设置"
```

---

## 📝 6. 直接编辑 .config（不推荐）

`.config` 是纯文本，格式非常简单：

```text
CONFIG_CLI_ENABLE_USER=y
CONFIG_HISTORY_MAX=4
# CONFIG_CLI_ENABLE_SCHEDULER_TICK_PRINT is not set
```

你**可以**直接用文本编辑器修改它，但强烈建议用 `make menuconfig`，因为：
- ✅ `menuconfig` 会检查依赖关系和取值范围（如 `HISTORY_MAX` 必须在 1~64 之间）
- ⚠️ 手动改错可能导致编译错误或奇怪的运行时行为
- 🔗 某些配置项有联动关系，`menuconfig` 会自动处理

---

## 🔄 7. 恢复默认配置

如果你把配置改乱了，想恢复到仓库默认状态：

```bash
make mrproper
make
```

`make mrproper` 会删除 `build/`、`.config`、`.config.old`，回到刚 clone 仓库时的干净状态。然后 `make` 会自动从 `configs/lincli_defconfig` 复制一份全新的默认配置。

如果你只是想重新生成一份默认配置（不删编译目录）：

```bash
make defconfig
make
```

---

## 🔧 8. 与 CMake 的关系

CMake 只管"编译哪些源文件、链接哪些库"，所有条件编译和数值配置都交给 Kconfig 管理。

测试相关的代码（`tests/commands/` 和 `tests/unit/`）始终参与编译，但是否生效由 Kconfig 控制：

| 🏗️ 层面 | 🎛️ 控制方式 | 📝 作用 |
|------|----------|------|
| CMake | 无条件编译 | 把 `tests/commands/*.c` 和 `tests/unit/*.c` 加入编译目标 |
| Kconfig | `CLI_ENABLE_DEMO_*` | 控制演示命令是否注册到 CLI 中 |
| Kconfig | `CLI_ENABLE_SCHEDULER_TICK_PRINT` | 控制调度器是否打印尾行测试 |

**最佳实践**：不要修改任何 CMake 文件，所有配置都通过 `make menuconfig` 完成。

> ⚠️ **重要：Kconfig 的本质是生成配置头文件**
>
> Kconfig 的最终产物只有一件——`build/include/cli_kconfig.h`。对于 **PC 端 CMake 构建**，这个头文件会被自动包含，你无需关心。
>
> 但对于 **MCU 裸编译工程**（Keil MDK / GCC Makefile），你需要**手动**将 `build/include/cli_kconfig.h` 的内容复制到对应示例工程目录下的 `cli_kconfig.h` 中（如 `examples/stm32_g431/cli_kconfig.h`），否则 MCU 工程仍然使用的是预置的默认配置快照，而不是你通过 `make menuconfig` 调整后的配置。
>
> ⚡ **快捷命令**：`make sync-kconfig`
>
> 该命令会将 `configs/lincli_defconfig` 同步到所有示例工程目录下的 `cli_kconfig.h`：
> ```bash
> make sync-kconfig
> # → examples/stm32_g431/cli_kconfig.h
> # → examples/stm32f103_keil/cli_kconfig.h
> # → examples/pc_linux/cli_kconfig.h
> # → examples/external_demo/cli_kconfig.h
> ```
> 如果你已通过 `make menuconfig` 修改了配置，请先用 `make savedefconfig` 将 `.config` 覆盖回 `configs/lincli_defconfig`，再执行 `make sync-kconfig`。

---

## ❓ 9. 常见问题

### Q1: 运行 `make menuconfig` 报错 "kconfiglib is not installed"

```bash
sudo apt install python3-kconfiglib
```

### Q2: 修改了 `.config` 但重新编译后宏没有变化

正常情况下 CMake 会自动检测 `.config` 变化并重新生成 `cli_kconfig.h`。如果确实没有变化，执行 `make clean && make` 强制刷新。

### Q3: 我想在 CI/脚本里批量修改某个配置项，不想开 TUI

可以直接用 `sed` 修改 `.config`，然后重新编译：

```bash
sed -i 's/CONFIG_HISTORY_MAX=4/CONFIG_HISTORY_MAX=10/' .config
make
```

### Q4: `configs/lincli_defconfig` 和 `.config` 有什么区别？

- 📦 `configs/lincli_defconfig`：仓库里的"出厂默认值"，**不应手动修改**（如需修改默认值，应去改 `src/*/Kconfig` 文件里的 `default` 字段）。
- 📝 `.config`：你当前工作区的配置，**会被 `.gitignore` 忽略**，可以随便改。

### Q5: 我想给项目添加一个新的配置项

在 `src/cli/Kconfig`（或 `src/lib/Kconfig`，取决于配置项属于哪个模块）中添加新的 `config` 条目，然后运行 `make menuconfig` 即可在菜单中看到它。修改后需要更新 `configs/lincli_defconfig`：

```bash
rm .config && make   # 让 CMake 重新生成默认 .config
# 然后把新的 .config 内容复制回 configs/lincli_defconfig
cp .config configs/lincli_defconfig
```

---

如有问题，欢迎提交 Issue 或 PR。🎯
