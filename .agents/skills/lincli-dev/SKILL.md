---
name: lincli-dev
description: LinCLI 嵌入式 CLI 框架项目开发规范。用于指导在该项目中编写 C 代码、提交更改、管理版本号等开发活动。当 Kimi 在该项目（/home/bunnydeny/programming/LinCLI）中执行代码修改、提交或任何开发任务时触发使用。
---

# LinCLI 项目开发规范

## 1. 版本号管理

版本号定义在 `include/cli_config.h`：

```c
#define CLI_VERSION_MAJOR 1
#define CLI_VERSION_MINOR 3
#define CLI_VERSION_PATCH N
```

### dev 分支（日常开发）

**任何提交前必须自增 `PATCH` 字段**（`N += 1`）。dev 是活跃分支，每次提交代表一个补丁，不得遗漏。

### 合并到 main 分支

#### 第 1 步：在 dev 分支上升级版本号

先切换到 dev，将版本号升级为发布版本：

```bash
git checkout dev
```

修改 `include/cli_config.h`：

1. `MINOR += 1`
2. `PATCH = 0`

例如从 `1.3.15` 改为 `1.4.0`。

然后提交这个版本号改动：

```bash
git add include/cli_config.h
git commit -m "chore: 升级版本号至 v1.4.0，准备发布"
```

#### 第 2 步：切换到 main 并合并

```bash
git checkout main
git merge --no-ff dev
```

由于版本号已在 dev 上提前升级，合并时 `include/cli_config.h` 不会出现冲突。

#### 第 3 步：发布后动作

合并完成后，**必须**在 merge 结果上执行以下两步：

1. **打 tag**：在 merge 完成后的 main 分支上创建版本标签
   ```bash
   git tag v$(MAJOR).$(MINOR).0
   git push origin v$(MAJOR).$(MINOR).0
   ```
   例如 `v1.4.0`。tag 必须打在 merge 之后的 commit 上，不得提前。

2. **编写 release 文档**：在 `/tmp/` 目录下创建 `release-v{MAJOR}.{MINOR}.0.md`，汇总该小版本在 dev 分支上的全部变更。
   - 文档中应**大量使用 emoji**（如 🎯 ✨ ⭐ 🚀 🆕 🐛 📝 等）来提升美观度和可读性
   - 各章节标题、功能列表、Bug 修复项均应配上合适的 emoji
   - 文档写完后必须提醒用户文档已就绪

## 2. 默认工作流：编译测试通过后自动提交推送

若用户**没有明确说"不要提交"、"只编译不提交"、"暂时不推送"**等，则在编译和测试成功后，默认自动执行 `git add`、`git commit`、`git push`。

## 3. 交互类 Bug 调试优先原则（最重要）

> ⚠️ 此条优先级高于代码阅读与静态分析。

当用户报告**终端交互类 bug**（如 Tab 补全异常、光标移动错乱、退格后行为异常、候选列表显示错误等）时：

### 3.1 禁止先读代码猜原因

**不得**在只读代码、脑补状态流之后就动手修改。用户的文字描述通常只能传达现象的七八成，剩余的细微差别往往才是根因所在。

### 3.2 第一步必须是：写脚本把程序跑起来，看实际终端输出

- 使用 `pty`、`pexpect` 或类似工具启动 `build/bin/a.out`
- 按用户描述的按键序列精确复现（如 `<tab><tab><退格><tab>`）
- 捕获并打印 ANSI 转义序列，**亲眼看见**终端的每一帧变化
- 只有在亲眼确认现象后，才开始结合代码定位根因

### 3.3 为什么这如此关键

本项目是嵌入式 CLI 框架，核心逻辑围绕**状态机 + 终端光标控制 + 字符缓冲区**展开。静态分析极易被以下细节误导：

- `prefix_len` 正确，但 `prefix` 指针指向的缓冲区被 `do_delete()` 回填的空格污染
- `candidate_ctx` 某个字段没被 `candidate_ctx_clear()` 清除，导致后续 Tab 走偏分支
- `display_unified_cmd_list()` 的 `rows` 计算漏掉手动输出的分隔换行
- `cmd_line.buf[size]` 位置的脏数据让 `strstr()` 把前缀误读成带空格的字符串

这些 bug 的共同特点是：**代码逻辑看起来都对，但运行时缓冲区状态被先前的操作污染了**。只有实际跑程序才能暴露这种时序/状态污染问题。

### 3.4 经验总结

> 💡 **先跑程序看见现象，再读代码定位根因，最后动手修改。** 顺序颠倒会导致反复修复、反复出错，浪费双方大量时间。

### 3.5 调试脚本的处置

- 🚫 **禁止提交临时调试脚本**（如 `test_*.py`、`debug_*.sh` 等用于启动 `a.out` 并捕获 ANSI 输出的 Python/Shell 脚本）
- 这些脚本仅在本地调试时使用，完成后应立即删除，或在 `.gitignore` 中排除
- 执行 `git add -A` 前务必检查 `git status`，确认没有误加临时文件

## 4. 提交日志语言

所有 Git 提交日志必须使用中文。禁止使用英文提交信息。

## 5. 内存池使用规范

栈上临时缓冲区若大于 32 字节，优先改用项目内存池：

```c
char *buf = cli_mpool_alloc();
if (!buf) { /* 处理 OOM */ }
/* 使用 buf */
cli_mpool_free(buf);
```

- 每次 `cli_mpool_alloc()` 必须在同一代码路径内配对 `cli_mpool_free()`
- 分配失败时打印 `<oom>` 并安全返回，禁止继续使用 NULL 指针

## 6. 函数行数限制

**适用范围**：`src/cli/`、`src/init/`、`tests/` 目录下的源文件。

