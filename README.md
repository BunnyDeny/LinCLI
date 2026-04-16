# LinCLI

LinCLI 是一个面向嵌入式/MCU 场景的 C 语言命令行交互框架。它采用**链接脚本段自动收集**机制，将命令、状态机、初始化函数的注册工作完全交给编译器与链接器，开发者只需用宏定义命令和选项即可。框架具备极低的内存开销、清晰的层次结构，并内建命令历史、Tab 补全、选项依赖/互斥检查、重复选项检测等实用功能。

---

## 目录

1. [环境搭建](#环境搭建)
2. [编译与运行](#编译与运行)
3. [项目结构与核心机制](#项目结构与核心机制)
4. [基本使用：如何添加自己的命令](#基本使用如何添加自己的命令)
5. [如何移植到单片机](#如何移植到单片机)
6. [测试用例与终端操作示例](#测试用例与终端操作示例)
7. [高级交互功能](#高级交互功能)
   - [命令历史](#命令历史)
   - [Tab 补全](#tab-补全)
8. [用户可定制接口](#用户可定制接口)
   - [日志系统 `cli_printk`](#日志系统-cli_printk)
   - [命令提示符样式](#命令提示符样式)

---

## 环境搭建

本框架使用 CMake 构建，依赖以下工具：

- **GCC** (`gcc`)
- **Make** (`make`)
- **CMake** (`cmake`，建议 >= 3.10)

### Ubuntu / Debian 安装

```bash
sudo apt update
sudo apt install build-essential cmake
```

### CentOS / Fedora / openEuler 安装

```bash
sudo yum install gcc make cmake
# 或
sudo dnf install gcc make cmake
```

### macOS 安装

```bash
xcode-select --install
brew install cmake
```

安装完成后，验证版本：

```bash
cmake --version
gcc --version
make --version
```

---

## 编译与运行

### 1. 克隆并进入项目目录

```bash
cd /path/to/cli
```

### 2. 创建构建目录并编译

```bash
mkdir build && cd build
cmake ..
make
```

编译成功后，可执行文件位于 `build/bin/a.out`。

### 3. 运行 PC 模拟程序

```bash
./bin/a.out
```

程序启动后，终端进入 **raw 模式**（禁用行缓冲和回显），每按一个键都会立即被框架处理。你可以直接输入测试命令进行交互。

> **退出方式**：目前测试用例中没有内建 `exit` 命令，直接按 `Ctrl+C` 即可终止程序。程序退出前会自动恢复终端规范模式。

---

## 项目结构与核心机制

```
cli/
├── cli/              # CLI 核心：命令行编辑、状态机、命令分发
│   ├── Inc/          # 头文件
│   └── Src/          # 源文件
├── init/             # 初始化与线程入口（PC 模拟）
│   ├── main.c        # 主线程 + 双 pthread 模拟输入/调度
│   └── scheduler.c   # 基于状态机的任务调度器
├── lib/              # 基础库：状态机、向量、红黑树
├── tests/            # 测试用例（全部链接到单个 a.out）
├── default.ld        # 自定义链接脚本：段自动收集核心
└── CMakeLists.txt    # 顶层构建配置
```

### 链接脚本段自动注册

`default.ld` 中定义了若干自定义段：

- `.cli_commands` — 存放所有注册的命令定义
- `.cli_cmd_line` — 存放命令行状态机的状态节点
- `.scheduler`    — 存放调度器任务
- `.my_init_d`    — 存放初始化函数

通过 GCC 的 `__attribute__((section(...), used))`，每个命令或状态在定义时自动被放入对应段，链接阶段由链接脚本收集成连续数组。框架运行时使用段起始/结束符号（如 `_cli_commands_start` / `_cli_commands_end`）遍历，**无需手动维护注册表**。

### `CLI_COMMAND` 宏 —— 一行代码注册一个命令

LinCLI 的核心理念是：**命令注册应当像定义变量一样简单**。你不需要在 `main()` 里写任何初始化代码，也不需要调用注册函数，只要在 C 文件里用 `CLI_COMMAND` 宏写一次，链接器就会自动把它收集到系统的命令表中。

下面以 `tests/test_conflicts.c` 为例，这是一个包含**互斥选项**、**整数数组**和**依赖关系**的相对复杂的命令：

```c
#include "cmd_dispose.h"

struct conflicts_args {
    bool verbose;
    int *nums;
    size_t nums_count;
};

static int conflicts_handler(void *_args)
{
    struct conflicts_args *args = _args;
    // ... 业务逻辑 ...
    return 0;
}

CLI_COMMAND(tcf, "tcf", "Test INT_ARRAY option with conflicts",
            conflicts_handler, (struct conflicts_args *)0,
            OPTION('v', "verbose", BOOL, "Enable verbose",
                   struct conflicts_args, verbose),
            OPTION('n', "nums", INT_ARRAY, "Number list",
                   struct conflicts_args, nums, 8, "!verbose"),
            END_OPTIONS);
```

就这么多。**不需要 `register_command("tcf", ...)`，不需要在头文件里 extern 声明，不需要链接时手动加表**。`CLI_COMMAND` 宏会自动：

1. 推导参数结构体类型；
2. 定义选项数组；
3. 生成命令描述结构体并放入 `.cli_commands` 段；
4. 运行时由框架自动遍历该段完成收集。

即使是 `CLI_COMMAND_WITH_BUF`（自定义独立缓冲区），也只需把宏名换掉、加上缓冲区参数即可，注册流程完全一致。开发者只需专注于**设计结构体**和**写 handler**，剩下的全部交给编译器与链接脚本。

### 双线程/双角色模型（PC 模拟）

| 角色 | 文件 | 职责 |
|------|------|------|
| 输入生产者 | `init/main.c` 的 `cli_in_entry` | 从 `getchar()` 读取单字节，推入 `cli_io.in` FIFO |
| 调度消费者 | `init/main.c` 的 `cli_task_thread_entry` | 循环执行 `scheduler_task()`，从 FIFO 中消费字符并驱动状态机 |

这种输入/处理解耦的设计，使得向 MCU 移植时只需替换输入端（把 `cli_in_push` 放到 UART 中断里即可）。

---

## 如何移植到单片机

LinCLI 的核心代码是纯 C11，不依赖 pthread、文件系统等 PC 特有设施。移植时主要做三件事：

### 1. 替换输入入口 —— 将 UART 中断映射为 `cli_in_push`

PC 模拟中，`cli_in_entry` 线程不断调用 `getchar()` 并把字符通过 `cli_in_push()` 写入内部 FIFO：

```c
// PC 版本（init/main.c）
void *cli_in_entry(void *arg)
{
    int ch;
    while (1) {
        ch = getchar();
        cli_in_push((_u8 *)&ch, 1);
    }
}
```

在 MCU 中，去掉线程，直接在**串口接收中断**里调用 `cli_in_push`：

```c
// MCU 版本 —— 串口接收中断
void USART1_IRQHandler(void)
{
    uint8_t ch = USART_ReceiveData(USART1);
    cli_in_push(&ch, 1);
}
```

### 2. 替换调度循环 —— 把 `cli_task_thread_entry` 放进主循环

PC 模拟中，调度器在一个独立线程里循环运行：

```c
void *cli_task_thread_entry(void *arg)
{
    scheduler_init();
    while (1) {
        scheduler_task();
        usleep(10000);  // 10 ms 休眠
    }
}
```

在 MCU 中，直接放到 `main()` 的 while(1) 里：

```c
int main(void)
{
    // 硬件初始化 ...
    cli_io_init();
    scheduler_init();

    while (1) {
        scheduler_task();
        // 可选：进入低功耗模式，等待中断唤醒
    }
}
```

### 3. 实现单字符输出 —— 替换 `cli_putc`

`cli_io.c` 中提供了一个 **weak** 的默认实现，用于 PC 终端输出：

```c
__attribute__((weak)) void cli_putc(char ch)
{
    write(STDOUT_FILENO, &ch, 1);
}
```

在 MCU 工程中，**必须覆盖此函数**，将字符发往串口发送寄存器（或发送 FIFO）：

```c
// 在你的 MCU 项目中实现
void cli_putc(char ch)
{
    // 等待发送寄存器空
    while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE));
    USART_SendData(USART1, ch);
}
```

> 框架内部通过 `cli_out_push` 缓存输出，然后 `cli_out_sync()` 逐个调用 `cli_putc` 发送，因此你只需要保证 `cli_putc` 能正确发送一个字节即可。

### 4. 链接脚本适配

将 `default.ld` 中自定义段的定义迁移到你的 MCU 链接脚本中：

```ld
.cli_commands : {
    _cli_commands_start = .;
    *(.cli_commands)
    _cli_commands_end = .;
}
.cli_cmd_line : {
    _cli_cmd_line_start = .;
    *(.cli_cmd_line)
    _cli_cmd_line_end = .;
}
.scheduler : {
    _scheduler_start = .;
    *(.scheduler)
    _scheduler_end = .;
}
.my_init_d : {
    _init_d_start = .;
    *(.my_init_d)
    _init_d_end = .;
}
```

完成以上四步后，框架即可在单片机上运行。

---

## 基本使用：如何添加自己的命令

### 总原则

你只需要做两件事：

1. **创建一个 `.c` 文件**，在里面用 `CLI_COMMAND` 宏定义你的命令和选项；
2. **确保你的 `.c` 文件被编译进最终可执行文件**。具体如何配置 CMake（`add_executable`、`add_library`、`target_sources` 等）由你决定，只要链接阶段能把你的对象文件包含进来即可。

下面以一个名为 `log` 的命令为例（源文件位于 `tests/test_log.c`），从 0 开始展示完整流程。

### 第 1 步：定义参数结构体

这个结构体是**命令选项与程序数据的桥梁**。框架在解析用户输入时，会自动把每个选项的值填充到结构体对应的字段中；随后在处理函数 `log_handler` 里，你就能直接通过该结构体访问用户输入的内容。

```c
#include "cmd_dispose.h"
#include "cli_io.h"

struct log_args {
    char *file;          /* STRING */
    int level;           /* INT */
    bool verbose;        /* BOOL */
    int *tags;           /* INT_ARRAY */
    size_t tags_count;   /* INT_ARRAY 的长度（必须紧跟数组指针字段） */
};
```

### 第 2 步：实现 handler

这是命令的**响应函数**（也叫 handler）。当用户在终端输入 `log` 并带上对应的选项、按下回车后，只要命令解析成功，框架就会调用这个函数。参数 `_args` 指向的就是上一步定义的 `log_args` 结构体，里面已经填好了用户输入的值。

handler 的返回值类型为 `int`，**返回 0 表示执行成功**；**返回负数表示执行失败**，框架会打印错误提示并结束当前命令。

在这个函数里，你可以：
- 读取结构体字段，执行常规的业务逻辑；
- 进行阻塞操作、创建线程、启动状态机（LinCLI 自带的 `stateM` 模块）等更复杂的响应方式。

```c
static int log_handler(void *_args)
{
    struct log_args *args = _args;
    cli_printk("LOG command executed!\n");
    if (args->file)
        cli_printk("  file  = %s\n", args->file);
    cli_printk("  level = %d\n", args->level);
    if (args->verbose)
        cli_printk("  verbose = true\n");
    if (args->tags && args->tags_count > 0) {
        cli_printk("  tags  = ");
        for (size_t i = 0; i < args->tags_count; i++)
            cli_printk("%d ", args->tags[i]);
        cli_printk("\n");
    }
    return 0;
}
```

### 第 3 步：用 `CLI_COMMAND` 注册命令

```c
CLI_COMMAND(log, "log", "Configure logger", log_handler, (struct log_args *)0,
    OPTION('f', "file", STRING, "Log file path", struct log_args, file, true),
    OPTION('l', "level", INT, "Log level", struct log_args, level, true),
    OPTION('v', "verbose", BOOL, "Enable verbose", struct log_args, verbose),
    OPTION('t', "tags", INT_ARRAY, "Tag list", struct log_args, tags, 8, "!verbose"),
    END_OPTIONS);
```

### 宏参数详解

#### `CLI_COMMAND(name, cmd_str, doc_str, parse_cb, arg_struct_ptr, ...)`

| 参数 | 含义 |
|------|------|
| `name` | **C 标识符名**。宏会用它生成内部静态符号（如 `_cli_cmd_def_log`、`_cli_options_log`），不会暴露给用户。 |
| `cmd_str` | **命令字符串**。用户在终端里实际输入的名字，如 `"log"`。 |
| `doc_str` | **帮助文本**。执行 `log --help` 时显示的第一行描述。 |
| `parse_cb` | **处理函数**。类型必须是 `int (*)(void *)`，框架会把填充好的参数结构体指针传给它。 |
| `arg_struct_ptr` | **类型推导指针**。通常写 `(struct log_args *)0`，宏内部用 `typeof(*arg_struct_ptr)` 推导结构体类型和大小。**不能写 `NULL`**。 |
| `...` | **选项列表**。由若干 `OPTION(...)` 组成，最后以 `END_OPTIONS` 结尾。 |

#### `OPTION` 的重载特性

`OPTION` 是一个**统一入口宏**，它会根据你传入的参数个数**自动重载**到对应的底层实现。你不需要记忆 `OPTION_6`、`OPTION_7` 等名字，直接数清楚你要传几个参数，按格式写即可：

| 参数个数 | 底层宏 | 适用场景 |
|----------|--------|----------|
| 6 | `OPTION_6` | 基础类型：`BOOL` / `STRING` / `INT` / `DOUBLE` / `CALLBACK` |
| 7 | `OPTION_7` | 基础类型 + `required`（是否必需） |
| 8 | `OPTION_8` | `INT_ARRAY` + `max_args`（最大元素个数）+ `depends`（依赖或互斥） |
| 9 | `OPTION_9` | `INT_ARRAY` + `max_args` + `depends` + `required` |

#### `OPTION` 各参数含义（以最多 9 个参数的 `OPTION_9` 为例）

```c
OPTION('t', "tags", INT_ARRAY, "Tag list", struct log_args, tags, 8, "!verbose", true)
```

| 位置 | 参数 | 说明 |
|------|------|------|
| 1 | `'t'` | **短选项字符**。终端可输入 `-t`。 |
| 2 | `"tags"` | **长选项名字符串**。终端可输入 `--tags`。 |
| 3 | `INT_ARRAY` | **选项类型**。框架内置类型，不需要加引号。可选：`BOOL`、`STRING`、`INT`、`DOUBLE`、`CALLBACK`、`INT_ARRAY`。 |
| 4 | `"Tag list"` | **帮助文本**。`log --help` 时显示在该选项后面。 |
| 5 | `struct log_args` | **参数结构体类型**。必须与 `CLI_COMMAND` 第 5 个参数推导出的类型一致。 |
| 6 | `tags` | **结构体字段名**。解析成功后，结果会写入 `args->tags`。对于 `INT_ARRAY`，该字段必须是指针类型（`int *`），且**紧跟其后的字段必须是 `_count`**（如 `tags_count`），用于存放数组长度。 |
| 7 | `8` | **最大参数个数**（仅 `INT_ARRAY` 有效）。表示该数组选项最多接收 8 个整数。 |
| 8 | `"!verbose"` | **依赖/互斥字符串**。<br>• 普通字符串（如 `"verbose"`）→ **依赖**：表示该选项只有在 `-v`/`--verbose` 也出现时才合法。<br>• 以 `!` 开头的字符串（如 `"!verbose"`）→ **互斥**：表示该选项与 `-v`/`--verbose` **不能同时出现**。 |
| 9 | `true` | **是否必需**（`required`）。`true` 表示用户必须提供该选项，否则报错。 |

对于参数较少的重载，后面的参数依次省略即可。例如上面的 `OPTION('f', "file", STRING, "Log file path", struct log_args, file, true)` 是 **7 参数**形式，最后一个 `true` 表示 `required`；而 `OPTION('t', "tags", INT_ARRAY, "Tag list", struct log_args, tags, 8, "!verbose")` 是 **8 参数**形式，`required` 缺省为 `false`。

### 第 4 步：编译并运行

把这个 `.c` 文件加入你的构建系统后重新编译。无需修改 `main()`，也无需手动注册，链接器会自动把它放入 `.cli_commands` 段。运行程序后即可在终端输入：

```text
lin@linCli> log -f /tmp/app.log -l 3 -t 1 2 3
LOG command executed!
  file  = /tmp/app.log
  level = 3
  tags  = 1 2 3
lin@linCli>
```

如果缺少必需选项：

```text
lin@linCli> log -f /tmp/app.log
[ERR] 缺少必需选项: -l/--level
[ERR] 命令解析失败: log
...
lin@linCli>
```

如果触发互斥：

```text
lin@linCli> log -f /tmp/app.log -l 3 -v -t 1 2 3
[ERR] 选项 -t/--tags 与 verbose 互斥，不能同时使用
[ERR] 命令解析失败: log
...
lin@linCli>
```

也可以只带 `-v` 不带 `-t`：

```text
lin@linCli> log -f /tmp/app.log -l 3 -v
LOG command executed!
  file  = /tmp/app.log
  level = 3
  verbose = true
lin@linCli>
```

---

## 内置帮助信息

LinCLI 为**每一个命令**都自动内置了 `-h` 和 `--help` 选项，用户**无需在 `OPTION` 里手动注册**。当用户输入命令名并带上 `-h` 或 `--help` 时，框架会自动收集注册命令时提供的 `doc_str` 以及每个选项的 `help`、`required`、`depends` 等元数据，拼接成帮助文本并打印。

以 `tests/test_log.c` 中注册的 `log` 命令为例：

```text
lin@linCli> log --help
命令: log
描述: Configure logger
选项:
  -f, --file             Log file path [必需]
  -l, --level            Log level [必需]
  -v, --verbose          Enable verbose
  -t, --tags             Tag list [互斥:verbose]
lin@linCli>
```

可以看到，所有选项的描述、是否必需、与谁互斥，都是框架自动生成的。这进一步减少了开发者的重复劳动：你只需要在注册时写一次帮助文本，系统会自动把它呈现给用户。

---

## 测试用例与终端操作示例

所有测试用例位于 `tests/` 目录，编译时会被自动收集并链接进 `a.out`。下表列出每个命令的含义、可用选项以及实际终端操作日志。

### 1. `tb` — BOOL 类型选项测试

**命令描述**：测试 BOOL 开关选项。  
**选项**：`-v, --verbose`（启用详细输出）

```text
lin@linCli> tb -v
BOOL test executed!
  verbose = true
lin@linCli>
```

### 2. `ts` — STRING 类型选项测试

**命令描述**：测试字符串选项。  
**选项**：`-m, --msg <text>`（消息文本）

```text
lin@linCli> ts -m hello
STRING test executed!
  msg = hello
lin@linCli>
```

### 3. `ti` — INT 类型选项测试

**命令描述**：测试单个整数选项。  
**选项**：`-n, --num <value>`（整数值）

```text
lin@linCli> ti -n 42
INT test executed!
  num = 42
lin@linCli>
```

### 4. `td` — DOUBLE 类型选项测试

**命令描述**：测试浮点数选项。  
**选项**：`-f, --factor <value>`（浮点值）

```text
lin@linCli> td -f 3.14
DOUBLE test executed!
  factor = 3.140000
lin@linCli>
```

### 5. `ta` — INT_ARRAY 类型选项测试（带依赖）

**命令描述**：测试整数数组选项，并演示 `depends` 依赖关系。  
**选项**：
- `-v, --verbose`         （BOOL）
- `-n, --nums <list>`     （INT_ARRAY，最多 8 个，**依赖** `verbose`）

```text
lin@linCli> ta -v -n 1 2 3
INT_ARRAY test executed!
  verbose = true
  nums =  1  2  3
lin@linCli>
```

如果缺少依赖的 `-v`，会报错：

```text
lin@linCli> ta -n 1 2 3
[ERR] 选项 -n/--nums 依赖 verbose 但未提供
[ERR] 命令解析失败: ta
...
lin@linCli>
```

### 6. `tc` — CALLBACK 类型选项测试

**命令描述**：测试自定义回调选项，框架仅将原始字符串传给 handler。  
**选项**：`-c, --cfg <raw>`（原始配置字符串）

```text
lin@linCli> tc -c foo
CALLBACK test executed!
  custom callback triggered with: foo
lin@linCli>
```

### 7. `tr` — REQUIRED 必需选项测试

**命令描述**：测试选项的 `required=true` 校验。  
**选项**：`-f, --file <path>`（STRING，**必需**）

正常输入：

```text
lin@linCli> tr -f /tmp/data.txt
REQUIRED test executed!
  file = /tmp/data.txt
lin@linCli>
```

缺少必需选项：

```text
lin@linCli> tr
[ERR] 缺少必需选项: -f/--file
[ERR] 命令解析失败: tr
...
lin@linCli>
```

### 8. `tcf` — CONFLICTS 互斥选项测试

**命令描述**：测试选项互斥（`!` 前缀）。  
**选项**：
- `-v, --verbose`         （BOOL）
- `-n, --nums <list>`     （INT_ARRAY，与 `verbose` **互斥**）

正常单独使用：

```text
lin@linCli> tcf -n 1 2 3
CONFLICTS test executed!
  nums =  1  2  3
lin@linCli>
```

同时使用互斥选项会报错：

```text
lin@linCli> tcf -v -n 1 2 3
[ERR] 选项 -n/--nums 与 verbose 互斥，不能同时使用
[ERR] 命令解析失败: tcf
...
lin@linCli>
```

### 9. `tw` — CLI_COMMAND_WITH_BUF 独立缓冲区测试

**命令描述**：演示为命令分配独立的参数解析缓冲区（而非共享 `g_cli_cmd_buf`）。  
**选项**：
- `-v, --verbose`
- `-n, --nums <list>`（INT_ARRAY，最多 16 个）

```text
lin@linCli> tw -v -n 10 20 30
WITH_BUF test executed!
  verbose = true
  nums =  10  20  30
lin@linCli>
```

---

## 高级交互功能

### 命令历史

LinCLI 内置了命令历史记录功能，最多保存 **16 条**命令。空命令不会被记录，重复输入相同命令也不会产生冗余条目。

| 按键 | 作用 |
|------|------|
| `↑`（上箭头） | 调出上一条历史命令 |
| `↓`（下箭头） | 调出下一条历史命令，翻到最新时回到空行 |

**示例**：

```text
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

```text
lin@linCli> t<Tab>
tb  tc  tcf  td  ti  ta  tr  ts  tw
lin@linCli> t
```

如果前缀唯一匹配，则直接补全并自动追加空格：

```text
lin@linCli> tb<Tab>
lin@linCli> tb
```

#### 命令选项补全

在命令名后按空格再按 `Tab`，会列出该命令的**所有选项**（短选项 + 长选项）：

```text
lin@linCli> tb <Tab>
-v --verbose
lin@linCli> tb
```

输入 `-` 后按 `Tab`，只列出**短选项**：

```text
lin@linCli> tb -<Tab>
-v
lin@linCli> tb -
```

输入 `--` 后按 `Tab`，只列出**长选项**：

```text
lin@linCli> tb --<Tab>
--verbose
lin@linCli> tb --
```

输入长选项前缀后按 `Tab`，可前缀补全：

```text
lin@linCli> tb --v<Tab>
lin@linCli> tb --verbose
```

如果存在多个候选（有歧义），按一次 `Tab` 就会响铃并列出所有候选，然后自动重绘当前输入行，保持光标位置。

---

## 用户可定制接口

LinCLI 在设计上保留了多处**弱定义（`weak`）接口**，用户无需修改框架源码，只需在自己的项目中重新实现同名函数，即可覆盖默认行为。

### 日志系统 `cli_printk`

`cli_printk` 的使用方式**一比一复刻 Linux 内核的日志打印函数**。你可以在任何地方（包括 handler 响应函数）调用它，框架推荐直接使用头文件里定义的宏：

```c
/* cli_io.h */
#define pr_emerg(fmt, ...)  cli_printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)  cli_printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)   cli_printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)    cli_printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)   cli_printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...) cli_printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)   cli_printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)  cli_printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#define pr_cont(fmt, ...)   cli_printk(KERN_CONT fmt, ##__VA_ARGS__)
```

#### 日志级别含义

| 宏 | Level | 说明 |
|----|-------|------|
| `pr_emerg`  | 0 | 系统不可用，最严重 |
| `pr_alert`  | 1 | 必须立即处理 |
| `pr_crit`   | 2 | 严重故障 |
| `pr_err`    | 3 | 错误 |
| `pr_warn`   | 4 | 警告 |
| `pr_notice` | 5 | 正常但重要 |
| `pr_info`   | 6 | 一般信息 |
| `pr_debug`  | 7 | 调试信息 |
| `pr_cont`   | c | 继续上一条日志（不换行） |

#### 日志过滤

框架通过全局变量 `log_level` 控制日志过滤阈值：

```c
/* cli_io.h */
extern char log_level[3];
```

默认值为 `"8"`（即不过滤，全部输出）。你可以在自己的初始化代码中修改它：

```c
/* 只显示警告及以上级别（level <= 4） */
log_level[0] = '4';
```

#### 自定义日志前缀和颜色

每条日志级别的前缀文本和颜色都是通过一组 `__attribute__((weak))` 函数生成的。如果你想统一修改样式（例如去掉前缀、改成自己的项目配色），只需在工程中重新实现对应函数：

```c
/* 默认定义在 cli/Src/cli_io.c 中，可被覆盖 */
const char *pre_ERR_gen(void)
{
    return "[ERR] ";  /* 去掉默认红色，改成纯文本 */
}

const char *pre_INFO_gen(void)
{
    return "[INFO] ";
}
```

可供覆盖的函数列表：`pre_EMERG_gen`、`pre_ALERT_gen`、`pre_CRIT_gen`、`pre_ERR_gen`、`pre_WARNING_gen`、`pre_NOTICE_gen`、`pre_INFO_gen`、`pre_DEBUG_gen`、`pre_DEFAULT_gen`、`pre_CONT_gen`。

### 命令提示符样式

命令提示符（`lin@linCli>`）通过 `cli_prompt_print` 输出，它也是一个弱定义函数（默认定义在 `init/scheduler.c`）。你可以在自己的项目中覆盖它，改成你想要的提示符：

```c
void cli_prompt_print(void)
{
    cli_printk(COLOR_BOLD COLOR_RED "MyDevice" COLOR_NONE "> ");
}
```

修改后，终端提示符将变为：

```text
MyDevice>
```

---

## 写在最后

LinCLI 的设计目标是：**让命令注册像定义变量一样简单**，同时保持极低的运行时开销。得益于 GCC `section` 属性 + 自定义链接脚本的组合，开发者只需要关心业务命令和选项的定义，剩下的收集、解析、校验工作全部交给框架自动完成。无论是 Linux 仿真开发还是 MCU 裸机移植，都能快速落地。

如有问题或改进建议，欢迎提交 Issue 或 PR。
