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

使用 `--no-ff` 进行非快进合并：

```bash
git merge --no-ff dev
```

合并时同时执行版本号升级：

1. `MINOR += 1`
2. `PATCH = 0`

这代表一个正式小版本的发布。

**注意**：合并过程中若出现 `include/cli_config.h` 冲突，保留 `MINOR` 升级后的值、`PATCH = 0`，然后重新提交完成合并。

### 发布后动作

合并完成后，**必须**在 merge 结果上执行以下两步：

1. **打 tag**：在 merge 完成后的 main 分支上创建版本标签
   ```bash
   git tag v$(MAJOR).$(MINOR).0
   git push origin v$(MAJOR).$(MINOR).0
   ```
   例如 `v1.4.0`。tag 必须打在 merge 之后的 commit 上，不得提前。

2. **编写 release 文档**：在 `/tmp/` 目录下创建 `release-v{MAJOR}.{MINOR}.0.md`，汇总该小版本在 dev 分支上的全部变更。文档写完后必须提醒用户文档已就绪。

## 2. 默认工作流：编译测试通过后自动提交推送

若用户**没有明确说"不要提交"、"只编译不提交"、"暂时不推送"**等，则在编译和测试成功后，默认自动执行 `git add`、`git commit`、`git push`。

### 推送前强制检查清单

提交之前**必须**显式读取 `include/cli_config.h` 的第 15 行和第 18 行，逐字确认以下两个开关的值：

```c
#define CLI_ENABLE_TESTS 0   /* 测试命令编译开关，第 15 行 */
#define INLINE_TEST_EN   0   /* 尾行模式测试开关，第 18 行 */
```

**硬性要求**：
- 禁止凭记忆跳过此步骤，每次提交前必须实际读取文件确认
- 若任一为 `1`，**必须先改为 `0`**，再执行提交
- 关闭后再编译一次，确保没有因开关关闭而引入的编译错误
- 只有确认两个开关都为 `0` 且编译通过后，才允许提交推送

## 3. 提交日志语言

所有 Git 提交日志必须使用中文。禁止使用英文提交信息。

## 4. 内存池使用规范

栈上临时缓冲区若大于 32 字节，优先改用项目内存池：

```c
char *buf = cli_mpool_alloc();
if (!buf) { /* 处理 OOM */ }
/* 使用 buf */
cli_mpool_free(buf);
```

- 每次 `cli_mpool_alloc()` 必须在同一代码路径内配对 `cli_mpool_free()`
- 分配失败时打印 `<oom>` 并安全返回，禁止继续使用 NULL 指针

## 5. 函数行数限制

**适用范围**：`cli/`、`init/`、`tests/` 目录下的源文件。

**不适用范围**：`lib/` 目录下的成熟基础库代码（如状态机、红黑树、内存池等），无需为此类代码做行数拆分。

适用范围内的函数不得超过 **25 行**（物理行数，含空行与注释）。超过时必须拆分为更小的函数。

拆分原则：
- 每个子函数职责单一
- 子函数命名清晰表达其操作
- 不得为了凑行数而故意合并逻辑
- **不得改变原有代码逻辑**。拆分只是代码结构的调整，输入、输出、副作用必须与原函数完全一致

如果拆分后无法百分之百确认逻辑未发生变化，必须：
1. **提醒用户进行测试**
2. **明确告知用户具体的测试方法**（例如：编译后运行哪些命令、观察哪些输出、验证哪些边界条件）

## 6. 代码风格

遵循项目根目录 `.clang-format` 配置。不得引入与其冲突的格式。

## 7. 项目文档与接口学习

在修改代码、添加功能或回答用户问题之前，**必须**详细阅读以下文档，从用户视角理解框架的功能接口：

### 必读文档

1. **README.md** — 项目总览、快速开始、命令注册示例
2. **Documentation/*.md** — 所有专题文档：
   - `architecture.md` — 架构与核心机制
   - `async_commands.md` — 非阻塞命令
   - `init.md` — 开机初始化
   - `porting.md` — MCU 移植
   - `customization.md` — 日志定制
   - `cli_var.md` — 变量系统
   - `candidates.md` — Tab 补全候选
   - `inline_print.md` — 尾行模式
   - `tests.md` — 测试用例说明

### 必读测试用例

`tests/` 目录下的每个 `.c` 文件都是**用户接口的完整示例程序**：

- `test_bool.c`、`test_int.c`、`test_double.c`、`test_string.c` — 基础选项类型用法
- `test_int_array.c`、`test_conflicts.c`、`test_required.c` — 高级选项特性
- `test_callback.c`、`test_with_buf.c` — 自定义缓冲区与回调
- `test_auto_cmd.c` — 开机自动执行命令
- `test_led.c`、`test_motor.c` — 完整命令示例（含异步命令）
- `test_log.c` — 日志过滤测试
- `test_key_interaction.c` — 键盘交互测试
- `test_init_d.c` — 初始化函数测试
- `test_cli_var.c` — 变量系统测试

**规则**：每份测试代码都展示了用户如何注册命令、定义选项、写 handler。新增功能前必须参考同类测试用例，确保新接口与已有风格一致。
