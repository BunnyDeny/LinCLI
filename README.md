# 🎯 LinCLI
<img width="562" height="552" alt="1" src="https://github.com/user-attachments/assets/134df4d5-a761-4097-8111-40f3b70881d9" />


LinCLI 是一个面向嵌入式/MCU 场景的 C 语言命令行交互框架。它采用**链接脚本段自动收集**机制，将命令、状态机、初始化函数的注册工作完全交给编译器与链接器，开发者只需用宏定义命令和选项即可。框架具备极低的内存开销、清晰的层次结构，并内建命令历史、Tab 补全、选项依赖/互斥检查、重复选项检测等实用功能。

> 🪶 **轻到离谱**：即使开启**全部功能**（用户管理、环境变量、变量导出、高级 Tab 补全、帮助系统、命令链、自动运行），LinCLI 在 ARM Cortex-M4 上仅占 **~22 KB Flash**（`arm-none-eabi-gcc -Os`，无 LTO）。通过条件编译按需关闭模块，最小可裁剪至 **~11.5 KB**。详见下方 [条件编译与内核裁剪](#条件编译与内核裁剪) 章节。

---

## 🚀 目录

1. [快速开始](#快速开始)
2. [条件编译与内核裁剪](#条件编译与内核裁剪)
3. [注册自己的第一个命令](#注册自己的第一个命令)
4. [高级交互功能](#高级交互功能)
5. [进阶指南](#进阶指南)

---

## 📋 快速开始 <a id="快速开始"></a>

在开始写命令之前，先把环境搭好并运行起来。

- ✅ **[环境搭建与编译运行](docs/setup.md)** — 涵盖 GCC、CMake 安装，以及 PC 端编译、运行、退出方式。

### 💡 启用内置测试命令

`tests/` 目录下包含框架自带的测试命令（如 `tb`、`ti`、`ts`、`td` 等）。这些命令默认**不参与编译**，需要通过 `include/cli_config.h` 中的宏手动开启：

```c
#define CLI_ENABLE_TESTS 1
```

将其设为 `1` 后重新编译，即可在终端中使用所有测试命令。设为 `0`（默认值）时，测试文件会被编译成空目标，不占用任何 Flash/RAM。

---

## 🪶 条件编译与内核裁剪 <a id="条件编译与内核裁剪"></a>

LinCLI 采用**模块化条件编译**设计，每个子功能都可以通过 `include/cli_config.h` 中的宏独立开关。关闭不用的模块后，链接器会自动剔除对应代码（`-ffunction-sections -fdata-sections --gc-sections`），不占用任何 Flash/RAM。

### 宏开关一览

| 宏 | 功能 | 默认 | 关闭后节省 |
|------|------|:----:|:---------:|
| `CLI_ENABLE_ADVANCED_COMPLETION` | 高级 Tab 补全（选项补全、候选值、高亮循环） | `1` | **~4.7 KB** |
| `CLI_ENABLE_VAR` | 变量导出系统（`CLI_VAR` / `var` 命令） | `1` | **~2.3 KB** |
| `CLI_ENABLE_ENV` | 环境变量系统（`CLI_ENV` / `env` 命令） | `1` | **~1.5 KB** |
| `CLI_ENABLE_USER` | 用户管理系统（`su` / 权限检查） | `1` | **~1.2 KB** |
| `CLI_ENABLE_HELP` | 帮助系统（`--help` 自动生成、用法提示） | `1` | **~0.5 KB** |
| `CLI_ENABLE_CMD_CHAIN` | 命令链（`&&` 分隔的多命令执行） | `1` | **~0.3 KB** |
| `CLI_ENABLE_AUTO_RUN` | 自动运行（`CLI_AUTO_CMD` 开机执行） | `1` | **~0.3 KB** |

### 常用裁剪组合参考

| 配置 | Flash |
|------|:-----:|
| **全开（默认）** | **~22.4 KB** |
| 关高级补全 | ~17.7 KB |
| 关高级补全 + 环境变量 + 变量导出 | ~13.9 KB |
| 关上述三项 + 帮助 + 命令链 + 自动运行 | ~12.8 KB |
| **全部关闭（仅核心骨架）** | **~11.5 KB** |

### 快速测量体积

项目自带 `tools/measure_size.sh`，基于**增量法**（构建两次：含 LinCLI / 不含 LinCLI，取差值）精确测量：

```bash
# 默认配置（无 LTO）
./tools/measure_size.sh

# 关闭指定模块
./tools/measure_size.sh --no-advanced-completion
./tools/measure_size.sh --no-var --no-env
./tools/measure_size.sh --no-help --no-chain --no-auto-run

# 最小化配置（关闭全部可选模块）
./tools/measure_size.sh --no-user --no-env --no-var --no-advanced-completion --no-help --no-chain --no-auto-run

# 查看帮助
./tools/measure_size.sh -h
```

> 📝 **为什么用增量法？** 因为 example_project 本身带有一套 STM32 HAL 基线代码。直接读 ELF 体积会包含 HAL 的代码，无法反映 LinCLI 的真实开销。增量法先把 HAL 基线扣除，只算 LinCLI 带来的增量，结果更精确。

---

## 🔧 注册自己的第一个命令 <a id="注册自己的第一个命令"></a>

> 📝 万物始于点灯。下面以控制一颗 LED 为例，展示如何在 LinCLI 中添加一个完整的命令。

### ⚙️ 为什么要设计依赖和互斥？

在写代码之前，先想清楚这个命令在物理上应该满足什么约束：

1. **不能同时开和关** — LED 只有亮/灭两种状态，`--on` 和 `--off` 必须**互斥**；
2. **开灯时最好带上亮度** — 只执行 `--on` 却不告诉它亮度是多少，命令语义不完整。所以 `--on` **依赖** `--brightness`；
3. **调亮度必须先亮灯** — 如果 LED 是灭的，设置亮度没有任何物理意义。所以 `--brightness` 也**依赖** `--on`。

这就形成了一个**循环依赖**：开灯需要亮度，调亮度需要灯亮。加上互斥，三条规则一起保证了命令的语义始终自洽。

### 🛡️ 第 1 步：定义参数结构体

```c
#include "cmd_dispose.h"
#include "cli_io.h"

struct led_args {
	bool on;
	bool off;
	int brightness;
};
```

### 🔹 第 2 步：实现 handler

```c
static int led_handler(void *_args)
{
	struct led_args *args = _args;

	if (args->on) {
		cli_printk("LED ON, brightness=%d\r\n", args->brightness);
	}
	if (args->off) {
		cli_printk("LED OFF\r\n");
	}
	return 0;
}
```

注意这里**没有**用 `else` 把 `on` 和 `off` 写成互斥分支，而是直接暴力判断 `if (args->on)` 和 `if (args->off)`。框架在调用 handler 之前，已经帮你做完了所有选项校验——包括互斥、依赖、required、重复选项检测等。你不需要在 handler 里再做任何校验，只需要专注于业务逻辑即可。

> 📝 **默认值保证**：框架在每次解析命令前，都会把 `struct led_args` 所占的内存**全部清零**。因此，如果用户没有输入某个选项，对应的字段一定是 `0`（`bool` 为 `false`，`int` 为 `0`，指针为 `NULL`，数组长度为 `0`）。任何选项都是如此，handler 里可以放心地按"未指定 = 0"来写逻辑。

### 📌 第 3 步：用 `CLI_COMMAND` 注册命令

```c
CLI_COMMAND(led, "led", "Control LED",
    USAGE("led --on [-b <brightness>]", "led --off"),
    led_handler, (struct led_args *)0,
    OPTION(0, "on",  BOOL, "Turn LED on",  struct led_args, on,  0, "brightness", "off", false),
    OPTION(0, "off", BOOL, "Turn LED off", struct led_args, off, 0, NULL,         "on",  false),
    OPTION('b', "brightness", INT, "Brightness 0-100", struct led_args, brightness, 0, "on", NULL, false),
    END_OPTIONS);
```

### 💡 宏参数详解

#### `CLI_COMMAND(name, cmd_str, brief_str, _usage_arr, parse_cb, arg_struct_ptr, ...)`

| 参数 | 含义 |
|------|------|
| `name` | **C 标识符名**。宏会用它生成内部静态符号（如 `_cli_cmd_def_led`、`_cli_options_led`），不会暴露给用户。 |
| `cmd_str` | **命令字符串**。用户在终端里实际输入的名字，如 `"led"`。 |
| `brief_str` | **命令简介**。执行 `led --help` 时显示的第一行描述。 |
| `_usage_arr` | **用法字符串数组**。通过 `USAGE(...)` 宏定义，如 `USAGE("led --on [-b <brightness>]", "led --off")`。解析失败时框架会自动打印这些用法提示。 |
| `parse_cb` | **处理函数**。类型必须是 `int (*)(void *)`，框架会把填充好的参数结构体指针传给它。 |
| `arg_struct_ptr` | **类型推导指针**。通常写 `(struct led_args *)0`，宏内部用 `typeof(*arg_struct_ptr)` 推导结构体类型和大小。**不能写 `NULL`**。 |
| `...` | **选项列表**。由若干 `OPTION(...)` 组成，最后以 `END_OPTIONS` 结尾。 |

#### `OPTION` 各参数含义

`OPTION` 是固定 10 参数宏，所有选项类型统一使用同一套接口。

```c
OPTION(0, "on", BOOL, "Turn LED on", struct led_args, on, 0, "brightness", "off", false)
```

| 位置 | 参数 | 说明 |
|------|------|------|
| 1 | `'o'` | **短选项字符**。终端可输入 `-o`。不需要时填 `0`。 |
| 2 | `"on"` | **长选项名字符串**。终端可输入 `--on`。 |
| 3 | `BOOL` | **选项类型**。框架内置类型，不需要加引号。可选：`BOOL`、`STRING`、`INT`、`DOUBLE`、`CALLBACK`、`INT_ARRAY`。 |
| 4 | `"Turn LED on"` | **帮助文本**。执行 `<命令> --help` 时显示在该选项后面。 |
| 5 | `struct led_args` | **参数结构体类型**。必须与 `CLI_COMMAND` 第 5 个参数推导出的类型一致。 |
| 6 | `on` | **结构体字段名**。解析成功后，结果会写入 `args->on`。对于 `INT_ARRAY`，该字段必须是 `int *` 类型；框架会自动寻找同名的 `_count` 字段来存放实际解析到的元素个数。 |
| 7 | `0` | **最大参数个数**（仅 `INT_ARRAY` 有效）。表示该数组选项最多接收多少个整数，同时框架会在 `arg_buf` 尾部静态预留对应空间。对于非数组类型，该字段不会被使用，固定填 `0`。 |
| 8 | `"brightness"` | **依赖列表**。空格分隔的多个长选项名字符串。表示：只有当列表中所有选项都出现时(他们与本选项的先后顺序无所谓也不重要)，本选项才是合法的。不需要依赖时填 `NULL`。这里 `--on` 依赖 `--brightness`，意味着用户开灯时必须同时给出亮度值。 |
| 9 | `"off"` | **互斥列表**。空格分隔的多个长选项名字符串。表示：列表中任一选项出现时，本选项不能出现。不需要互斥时填 `NULL`。<br><br>【设计原则】互斥是**单向声明**的。如果 `-a` 与 `-b` 互斥，只需在 `-a` 的互斥列表中写 `"b"`，或在 `-b` 的互斥列表中写 `"a"`，即可覆盖整个互斥关系。当然，双方都写也完全合法，效果等价。 |
| 10 | `false` | **是否必需**（`required`）。`true` 表示用户必须提供该选项，否则报错。 |

> 📝 **INT_ARRAY 使用约束**
>
> ⚠️ 1. **字段名配对**：若 `INT_ARRAY` 的字段名为 `xxx`，则结构体中**必须存在**名为 `xxx_count` 的字段（类型通常为 `size_t`），用于存放实际解析到的数组长度。该字段在结构体中的位置没有强制要求。
> ⚠️ 2. **初始化为 `NULL`**：`int *xxx` 字段在解析前必须保证为 `NULL`（框架会在 `cli_auto_parse` 开始时 `memset(arg_struct, 0, ...)` 清零，因此默认即可满足）。如果用户手动将其设为某个非 `NULL` 指针，框架会直接把解析结果写入该地址，**不再进行任何边界检查**，可能导致越界。
> 📝 3. **静态缓冲区上限**：`max_args` 同时决定了"允许用户输入的最大个数"和"框架静态预留的连续空间大小"。如果尾部剩余空间不足，即使只输入 1 个整数也会直接报错"缓冲区不足"。

### ⚙️ 第 4 步：编译并运行

把这个 `.c` 文件加入你的构建系统后重新编译。无需修改 `main()`，也无需手动注册，链接器会自动把它放入 `.cli_commands` 段。运行程序后即可在终端输入：

```bash
lin@linCli> led --on -b 80
LED ON, brightness=80
lin@linCli>
```

关灯：

```bash
lin@linCli> led --off
LED OFF
lin@linCli>
```

如果缺少依赖（只开灯不带亮度）：

```bash
lin@linCli> led --on
[ERR] command parsing failed: led
usage: led --on [-b <brightness>]
       led --off
[ERR] try 'led -h' or 'led --help' for more information.
lin@linCli>
```

如果调亮度但不亮灯：

```bash
lin@linCli> led -b 80
[ERR] command parsing failed: led
usage: led --on [-b <brightness>]
       led --off
[ERR] try 'led -h' or 'led --help' for more information.
lin@linCli>
```

如果同时开和关：

```bash
lin@linCli> led --on --off
[ERR] command parsing failed: led
usage: led --on [-b <brightness>]
       led --off
[ERR] try 'led -h' or 'led --help' for more information.
lin@linCli>
```

> **💡 简化提示**
>
> 💡 上面的 `led` 示例把「开灯」和「亮度」拆成了两个选项（`--on` + `--brightness`），主要是为了演示 `depends` 和 `conflicts` 的用法。在真实项目中，这两个字段完全可以合并成一个 `INT` 类型的选项：
>
> 📝 ```c
> 📝 OPTION(0, "on", INT, "Turn LED on with brightness", struct led_args, brightness, 0, NULL, "off", false)
> 📝 ```
>
> 💡 这样用户只需输入 `led --on 80` 即可同时完成「开灯」和「设置亮度为 80」两个语义。`INT` 类型选项的完整用法可参考 `tests/commands/test_int.c`。
>
> 💡 此外，框架支持的选项类型远不止 `BOOL` 和 `INT`。同一个参数结构体中可以自由混用 **`STRING`**（字符串）、**`DOUBLE`**（浮点数）、**`INT_ARRAY`**（整数数组）等类型，同一个类型也可以定义多个不同名字的字段，彼此之间完全独立。欲了解各类型的详细用法和约束，请参考 [进阶指南](#进阶指南) 相关章节，或者直接查看 `tests/` 目录下的测试用例（如 `test_string.c`、`test_double.c`、`test_int_array.c` 等）。

## 💡 内置帮助信息

> 📝 **开关**：`#define CLI_ENABLE_HELP 1`（`include/cli_config.h`）。关闭可省 **~0.5 KB** Flash，命令仍能运行但不再响应 `--help` / `-h`。

LinCLI 为**每一个命令**都自动内置了 `-h` 和 `--help` 选项，用户**无需在 `OPTION` 里手动注册**。当用户输入命令名并带上 `-h` 或 `--help` 时，框架会自动收集注册命令时提供的 `brief_str`、用法列表以及每个选项的 `help`、`required`、`depends`、`conflicts` 等元数据，拼接成帮助文本并打印。

以 `led` 命令为例：

```bash
lin@linCli> led --help
 command     : led
 description : Control LED
 usage       : led --on [-b <brightness>]
               led --off
 option      :
  - , --off              Turn LED off
  - , --on               Turn LED on [depends:brightness] [conflicts:off]
  -b, --brightness       Brightness 0-100 [depends:on]
lin@linCli>
```

可以看到，所有选项的描述、是否必需、与谁互斥，都是框架自动生成的。这进一步减少了开发者的重复劳动：你只需要在注册时写一次帮助文本，系统会自动把它呈现给用户。

## ⚙️ 环境变量系统

> 📝 **使用前请确认宏开关**：`#define CLI_ENABLE_ENV 1`（`include/cli_config.h`）。关闭可省 **~1.5 KB** Flash。

LinCLI 内建一套**字符串环境变量系统**，允许你在代码中预定义一组字符串键值对，用户在终端中通过 `$NAME` 或 `$id` 引用它们。与编译期固定的宏不同，环境变量的值可以在运行时通过 `env` 命令动态修改，且替换发生在命令解析之前，因此能无缝享受命令链、`--help` 等后续流程。

### 🛡️ 注册环境变量

在任意 `.c` 源文件中：

```c
#include "cli_env.h"

CLI_ENV(PROJECT, "LinCLI-Framework");
CLI_ENV(BUILD_TYPE, "debug");
CLI_ENV(DEVICE_PREFIX, "sensor-A");
```

### 🔹 终端引用方式

**按名字引用 `$NAME`**：

```bash
lin@linCli> $echo $PROJECT
[echo] LinCLI-Framework
```

**按系统 ID 引用 `$id`**：

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

### 📌 env 命令

- 🔹 `env -l` / `env --list` — 列出所有环境变量
- 📌 `env -r <name>` / `env --read <name>` — 读取指定变量
- 🆕 `env -s 'name=value'` / `env --set 'name=value'` — 修改指定变量

```bash
lin@linCli> env -s 'BUILD_TYPE=release'
lin@linCli> env -r BUILD_TYPE
BUILD_TYPE = release
lin@linCli> $echo $BUILD_TYPE
[echo] release
```

> 📝 **引号保护**：从 v1.4.7 开始，支持用 `'` 或 `"` 包裹整个参数。`env -s 'GREETING=hello world'` 可以正确设置含空格的变量；`env -s 'CMD=ts -m hello && ts -m world'` 设置后执行 `$CMD`，会正确分割命令链并依次执行。
>
> 📝 **仅支持修改已注册变量**：如果变量未通过 `CLI_ENV` 注册，`env -s` 会报错 `"unknown environment variable: xxx"`。

### 💡 使用示例：动态切换命令模板

把一条复杂命令的参数模板提取成环境变量，在不同场景下快速切换：

```c
CLI_ENV(TARGET, "sensor-A");
```

```bash
lin@linCli> log --file /tmp/$TARGET.log
# 实际执行：log --file /tmp/sensor-A.log

lin@linCli> env -s TARGET=sensor-B
lin@linCli> log --file /tmp/$TARGET.log
# 实际执行：log --file /tmp/sensor-B.log
```

环境变量的值中如果包含 `&&`，展开后会自动触发命令链分割，实现参数级的命令组合：

```bash
lin@linCli> env -s 'PIPELINE=flash --erase && flash --write && flash --verify'
lin@linCli> $PIPELINE
# 依次执行三条命令，前一条失败则后续自动停止
```
---

## 🚀 高级交互功能 <a id="高级交互功能"></a>


### ⚙️ 命令历史

LinCLI 内置了命令历史记录功能，最多保存 **HISTORY_MAX 条**命令。空命令不会被记录，重复输入相同命令也不会产生冗余条目。

| 按键 | 作用 |
|------|------|
| `↑`（上箭头） | 调出上一条历史命令 |
| `↓`（下箭头） | 调出下一条历史命令，翻到最新时回到空行 |

**示例**：

```bash
lin@linCli> tb -v
BOOL test executed!
  verbose = true
lin@linCli> ti -n 100
INT test executed!
  num = 100
lin@linCli>        <-- 按 ↑
lin@linCli> ti -n 100
        <-- 再按 ↑
lin@linCli> tb -v
```

即使从历史中调出旧命令并按 Enter，该命令仍会被保存为最新历史条目（如果与最新历史不同）。

### 🛡️ Tab 补全

> 📝 **高级补全开关**：`#define CLI_ENABLE_ADVANCED_COMPLETION 1`（`include/cli_config.h`）。关闭后仅保留命令名前缀匹配 + 列表打印，可省 **~4.7 KB** Flash。

按 `Tab` 键可触发两层补全：**命令名补全** 和 **命令选项补全**。框架会自动遍历链接段中的命令/选项定义，全部在 Flash 中完成，**不占用额外 RAM**。

#### 命令名补全

输入命令前缀后按 `Tab`：

```bash
lin@linCli> t<Tab>
tb  tc  tcf  td  ti  ta  tr  ts  tw
```
如果前缀唯一匹配，则直接补全并自动追加空格：


#### 命令选项补全

在命令名后按空格再按 `Tab`：

- 💎 **若该命令只有一个选项**：直接补全该选项的长选项（或短选项），并追加空格


- ✅ **若该命令有多个选项**：列出所有选项(长选项与短选项)：

```bash
lin@linCli> led 
-f --file  
-l --level  
-v --verbose  
-t --tags  
```

输入 `-`后按`Tab`，列出所有的选项（长选项与短选项）：
```bash
lin@linCli> log -
-f --file  
-l --level  
-v --verbose  
-t --tags  
```

输入 `--` 后按 `Tab`，只列出**长选项**（单选项命令则直接补全）：

```bash
lin@linCli> log --
--file  
--level  
--verbose  
--tags  
```

输入长选项前缀后按 `Tab`，可前缀补全（与命令名补全逻辑相同，支持歧义前缀的 LCP 填充）：

```bash
lin@linCli> tb --v<Tab>
lin@linCli> tb --verbose 
```

如果存在多个候选（有歧义），按一次 `Tab` 就会响铃并列出所有候选，然后自动重绘当前输入行，保持光标位置。


> **⚠️ 关于 `-h` / `--help` 的注意事项**
>
> 📝 所有命令都**内建** `-h` 和 `--help` 支持（框架在 `cmd_parse_prepare` 中硬编码检查），因此：
> 📝 1. **Tab 补全不会列出 `-h` / `--help`**，它们不在 `cmd->options[]` 数组里。
> 📝 2. **请避免在自己的命令中注册 `-h` 或 `--help` 选项**。如果注册了，框架的帮助逻辑会优先拦截，导致你自定义的 handler 永远不会被执行。

> 📝 **小提示**：按 `Ctrl + L` 会发送清屏转义序列（`\x0c`），框架接收到后会立即清空当前终端屏幕并重新打印提示符，方便在输出内容较多时快速获得干净的命令行界面。

### 🔹 开机自动执行命令

> 📝 **开关**：`#define CLI_ENABLE_AUTO_RUN 1`（`include/cli_config.h`）。关闭可省 **~0.3 KB** Flash。

LinCLI 支持在调度器初始化完毕、并执行完所有 `init_d` 导出的初始化函数之后，**自动顺序执行一系列预设命令**。这非常适合在设备上电后自动完成一些配置或自检动作。

你只需要在自己的源文件中重新定义弱定义的 `cli_auto_cmds` 数组和 `cli_auto_cmds_count` 变量：

```c
#include "cli_auto_cmd.h"

const char * const cli_auto_cmds[] = {
    "tb -v",
    "ti -n 100",
};
const int cli_auto_cmds_count = sizeof(cli_auto_cmds) / sizeof(cli_auto_cmds[0]);
```

系统启动后会按数组下标顺序依次执行。如果某一条命令执行失败（解析失败、验证失败或 handler 返回负数），框架会**立即停止后续自动命令**，并进入命令提示符模式等待用户输入。失败的那条命令的错误信息会正常打印出来。

> 📝 **为什么可以「不写就当作没有」？—— `weak` 符号简介**
>
> 📝 `cli_auto_cmds` 在框架头文件中被声明为 `__attribute__((weak))`（弱符号）。这意味着：
> 📝 - **如果你在自己的代码中定义了它**，链接器会优先使用你的定义；
> 📝 - **如果你没有任何地方定义它**，链接器不会报错，而是自动把它当作 `NULL`（对于整数类型则是 `0`）。
>
> 📝 因此，LinCLI 在启动时只需要检查 `cli_auto_cmds` 是否为 `NULL`，就可以安全地判断用户是否需要自动执行命令。**你不需要注册、不需要开关宏、不需要写空数组占位**——什么都不做，功能就自动关闭。
>
> 💡 项目中 `tests/commands/test_auto_cmd.c` 里的示例默认是被注释掉的，原因正是如此：如果打开它，每次启动都会先打印几条测试命令的输出，影响开机界面的美观；而注释掉之后，由于 `weak` 机制，程序链接、运行都不会报错，直接平滑跳过自动执行阶段。

### 📌 命令链 `&&`

> 📝 **开关**：`#define CLI_ENABLE_CMD_CHAIN 1`（`include/cli_config.h`）。关闭可省 **~0.3 KB** Flash。

在命令提示符下，你可以用 `&&` 把多个命令串联成一行，实现类似 Shell 的短路与行为：

```bash
lin@linCli> tb -v && ti -n 42
BOOL test executed!
  verbose = true
INT test executed!
  num = 42
lin@linCli>
```

**规则与 Linux Shell 一致**：
- 🔹 只有前一个命令**成功**（返回 0），才会执行后一个命令；
- 📌 如果某个命令**失败**，后续命令全部跳过，直接回到提示符；
- 🆕 每个命令的返回状态由其 handler 的返回值决定。

**失败示例**：

```bash
lin@linCli> tb -v && big1 && ti -n 99
BOOL test executed!
  verbose = true
[ERR] 命令 big1 缓冲区不足，缺少 1 字节
[ERR] 命令解析失败: big1
...
lin@linCli>
```

可以看到，`big1` 失败后，`ti -n 99` 没有执行。

注意：
- 💎 命令链的右侧至少需要一个空格，即不能是 `echo hello &&tb -v`这样，但是即使这样输入了，两个命令仍然是可以正常运行的，唯一的后果是第二个命令`tb -v`就享受不到系统的补全服务了，所以不建议这么做，毕竟我们使用这个项目是追求高效的，肯定不希望享受不到这么完备的补全功能

### 💡 日志过滤 `level`

框架内置了 `cli_printk` / `pr_*` 日志输出系统。默认情况下所有级别的日志都会输出。你可以通过 `level` 命令设置过滤级别，只显示指定级别及以上的日志：

```bash
lin@linCli> level --info
```

支持的级别选项（从低到高）：

| 选项 | 级别 | 数值 | 说明 |
|------|------|------|------|
| `--emerg` | EMERG | 0 | 系统崩溃 |
| `--alert` | ALERT | 1 | 必须立即处理 |
| `--crit` | CRIT | 2 | 严重错误 |
| `--err` | ERR | 3 | 一般错误 |
| `--warning` | WARNING | 4 | 警告 |
| `--notice` | NOTICE | 5 | 注意 |
| `--info` | INFO | 6 | 普通信息（默认） |
| `--debug` | DEBUG | 7 | 调试信息 |

**使用示例**：

```bash
# 只显示错误及以上级别（屏蔽 warning/notice/info/debug）
lin@linCli> level --err

# 恢复默认（显示所有级别）
lin@linCli> level
```

> 🐛 **提示**：日志过滤在嵌入式调试时非常实用。例如电机控制场景下，你可以先 `level --err` 屏蔽常规信息，只看错误日志；排查完成后切回默认级别继续观察全量输出。

---


---

## 📋 进阶指南 <a id="进阶指南"></a>

以下主题适合在掌握基本命令注册之后进一步阅读：

- ✅ **[项目结构与核心机制](docs/architecture.md)** — 介绍 `src/cli/`、`src/lib/`、`src/init/`、`tests/` 各目录职责，以及链接脚本段自动收集命令的核心原理。适合在注册完第一个命令之后，想要理解框架内部工作机制时阅读。

- 🔹 **[异步非阻塞命令](docs/async_commands.md)** — 介绍 `CLI_COMMAND_ASYNC` 宏与 `entry/task/exit` 三阶段接口。把耗时操作（电机控制、传感器等待、Flash 擦写等）拆成多次调度周期分片执行，避免阻塞 CLI 主循环。包含返回值语义、状态流转图和完整示例。

- 📌 **[开机初始化函数](docs/init.md)** — 通过 `_EXPORT_INIT_SYMBOL` 宏自动收集开机初始化例程，无需在 `main()` 中手动调用。支持按优先级排序执行，非常适合 Logo 打印、许可证声明、全局状态置初值等轻量级工作。

- 🆕 **[移植到单片机](docs/porting.md)** — 将 LinCLI 从 PC 模拟环境移植到 MCU 的完整指南，包括 UART 中断输入映射、调度循环集成、临界区保护实现、单字符输出覆盖，以及 GCC/LD 和 Keil MDK 的链接脚本适配示例。

- 💎 **[测试用例详解](docs/tests.md)** — 所有内置测试命令（`tb`、`ts`、`ti`、`td`、`ta`、`tc`、`tr`、`tcf`、`tw` 等）的功能说明、可用选项和终端操作示例。

- ✅ **[用户可定制接口](docs/customization.md)** — 介绍如何通过弱定义（`weak`）覆盖框架的默认行为，包括日志系统 `cli_printk`、日志过滤与颜色、命令提示符样式等。

- 🔹 **[变量系统](docs/cli_var.md)** — 通过 `CLI_VAR` / `CLI_VAR_RO` 宏把代码中的全局变量导出为 CLI 可读写对象。需开启 `#define CLI_ENABLE_VAR 1`。

- 📌 **[环境变量系统](docs/cli_env.md)** — 通过 `CLI_ENV` 宏注册字符串键值对。需开启 `#define CLI_ENABLE_ENV 1`。

- 🆕 **[Tab 补全候选列表](docs/candidates.md)** — 通过 `CLI_CANDIDATE` 宏为 `STRING` 类型选项预先定义一组候选值，用户在终端按 `Tab` 即可自动补全文件名、配置项等已知常量。

- 🛡️ **[用户管理系统](docs/cli_user.md)** — 通过 `CLI_USER` 宏注册用户并分配命令级权限。需开启 `#define CLI_ENABLE_USER 1`。

- 💎 **[尾行模式打印支持](docs/inline_print.md)** — 当后台代码通过 `cli_printk` / `pr_*` 输出日志时，如果用户正处于命令输入状态，框架会自动清行、输出日志、再完整重绘命令提示符和已输入内容（包括 Tab 补全候选列表），光标位置也会自动恢复。无需任何配置，开箱即用。


LinCLI 的设计目标是：**让命令注册像定义变量一样简单**，同时保持极低的运行时开销。得益于 GCC `section` 属性 + 自定义链接脚本的组合，开发者只需要关心业务命令和选项的定义，剩下的收集、解析、校验工作全部交给框架自动完成。无论是 Linux 仿真开发还是 MCU 裸机移植，都能快速落地。

如有问题或改进建议，欢迎提交 Issue 或 PR。
