
https://github.com/user-attachments/assets/8986da7a-ab7a-4236-bb4c-73b00fdc9420


# LinCLI

LinCLI 是一个面向嵌入式/MCU 场景的 C 语言命令行交互框架。它采用**链接脚本段自动收集**机制，将命令、状态机、初始化函数的注册工作完全交给编译器与链接器，开发者只需用宏定义命令和选项即可。框架具备极低的内存开销、清晰的层次结构，并内建命令历史、Tab 补全、选项依赖/互斥检查、重复选项检测等实用功能。

---

## 目录

1. [快速开始](#快速开始)
2. [注册自己的第一个命令](#注册自己的第一个命令)
3. [高级交互功能](#高级交互功能)
4. [进阶指南](#进阶指南)

---

## 快速开始

在开始写命令之前，先把环境搭好并运行起来。

- **[环境搭建与编译运行](Documentation/setup.md)** — 涵盖 GCC、CMake 安装，以及 PC 端编译、运行、退出方式。

### 启用内置测试命令

`tests/` 目录下包含框架自带的测试命令（如 `tb`、`ti`、`ts`、`td` 等）。这些命令默认**不参与编译**，需要通过 `include/cli_config.h` 中的宏手动开启：

```c
#define CLI_ENABLE_TESTS 1
```

将其设为 `1` 后重新编译，即可在终端中使用所有测试命令。设为 `0`（默认值）时，测试文件会被编译成空目标，不占用任何 Flash/RAM。

---

## 注册自己的第一个命令

> 万物始于点灯。下面以控制一颗 LED 为例，展示如何在 LinCLI 中添加一个完整的命令。

### 为什么要设计依赖和互斥？

在写代码之前，先想清楚这个命令在物理上应该满足什么约束：

1. **不能同时开和关** — LED 只有亮/灭两种状态，`--on` 和 `--off` 必须**互斥**；
2. **开灯时最好带上亮度** — 只执行 `--on` 却不告诉它亮度是多少，命令语义不完整。所以 `--on` **依赖** `--brightness`；
3. **调亮度必须先亮灯** — 如果 LED 是灭的，设置亮度没有任何物理意义。所以 `--brightness` 也**依赖** `--on`。

这就形成了一个**循环依赖**：开灯需要亮度，调亮度需要灯亮。加上互斥，三条规则一起保证了命令的语义始终自洽。

### 第 1 步：定义参数结构体

```c
#include "cmd_dispose.h"
#include "cli_io.h"

struct led_args {
	bool on;
	bool off;
	int brightness;
};
```

### 第 2 步：实现 handler

```c
static int led_handler(void *_args)
{
	struct led_args *args = _args;

	if (!args->on && !args->off) {
		pr_err("please specify --on or --off\r\n");
		return -1;
	}
	if (args->on) {
		cli_printk("LED ON");
		if (args->brightness > 0)
			cli_printk(", brightness=%d", args->brightness);
		cli_printk("\r\n");
	}
	if (args->off) {
		cli_printk("LED OFF\r\n");
	}
	return 0;
}
```

注意这里**没有**用 `else` 把 `on` 和 `off` 写成互斥分支，而是直接暴力判断 `if (args->on)` 和 `if (args->off)`。因为框架在调用 handler 之前，会先检查互斥和依赖关系——这些规则是在下一步注册命令时，通过 `OPTION` 宏的 `conflicts` 和 `depends` 参数配置的。你不需要在 handler 里重复做这些校验，只需要各自处理自己的业务逻辑即可。

> **默认值保证**：框架在每次解析命令前，都会把 `struct led_args` 所占的内存**全部清零**。因此，如果用户没有输入某个选项，对应的字段一定是 `0`（`bool` 为 `false`，`int` 为 `0`，指针为 `NULL`，数组长度为 `0`）。任何选项都是如此，handler 里可以放心地按"未指定 = 0"来写逻辑。

### 第 3 步：用 `CLI_COMMAND` 注册命令

```c
CLI_COMMAND(led, "led", "Control LED", led_handler, (struct led_args *)0,
    OPTION(0, "on",  BOOL, "Turn LED on",  struct led_args, on,  0, "brightness", "off", false),
    OPTION(0, "off", BOOL, "Turn LED off", struct led_args, off, 0, NULL,         "on",  false),
    OPTION('b', "brightness", INT, "Brightness 0-100", struct led_args, brightness, 0, "on", NULL, false),
    END_OPTIONS);
```

### 宏参数详解

#### `CLI_COMMAND(name, cmd_str, doc_str, parse_cb, arg_struct_ptr, ...)`

| 参数 | 含义 |
|------|------|
| `name` | **C 标识符名**。宏会用它生成内部静态符号（如 `_cli_cmd_def_led`、`_cli_options_led`），不会暴露给用户。 |
| `cmd_str` | **命令字符串**。用户在终端里实际输入的名字，如 `"led"`。 |
| `doc_str` | **帮助文本**。执行 `led --help` 时显示的第一行描述。 |
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

> **INT_ARRAY 使用约束**
>
> 1. **字段名配对**：若 `INT_ARRAY` 的字段名为 `xxx`，则结构体中**必须存在**名为 `xxx_count` 的字段（类型通常为 `size_t`），用于存放实际解析到的数组长度。该字段在结构体中的位置没有强制要求。
> 2. **初始化为 `NULL`**：`int *xxx` 字段在解析前必须保证为 `NULL`（框架会在 `cli_auto_parse` 开始时 `memset(arg_struct, 0, ...)` 清零，因此默认即可满足）。如果用户手动将其设为某个非 `NULL` 指针，框架会直接把解析结果写入该地址，**不再进行任何边界检查**，可能导致越界。
> 3. **静态缓冲区上限**：`max_args` 同时决定了"允许用户输入的最大个数"和"框架静态预留的连续空间大小"。如果尾部剩余空间不足，即使只输入 1 个整数也会直接报错"缓冲区不足"。

### 第 4 步：编译并运行

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
[ERR] option - /--on depends on brightness but not provided
[ERR] command parsing failed: led
...
lin@linCli>
```

如果调亮度但不亮灯：

```bash
lin@linCli> led -b 80
[ERR] option -b/--brightness depends on on but not provided
[ERR] command parsing failed: led
...
lin@linCli>
```

如果同时开和关：

```bash
lin@linCli> led --on --off
[ERR] option - /--on conflicts with off, cannot be used together
[ERR] command parsing failed: led
...
lin@linCli>
```

> **💡 简化提示**
>
> 上面的 `led` 示例把「开灯」和「亮度」拆成了两个选项（`--on` + `--brightness`），主要是为了演示 `depends` 和 `conflicts` 的用法。在真实项目中，这两个字段完全可以合并成一个 `INT` 类型的选项：
>
> ```c
> OPTION(0, "on", INT, "Turn LED on with brightness", struct led_args, brightness, 0, NULL, "off", false)
> ```
>
> 这样用户只需输入 `led --on 80` 即可同时完成「开灯」和「设置亮度为 80」两个语义。`INT` 类型选项的完整用法可参考 `tests/test_int.c`。
>
> 此外，框架支持的选项类型远不止 `BOOL` 和 `INT`。同一个参数结构体中可以自由混用 **`STRING`**（字符串）、**`DOUBLE`**（浮点数）、**`INT_ARRAY`**（整数数组）等类型，同一个类型也可以定义多个不同名字的字段，彼此之间完全独立。欲了解各类型的详细用法和约束，请参考 [进阶指南](#进阶指南) 相关章节，或者直接查看 `tests/` 目录下的测试用例（如 `test_string.c`、`test_double.c`、`test_int_array.c` 等）。

## 内置帮助信息

LinCLI 为**每一个命令**都自动内置了 `-h` 和 `--help` 选项，用户**无需在 `OPTION` 里手动注册**。当用户输入命令名并带上 `-h` 或 `--help` 时，框架会自动收集注册命令时提供的 `doc_str` 以及每个选项的 `help`、`required`、`depends`、`conflicts` 等元数据，拼接成帮助文本并打印。

以 `led` 命令为例：

```bash
lin@linCli> led --help
 command     : led
 description : Control LED
 option      :
  - , --off              Turn LED off
  - , --on               Turn LED on [depends:brightness] [conflicts:off]
  -b, --brightness       Brightness 0-100 [depends:on]
lin@linCli>
```

可以看到，所有选项的描述、是否必需、与谁互斥，都是框架自动生成的。这进一步减少了开发者的重复劳动：你只需要在注册时写一次帮助文本，系统会自动把它呈现给用户。

## alias-命令重命名

命令重命名功能默认**关闭**。使用前需要将 `include/cli_config.h` 中的宏设为 `1`：

```c
#define ALIAS_EN 1
```

开启后，通过 `CMD_ALIAS` 宏来重命名你的命令，这适用于简化已经定义的一些复杂命令。
需要注意的是，新的名字只能是单个单词的形式，形如`msg log`这样的名字
是不被允许的。举一个重命名的例子(`tests/test_string.c`)：
```c
struct string_args {
	char *msg;
};

static int string_handler(void *_args)
{
	struct string_args *args = _args;
	if (args->msg)
		cli_printk(" %s\r\n", args->msg);
	return 0;
}

CLI_COMMAND(ts, "ts", "Test STRING option", string_handler,
	    (struct string_args *)0,
	    OPTION('m', "msg", STRING, "Message text", struct string_args, msg, 0, NULL, NULL, false),
	    END_OPTIONS);
CMD_ALIAS(echo, "ts --msg");
```
> **位置无关提示**
>
> `CMD_ALIAS` 和 `CLI_COMMAND` 一样，也是通过链接脚本段自动收集的。因此你**不需要**把它和被重命名的命令写在同一个文件里，甚至不需要放在该命令的注册语句后面——它可以定义在项目的任何 `.c` 文件中，链接器会自动汇总。

首先通过 `CLI_COMMAND` 宏注册了一个 `ts` 命令，这个命令可以通过 `ts --msg hello world` 向终端打印 `hello world` 字符串。可以通过下面的方式将 `ts --msg` 这个较复杂的命令重命名为 `echo` 这样的简短命令：
```c
CMD_ALIAS(echo, "ts --msg");
```
通过`alias`命令可以打印出系统当前通过`CMD_ALIAS`重命名名的所有命令：
```bash
lin@linCli> alias 

ALIAS                ORIGINAL COMMAND                        
------------------------------------------------------------
echo                 ts --msg                                
------------------------------------------------------------

lin@linCli> 
```
当`CMD_ALIAS`重命名的命令的新名字与`CLI_COMMAND`注册的命令冲突的时候，前者会覆盖后者，这也是linux系统的行为

所以，现在，通过`echo`即可实现`ts --msg`相同的功能：
```bash
lin@linCli> echo hello world
 hello world

lin@linCli> 
```
另外注意，重命名之后的命令使用`-h`选项，会打印出重命名之前的命令的帮助信息，例如上述`echo`,如果使用`echo -h`则等价于`ts -h`：
```bash
lin@linCli> echo -h
 command     : ts
 description : Test STRING option
 option      :
  -m, --msg              Message text

lin@linCli> 
```

重命名的新命令可以正常使用下文的命令链（详情看下文命令链章节）：
```bash
lin@linCli> tb --verbose && echo hello && echo nihao shijie
BOOL test executed!
  verbose = true

 hello

 nihao shijie

lin@linCli> 
```
---

## 高级交互功能


### 命令历史

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

### Tab 补全

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

- **若该命令只有一个选项**：直接补全该选项的长选项（或短选项），并追加空格


- **若该命令有多个选项**：列出所有选项(长选项与短选项)：

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
> 所有命令都**内建** `-h` 和 `--help` 支持（框架在 `dispose_start_task` 中硬编码检查），因此：
> 1. **Tab 补全不会列出 `-h` / `--help`**，它们不在 `cmd->options[]` 数组里。
> 2. **请避免在自己的命令中注册 `-h` 或 `--help` 选项**。如果注册了，框架的帮助逻辑会优先拦截，导致你自定义的 handler 永远不会被执行。

> **小提示**：按 `Ctrl + L` 会发送清屏转义序列（`\x0c`），框架接收到后会立即清空当前终端屏幕并重新打印提示符，方便在输出内容较多时快速获得干净的命令行界面。

### 开机自动执行命令

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

> **为什么可以「不写就当作没有」？—— `weak` 符号简介**
>
> `cli_auto_cmds` 在框架头文件中被声明为 `__attribute__((weak))`（弱符号）。这意味着：
> - **如果你在自己的代码中定义了它**，链接器会优先使用你的定义；
> - **如果你没有任何地方定义它**，链接器不会报错，而是自动把它当作 `NULL`（对于整数类型则是 `0`）。
>
> 因此，LinCLI 在启动时只需要检查 `cli_auto_cmds` 是否为 `NULL`，就可以安全地判断用户是否需要自动执行命令。**你不需要注册、不需要开关宏、不需要写空数组占位**——什么都不做，功能就自动关闭。
>
> 项目中 `tests/test_auto_cmd.c` 里的示例默认是被注释掉的，原因正是如此：如果打开它，每次启动都会先打印几条测试命令的输出，影响开机界面的美观；而注释掉之后，由于 `weak` 机制，程序链接、运行都不会报错，直接平滑跳过自动执行阶段。

### 命令链 `&&`

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
- 只有前一个命令**成功**（返回 0），才会执行后一个命令；
- 如果某个命令**失败**，后续命令全部跳过，直接回到提示符；
- 每个命令的返回状态由其 handler 的返回值决定。

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
- 命令链的右侧至少需要一个空格，即不能是 `echo hello &&tb -v`这样，但是即使这样输入了，两个命令仍然是可以正常运行的，唯一的后果是第二个命令`tb -v`就享受不到系统的补全服务了，所以不建议这么做，毕竟我们使用这个项目是追求高效的，肯定不希望享受不到这么完备的补全功能

---


---

## 进阶指南

以下主题适合在掌握基本命令注册之后进一步阅读：

- **[项目结构与核心机制](Documentation/architecture.md)** — 介绍 `cli/`、`lib/`、`init/`、`tests/` 各目录职责，以及链接脚本段自动收集命令的核心原理。适合在注册完第一个命令之后，想要理解框架内部工作机制时阅读。

- **[开机初始化函数](Documentation/init.md)** — 通过 `_EXPORT_INIT_SYMBOL` 宏自动收集开机初始化例程，无需在 `main()` 中手动调用。支持按优先级排序执行，非常适合 Logo 打印、许可证声明、全局状态置初值等轻量级工作。

- **[移植到单片机](Documentation/porting.md)** — 将 LinCLI 从 PC 模拟环境移植到 MCU 的完整指南，包括 UART 中断输入映射、调度循环集成、临界区保护实现、单字符输出覆盖，以及 GCC/LD 和 Keil MDK 的链接脚本适配示例。

- **[测试用例详解](Documentation/tests.md)** — 所有内置测试命令（`tb`、`ts`、`ti`、`td`、`ta`、`tc`、`tr`、`tcf`、`tw` 等）的功能说明、可用选项和终端操作示例。

- **[用户可定制接口](Documentation/customization.md)** — 介绍如何通过弱定义（`weak`）覆盖框架的默认行为，包括日志系统 `cli_printk`、日志过滤与颜色、命令提示符样式等。


LinCLI 的设计目标是：**让命令注册像定义变量一样简单**，同时保持极低的运行时开销。得益于 GCC `section` 属性 + 自定义链接脚本的组合，开发者只需要关心业务命令和选项的定义，剩下的收集、解析、校验工作全部交给框架自动完成。无论是 Linux 仿真开发还是 MCU 裸机移植，都能快速落地。

如有问题或改进建议，欢迎提交 Issue 或 PR。
