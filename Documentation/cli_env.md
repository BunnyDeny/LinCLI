# 环境变量系统

LinCLI 内建一套**字符串环境变量系统**，允许你在代码中预定义一组字符串键值对，用户在终端中通过 `$NAME` 或 `$id` 引用它们。与 `alias` 的"固定替换"不同，环境变量的值可以在运行时通过 `env` 命令动态修改，且替换发生在命令解析之前，因此能无缝享受命令链、`--help`、Tab 补全等所有后续流程。

这套系统的设计目标是：**提供比 alias 更灵活的命令简化能力**。例如，你可以把一条复杂命令的公共前缀提取成环境变量，在不同场景下快速切换，而无需重新编译。

---

## 注册环境变量

在任意 `.c` 源文件中：

```c
#include "cli_env.h"

CLI_ENV(PROJECT, "LinCLI-Framework");
CLI_ENV(BUILD_TYPE, "debug");
CLI_ENV(DEVICE_PREFIX, "sensor-A");
```

**宏参数说明：**

| 参数 | 含义 |
|------|------|
| `PROJECT` | C 标识符名，宏用它生成内部静态符号 |
| `"LinCLI-Framework"` | 注册到 CLI 的环境变量名和初始值 |

> **命名限制**：环境变量名**不能是纯整数**（如 `123`）。框架在启动时会自动忽略这类注册，避免与 `$id` 访问方式冲突。

---

## 终端引用方式

### `$NAME` — 按名字引用

```bash
lin@linCli> techo --msg $PROJECT
LinCLI-Framework
```

### `$id` — 按系统 ID 引用

框架在启动时会为每个有效环境变量分配一个递增 ID（从 0 开始），用户也可以通过 `$id` 引用：

```bash
lin@linCli> env -l
ID   NAME                 VALUE
--------------------------------------------
0    PROJECT              LinCLI-Framework
1    BUILD_TYPE           debug
2    DEVICE_PREFIX        sensor-A

lin@linCli> techo --msg $0
LinCLI-Framework
```

> **替换规则**：未定义的变量保留原样。例如 `$UNKNOWN` 找不到对应注册时，会原样输出 `$UNKNOWN`。

---

## env 命令

`env` 是框架内建命令，无需手动注册。支持三个子命令：

### `env -l` / `env --list` — 列出所有环境变量

```bash
lin@linCli> env -l
ID   NAME                 VALUE
--------------------------------------------
0    PROJECT              LinCLI-Framework
1    BUILD_TYPE           debug
2    DEVICE_PREFIX        sensor-A
```

### `env -r` / `env --read` — 读取指定变量

```bash
lin@linCli> env -r PROJECT
PROJECT = LinCLI-Framework
```

### `env -s` / `env --set` — 修改指定变量

格式必须是 `name=value`：

```bash
lin@linCli> env -s BUILD_TYPE=release

lin@linCli> env -r BUILD_TYPE
BUILD_TYPE = release

lin@linCli> techo --msg $BUILD_TYPE
release
```

> **仅支持修改已注册变量**：如果变量未通过 `CLI_ENV` 注册，`env -s` 会报错 `"unknown environment variable: xxx"`。

---

## Tab 补全支持

`env` 命令的 `-r` 和 `-s` 选项支持 Tab 补全候选列表，按 `Tab` 即可列出所有已注册的环境变量名：

```bash
lin@linCli> env -r <Tab>
BUILD_TYPE    DEVICE_PREFIX    PROJECT

lin@linCli> env -s <Tab>
BUILD_TYPE    DEVICE_PREFIX    PROJECT
```

候选列表在运行时从 `.cli_envs` 段自动遍历生成，新增变量无需额外注册补全数据。

---

## 非法字符与使用限制

环境变量的值虽然本质上是字符串，但在 LinCLI 的解析体系中有一些**硬边界**。以下情况会导致非预期行为，使用时必须避开：

### 1. 不支持 `&&`（命令链分隔符）

`&&` 是框架的命令链分隔符，在调度器阶段以裸字符串匹配的方式拆分命令。无论 `&&` 出现在环境变量值中的哪个位置，都会在替换前被命令链拆分逻辑截断。

**错误示例**：

```bash
# 试图把 A 设为 "echo hello && echo world"
lin@linCli> env -s A=echo hello && echo world
```

