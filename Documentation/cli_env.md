# 🎯 环境变量系统

LinCLI 内建一套**字符串环境变量系统**，允许你在代码中预定义一组字符串键值对，用户在终端中通过 `$NAME` 或 `$id` 引用它们。环境变量的值可以在运行时通过 `env` 命令动态修改，且替换发生在命令解析之前，因此能无缝享受命令链、`--help` 等后续流程。

这套系统的设计目标是：**提供灵活的命令参数模板能力**。例如，你可以把一条复杂命令的公共前缀提取成环境变量，在不同场景下快速切换，而无需重新编译。

---

## 🚀 注册环境变量

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

> 📝 **命名限制**：环境变量名**不能是纯整数**（如 `123`）。框架在启动时会自动忽略这类注册，避免与 `$id` 访问方式冲突。

---

## 📋 终端引用方式

### 💡 `$NAME` — 按名字引用

```bash
lin@linCli> $echo $PROJECT
[echo] LinCLI-Framework
```

### ⚙️ `$id` — 按系统 ID 引用

框架在启动时会为每个有效环境变量分配一个 ID（从 0 开始），用户也可以通过 `$id` 引用：

```bash
lin@linCli> env -l
ID   NAME                 VALUE
--------------------------------------------
0    PROJECT              LinCLI-Framework
1    BUILD_TYPE           debug
2    DEVICE_PREFIX        sensor-A

lin@linCli> $echo $0
[echo] LinCLI-Framework
```

> 📝 **替换规则**：未定义的变量保留原样。例如 `$UNKNOWN` 找不到对应注册时，会原样输出 `$UNKNOWN`。

---

## 🔧 env 命令

### 🛡️ `env -l` / `env --list` — 列出所有环境变量

```bash
lin@linCli> env -l
ID   NAME                 VALUE
--------------------------------------------
0    PROJECT              LinCLI-Framework
1    BUILD_TYPE           debug
2    DEVICE_PREFIX        sensor-A
```

### 🔹 `env -r` / `env --read` — 读取指定变量

```bash
lin@linCli> env -r PROJECT
PROJECT = LinCLI-Framework
```

### 📌 `env -s` / `env --set` — 修改指定变量

格式必须是 `name=value`：

```bash
lin@linCli> env -s BUILD_TYPE=release
#注意这里等号两侧不能出现空格！

lin@linCli> env -r BUILD_TYPE
BUILD_TYPE = release

lin@linCli> $echo $BUILD_TYPE
[echo] release
```

> 📝 **仅支持修改已注册变量**：如果变量未通过 `CLI_ENV` 注册，`env -s` 会报错 `"unknown environment variable: xxx"`。

---

## 💡 Tab 补全支持

`env` 命令的 `-r` 和 `-s` 选项支持 Tab 补全候选列表，按 `Tab` 即可列出所有已注册的环境变量名：

```bash
lin@linCli> env -r <Tab>
BUILD_TYPE    DEVICE_PREFIX    PROJECT

lin@linCli> env -s <Tab>
BUILD_TYPE    DEVICE_PREFIX    PROJECT
```

---

## ⚙️ 非法字符与使用限制

### 💡 引号保护

从 v1.4.7 开始，LinCLI 支持用单引号 `'` 或双引号 `"` 包裹字符串作为不可再分的最小单元。引号内的内容不会被命令链分隔符 `&&` 拆分，也不会被空白字符分割。

**典型用法**：

```bash
lin@linCli> env -s 'GREETING=hello world'
lin@linCli> env -s 'CMD=ts -m hello && ts -m world'
lin@linCli> $1
 hello
 world
```

通过引号包裹整个 `name=value`，`env -s` 的值中可以安全地包含空格和 `&&`。

### ⚙️ `$` 替换与引号的先后顺序

LinCLI 的处理顺序非常直接：**先替换 `$变量`，再按引号分词**。这意味着引号的作用是防止空格和 `&&` 拆分 token，而不是阻止 `$` 替换。

**表象示例**：

```bash
# 假设 echo = "_echo --msg"，且 echo 的 ID 为 2

lin@linCli> $echo '$2'
[echo] _echo --msg
```

直观上你可能以为 `'$2'` 被引号包裹，`$2` 不会被替换——但在 LinCLI 中，`$echo` 和 `$2` **都会被替换**，替换完成后再去掉引号、按 token 切分。最终等效于执行 `_echo --msg '_echo --msg'`。

如果你希望 `$2` 保持字面量原样，当前唯一的办法是确保 ID 为 2 的环境变量未定义（未定义变量会保留原样输出）。

> 💡 **设计意图**：嵌入式场景下，"看到 `$` 就替，替完再分词" 的规则单一、无歧义，代码和心智负担都最小。

### 🛡️ `CLI_ENV` 宏注册 vs `env -s` 命令行设置

两者在字符容忍度上有区别：

| 场景 | `CLI_ENV` 宏注册 | `env -s` 命令行设置 |
|------|------------------|---------------------|
| `&&` | ✅ 可以注册，值完整保存 | ✅ 用引号包裹后可完整传入 |
| 空格后紧跟 `-` | ✅ 可以注册，值完整保存 | ✅ 用引号包裹后可完整传入 |
| 同一单词内 `-` | ✅ 正常 | ✅ 正常 |

`CLI_ENV` 宏的值是 C 代码中的字符串常量，不经过命令行解析，因此**任何字符都能注册**。`env -s` 通过命令行输入时，只要用引号包裹整个参数，同样可以包含任意字符。

### 🔹 替换执行时的 `-` 解析规则（使用层）

当 `$变量` 被替换到命令行中执行时，替换后的字符串会经过完整的命令解析流程（包括命令链分割、token 切分、选项解析）。因此如果变量值中包含空格后紧跟 `-` 的内容，`-` 开头的 token 仍然会被解析为选项开关。

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

## 🚀 注意事项

1. **生命周期**：`CLI_ENV` 注册的环境变量在运行时必须始终有效。虽然值本身存储在可写段中，但不要在局部作用域中注册。

2. **值长度上限**：`env -s` 修改值时，新值通过内存池分配，最大长度受 `CLI_MPOOL_SIZE` 限制（默认与 `CLI_CMD_BUF_SIZE` 相同，通常为 128 字节）。超出部分会被截断。

3. **替换只做一轮**：`cli_env_replace` 只做**单次非递归替换**。如果变量 A 的值中包含 `$B`，`$B` 不会被再次展开。这是为了避免循环引用导致的无限替换。

4. **历史记录保存原始输入**：用户在终端输入 `$PROJECT` 并按回车，历史记录中保存的是原始字符串 `$PROJECT`，而不是展开后的值。这样从历史中调出时仍然可读、可编辑。

5. **Tab 补全阶段不展开**：Tab 补全基于用户在屏幕上实际输入的字符工作，不会自动把 `$A` 替换成值后再补全。这是有意为之，保持输入行为的直观性和一致性。

6. **无需手动注册 `env` 命令**：`env` 命令是框架内建命令，只要项目中包含了 `cli_env.c`（或链接了 `cli` 库），即可直接使用。