**不适用范围**：`src/lib/` 目录下的成熟基础库代码（如状态机、红黑树、内存池等），无需为此类代码做行数拆分。

适用范围内的函数不得超过 **25 行**（物理行数，含空行与注释）。超过时必须拆分为更小的函数。

拆分原则：
- 每个子函数职责单一
- 子函数命名清晰表达其操作
- 不得为了凑行数而故意合并逻辑
- **不得改变原有代码逻辑**。拆分只是代码结构的调整，输入、输出、副作用必须与原函数完全一致

如果拆分后无法百分之百确认逻辑未发生变化，必须：
1. **提醒用户进行测试**
2. **明确告知用户具体的测试方法**（例如：编译后运行哪些命令、观察哪些输出、验证哪些边界条件）

## 7. 代码风格

遵循项目根目录 `.clang-format` 配置。不得引入与其冲突的格式。

## 8. 项目文档与接口学习

在修改代码、添加功能或回答用户问题之前，**必须**详细阅读以下文档，从用户视角理解框架的功能接口：

### 必读文档

1. **README.md** — 项目总览、快速开始、命令注册示例
2. **Documentation/*.md** — 所有专题文档：
   - `docs/architecture.md` — 架构与核心机制
   - `docs/async_commands.md` — 非阻塞命令
   - `docs/init.md` — 开机初始化
   - `docs/porting.md` — MCU 移植
   - `docs/customization.md` — 日志定制
   - `docs/cli_var.md` — 变量系统
   - `docs/candidates.md` — Tab 补全候选
   - `docs/inline_print.md` — 尾行模式
   - `docs/tests.md` — 测试用例说明

### 必读测试用例

`tests/commands/` 目录下的每个 `.c` 文件都是**用户接口的完整示例程序**：

- `test_bool.c`、`test_int.c`、`test_double.c`、`test_string.c` — 基础选项类型用法
- `test_int_array.c`、`test_conflicts.c`、`test_required.c` — 高级选项特性
- `test_callback.c`、`test_with_buf.c` — 自定义缓冲区与回调
- `test_auto_cmd.c` — 开机自动执行命令
- `test_led.c`、`test_motor.c` — 完整命令示例（含异步命令）
- `test_log.c` — 日志过滤测试
- `test_key_interaction.c` — 键盘交互测试
- `test_init_d.c` — 初始化函数测试
- `test_cli_var.c` — 变量系统测试

`tests/unit/` 目录下是 Unity 单元测试，测试基础库函数（string、atoi、float、mpool、vector、vsnprintf、stateM、errno）。

**规则**：每份测试代码都展示了用户如何注册命令、定义选项、写 handler。新增功能前必须参考同类测试用例，确保新接口与已有风格一致。

## 9. Markdown 文档 Emoji 规范

**适用范围**：项目内所有 `.md` 文件，包括但不限于 `README.md`、`docs/*.md`、release 文档。

### 强制要求

1. **章节标题前必须加 emoji**
   - 每个一级、二级、三级标题左侧必须放置一个语义相关的 emoji
   - 禁止出现纯文本标题（如 `## 快速开始` 必须写成 `## 🚀 快速开始`）

2. **列表项必须加 emoji**
   - 无序列表的每一项前缀必须带 emoji（如 `- ✅ 功能A`、`- 🔹 功能B`）
   - 功能特性列表、对比项、选型建议等必须使用 emoji 分类

3. **表格内必须加 emoji**
   - 状态/支持列用 ✅ ❌ ⚠️ 代替 "是/否/部分"
   - 分类/类型列用相关 emoji 装饰（如 🎯 核心、🔹 可选）

4. **提示/警告/注意块必须加 emoji**
   - 提示用 `💡` 或 `✨`
   - 警告用 `⚠️` 或 `🐛`
   - 重要说明用 `📌` 或 `📝`

5. **禁止整篇文档完全没有 emoji**
   - 任何新增或修改的 markdown 文件，提交前必须检查 emoji 覆盖率
   - 目标：每 5 行至少出现 1 个 emoji

### 推荐 Emoji 对照表

| 场景 | 推荐 Emoji |
|------|-----------|
| 章节标题（总览/介绍） | 🎯 📋 🌟 |
| 章节标题（功能/特性） | ⚙️ 💡 🔧 🛠️ |
| 章节标题（指南/教程） | 📖 📚 🎓 |
| 章节标题（对比/选型） | ⚖️ 🔍 📊 |
| 功能/特性列表项 | ✅ ❌ 🔹 💎 🆕 |
| 状态/结果 | 🟢 🔴 🟡 ✅ ❌ |
| 步骤/流程 | 1️⃣ 2️⃣ 3️⃣ 或 🥇 🥈 🥉 |
| 提示/建议 | 💡 ✨ 🔔 |
| 警告/注意 | ⚠️ 🐛 🚨 |
| 代码/技术 | 💻 🔌 ⌨️ |
| 性能/体积 | 🪶 ⚡ 📉 📈 |
| 移植/硬件 | 🔌 🔧 🖥️ |

### 示例（符合规范）

```markdown
## 🎯 快速开始

### 💡 前置条件

- ✅ 已安装 GCC
- ✅ 已安装 CMake
- ❌ 不需要额外依赖

### ⚙️ 编译步骤

1️⃣ 克隆仓库
2️⃣ 执行 `make menuconfig`
3️⃣ 执行 `make`

> ⚠️ 注意：首次编译会自动生成 `cli_kconfig.h`
```

### 反例（不符合规范）

```markdown
## 快速开始

### 前置条件

- 已安装 GCC
- 已安装 CMake

### 编译步骤

1. 克隆仓库
2. 执行 make menuconfig
3. 执行 make

> 注意：首次编译会自动生成 cli_kconfig.h
```