实际结果：`split_cmd_chain` 会先按 `&&` 把整行拆成两半：
- 前半：`env -s A=echo hello` → `A` 的值被设为 `echo hello`
- 后半：`echo world` → 作为独立命令执行

**结论**：环境变量值中**禁止出现 `&&`**。如果你确实需要表达多段语义，建议拆成多个独立变量，或改用其他分隔符（如分号 `;`、管道符 `|` 等，具体取决于你的 handler 如何解析）。

### 2. 空格后紧跟 `-` 会被解析为选项起始

LinCLI 的选项解析器在 `tokenize` 阶段，把任何以 `-` 开头的 token 都识别为选项开关。如果环境变量值中包含**空格 + 减号**（即 `-` 前面有空格），替换后的命令在解析时会把该 token 当成选项，导致命令解析失败。

**错误示例**：

```c
CLI_ENV(ARGS, "hello -v");
```

```bash
lin@linCli> ts --msg $ARGS
[ERR] command parsing failed: ts
```

原因：`ts --msg hello -v` 中的 `-v` 被当成 `ts` 命令的选项，而 `ts` 并没有 `-v` 选项。

**合法示例**（同一个单词内部使用 `-` 完全没问题）：

```c
CLI_ENV(TAG, "hello-world");
CLI_ENV(FLAGS, "motor-ctrl-v2");
```

```bash
lin@linCli> ts --msg $TAG
 hello-world
```

**结论**：
- ✅ `hello-world`、`-v2`（连在一起）完全合法
- ❌ `hello -v`（空格后紧跟 `-`）会导致解析出错

### 3. 空格是合法的，但要关注 token 边界

环境变量值**支持包含空格**，框架的字符串拼接机制会把连续的非 `-` 开头文本当作一个整体参数：

```c
CLI_ENV(GREETING, "hello world");
```

```bash
lin@linCli> ts --msg $GREETING
 hello world
```

这在 `STRING` 类型选项中完全正常工作。唯一需要注意的是：如果替换后出现在**命令名位置**或**选项名位置**，空格会导致 token 拆分，从而改变命令语义。

---

## 与 alias 的对比

| 特性 | `alias` | 环境变量 `CLI_ENV` |
|------|---------|-------------------|
| 替换时机 | 调度阶段（回车后） | 调度阶段（alias 之后） |
| 值是否可变 | ❌ 固定不变 | ✅ 运行时可通过 `env -s` 修改 |
| 支持 `$` 引用 | ❌ 不支持 | ✅ 支持 `$NAME` 和 `$id` |
| 替换粒度 | 只能替换命令名第一个单词 | 可替换命令行任意位置的 token |
| 适用场景 | 给长命令起短名字 | 动态切换公共前缀、参数模板 |

**组合使用示例**：

```c
CLI_ENV(TARGET, "sensor-A");
CMD_ALIAS(tlog, "log --file /tmp/$TARGET.log");
```

```bash
lin@linCli> env -s TARGET=sensor-B
lin@linCli> tlog
# 实际执行：log --file /tmp/sensor-B.log
```

---

## 注意事项

1. **生命周期**：`CLI_ENV` 注册的环境变量在运行时必须始终有效。虽然值本身存储在可写段中，但不要在局部作用域中注册。

2. **值长度上限**：`env -s` 修改值时，新值通过内存池分配，最大长度受 `CLI_MPOOL_SIZE` 限制（默认与 `CLI_CMD_BUF_SIZE` 相同，通常为 128 字节）。超出部分会被截断。

3. **替换只做一轮**：`cli_env_replace` 只做**单次非递归替换**。如果变量 A 的值中包含 `$B`，`$B` 不会被再次展开。这是为了避免循环引用导致的无限替换。

4. **历史记录保存原始输入**：用户在终端输入 `$PROJECT` 并按回车，历史记录中保存的是原始字符串 `$PROJECT`，而不是展开后的值。这样从历史中调出时仍然可读、可编辑。

5. **Tab 补全阶段不展开**：Tab 补全基于用户在屏幕上实际输入的字符工作，不会自动把 `$A` 替换成值后再补全。这是有意为之，保持输入行为的直观性和一致性。

6. **无需手动注册 `env` 命令**：`env` 命令是框架内建命令，只要项目中包含了 `cli_env.c`（或链接了 `cli` 库），即可直接使用。
