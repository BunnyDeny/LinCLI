# 环境变量系统

LinCLI 内建一套**字符串环境变量系统**，允许你在代码中预定义一组字符串键值对，用户在终端中通过 `$NAME` 或 `$id` 引用它们。与 `alias` 的"固定替换"不同，环境变量的值可以在运行时通过 `env` 命令动态修改，且替换发生在命令解析之前，因此能无缝享受命令链、`--help` 等后续流程。

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
| `PROJECT` | 环境变量名，终端中通过 `$PROJECT` 引用 |
| `"LinCLI-Framework"` | 环境变量的初始值 |

> **命名限制**：环境变量名**不能是纯整数**（如 `123`）。框架在启动时会自动忽略这类注册，避免与 `$id` 访问方式冲突。

---

## 终端引用方式

### `$NAME` — 按名字引用

```bash
lin@linCli> techo --msg $PROJECT
LinCLI-Framework
```

### `$id` — 按系统 ID 引用

框架在启动时会为每个有效环境变量分配一个 ID（从 0 开始），用户也可以通过 `$id` 引用：

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
#注意这里等号两侧不能出现空格！

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

---

## 非法字符与使用限制

### `CLI_ENV` 宏注册 vs `env -s` 命令行设置

两者在字符容忍度上有本质区别：

| 场景 | `CLI_ENV` 宏注册 | `env -s` 命令行设置 |
|------|------------------|---------------------|
| `&&` | ✅ 可以注册，值完整保存 | ❌ 命令行先被 `split_cmd_chain` 拆分，无法完整传入 |
| 空格后紧跟 `-` | ✅ 可以注册，值完整保存 | ❌ `-` 被 `tokenize` 识别为选项开关，设置不完整 |
| 同一单词内 `-` | ✅ 正常 | ✅ 正常 |

`CLI_ENV` 宏的值是 C 代码中的字符串常量，不经过命令行解析，因此**任何字符都能注册**。但 `env -s` 是通过命令行输入的，值在到达 `env_handler` 之前必须先经过 `split_cmd_chain` 和 `tokenize` 的处理，所以有限制。

### 1. `env -s` 的值不能包含 `&&`

`&&` 是框架的命令链分隔符。在调度器阶段，`split_cmd_chain` 会以裸字符串匹配的方式按 `&&` 拆分整行命令。这发生在 `env -s` 的命令执行之前，因此 `&&` 右侧的内容会被截断成独立的子命令。

**错误示例**：

```bash
# 试图把 A 设为 "echo hello && echo world"
lin@linCli> env -s A=echo hello && echo world
```

实际结果：
- 前半 `env -s A=echo hello` → `A` 的值被设为 `echo hello`
- 后半 `echo world` → 作为独立命令执行

**结论**：通过 `env -s` 设置值时，**值中不能出现 `&&`**。如果确实需要包含 `&&` 的值，应通过 `CLI_ENV` 宏在编译期注册。

### 2. `env -s` 的值不能包含空格后紧跟 `-`

LinCLI 的选项解析器在 `tokenize` 阶段，把任何以 `-` 开头的 token 都识别为选项开关。如果 `env -s` 的值中出现**空格 + 减号**，`-` 及其后面的内容会被当成 `env` 命令自身的选项解析，导致设置失败或设置不完整。

**错误示例**：

```bash
lin@linCli> env -s A=hello -v
```

实际结果：`-v` 被当成 `env` 命令的选项，而 `env` 并没有 `-v` 选项，直接报错。

**结论**：通过 `env -s` 设置值时，**值中不能出现空格后紧跟 `-`**。如果确实需要包含 `-` 空格的值，应通过 `CLI_ENV` 宏在编译期注册。

### 3. 替换执行时的 `-` 解析规则（使用层）

即使通过 `CLI_ENV` 宏成功注册了含 `-` 空格的值，当 `$变量` 被替换到命令行中执行时，`-` 开头的 token 仍然会被解析为选项开关。

**示例**：

```c
CLI_ENV(ARGS, "hello -v");
```

```bash
lin@linCli> ts --msg $ARGS
[ERR] unknow option: -v
[ERR] command parsing failed: ts
```

原因：替换后等效于 `ts --msg hello -v`，`-v` 被当成 `ts` 的选项。

**合法示例**（同一个单词内部使用 `-` 完全没问题）：

```c
CLI_ENV(TAG, "hello-world");
```

```bash
lin@linCli> ts --msg $TAG
 hello-world
```

原因：`-` 在单词内部，不会被识别为选项开关。

**结论**：
- ✅ `hello-world`、`-v2`（连在一起）注册和执行都正常
- ❌ `hello -v`（空格后紧跟 `-`）注册可以成功，但执行时会解析出错

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
