

https://github.com/user-attachments/assets/205a4c8e-cd7d-4e33-8ad9-728898dd1b27

# LinCLI

LinCLI 是一个面向嵌入式/MCU 场景的 C 语言命令行交互框架。它采用**链接脚本段自动收集**机制，将命令、状态机、初始化函数的注册工作完全交给编译器与链接器，开发者只需用宏定义命令和选项即可。框架具备极低的内存开销、清晰的层次结构，并内建命令历史、Tab 补全、选项依赖/互斥检查、重复选项检测等实用功能。

---

## 目录

1. [环境搭建](#环境搭建)
2. [编译与运行](#编译与运行)
3. [项目结构与核心机制](#项目结构与核心机制)
4. [基本使用：如何添加自己的命令](#基本使用如何添加自己的命令)
5. [开机初始化函数 `_EXPORT_INIT_SYMBOL`](#开机初始化函数-_export_init_symbol)
6. [如何移植到单片机](#如何移植到单片机)
7. [内置帮助信息](#内置帮助信息)
8. [alias-命令重命名](#alias-命令重命名)
9. [测试用例与终端操作示例](#测试用例与终端操作示例)
10. [高级交互功能](#高级交互功能)
   - [命令历史](#命令历史)
   - [Tab 补全](#tab-补全)
   - [开机自动执行命令](#开机自动执行命令)
   - [命令链 `&&`](#命令链-)
11. [用户可定制接口](#用户可定制接口)
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

## 开机初始化函数 `_EXPORT_INIT_SYMBOL`

除了用 `CLI_COMMAND` 注册命令之外，LinCLI 还提供了一套与命令注册完全对称的**初始化函数自动收集机制**：`_EXPORT_INIT_SYMBOL`。它的核心思想和 `CLI_COMMAND` 一样——你只需要在某个 `.c` 文件里写一次，链接器会自动把它收集到初始化表中，**无需在 `main()` 里手动调用**。

### 宏参数格式

```c
_EXPORT_INIT_SYMBOL(obj, priority, private, init_entry)
```

| 参数 | 含义 |
|------|------|
| `obj` | **C 标识符名**。宏会用它生成内部静态符号（如 `init_d_pr_license`）。 |
| `priority` | **优先级整数**。数值越小优先级越高，越早执行（如 `-100` 先于 `0`，`0` 先于 `10`）。 |
| `private` | **私有数据指针**。在调用 `init_entry` 时会作为参数传入；如果不需要，可以填 `NULL`。 |
| `init_entry` | **初始化函数指针**。签名必须是 `void (*)(void *)`。 |

### 使用示例

参考项目中的 `cli/Src/cli_logo.c`，这是一个非常简单的例子：

```c
#include "cli_io.h"
#include "init_d.h"

void pr_logo(void *arg)
{
    cli_printk(" _     _        ____ _     ___ \n");
    cli_printk("| |   (_)_ __  / ___| |   |_ _|\n");
    cli_printk("| |   | | '_ \\| |   | |    | | \n");
    cli_printk("| |___| | | | | |___| |___ | | \n");
    cli_printk("|_____|_|_| |_|\\____|_____|___|\n");

    pr_info("欢迎使用LinCLI\n");
}

_EXPORT_INIT_SYMBOL(logo, 10, NULL, pr_logo);
```

就这么多。编译后，`_EXPORT_INIT_SYMBOL` 会自动把 `pr_logo` 放入 `.my_init_d` 段，调度器在启动时会遍历并执行它。

### 调用时机

所有通过 `_EXPORT_INIT_SYMBOL` 注册的初始化函数，会在**调度器状态机的启动状态**（`scheduler_start` 的 `start_entry`）中被统一调用。具体来说，是在 `scheduler_init()` 完成、状态机进入 `scheduler_start` 状态时，通过 `CALL_INIT_D` 将所有初始化项按 `priority` 数值升序排序后依次执行。

这发生在任何用户交互或自动命令执行之前，因此非常适合做开机 Logo 打印、许可证声明、全局状态置初值等轻量级工作。

### 使用注意事项

> ⚠️ **1. 初始化函数必须是独立、幂等的**
> 
> `_EXPORT_INIT_SYMBOL` 现在通过红黑树按 `priority` 数值升序排序后调用，因此**相同优先级的初始化函数之间不保证顺序**。建议为存在依赖关系的模块分配不同的优先级整数，确保执行顺序明确。

> ⚠️ **2. 只建议使用日志系统，避免复杂操作**
> 
> 在初始化阶段，其他外部模块（如硬件驱动、网络栈、文件系统等）可能尚未就绪——因为它们本身也可能通过 `_EXPORT_INIT_SYMBOL` 注册且顺序可能不同。因此，**初始化函数中只建议使用 `cli_printk` 打印日志或做最简单的状态置位**，不要进行阻塞操作、创建线程、访问未初始化硬件等复杂行为。
> 
> 日志系统在此阶段是安全的，因为 `cli_io_init()` 已经在 `scheduler_init()` 中被提前调用。

> ⚠️ **3. 不传递返回值**
> 
> `init_entry` 的签名为 `void (*)(void *)`，没有返回值。如果初始化过程中出现致命错误，建议通过日志打印错误信息，并由用户自行决定是否进入后续流程。

### 测试用例 `tests/test_init_d.c`

项目中提供了专门的初始化优先级排序测试用例。打开 `tests/test_init_d.c` 中的开关宏 `INIT_D_TEST_EN`（设为 `1`）后编译运行，可直观验证优先级排序效果：

```c
#define INIT_D_TEST_EN 1
```

该测试注册了 4 个初始化函数，优先级分别为 `-1`、`0`、`1`、`10`。由于数字越小优先级越高，预期执行顺序为 `-1` → `0` → `1` → `10`。

终端实际输出如下（已去除颜色标记）：

```bash
[INFO] [scheduler]调度器初始化成功
[INFO] 执行_EXPORT_INIT_SYMBOL导出的初始化程序
[INFO] [init_d test] priority -1 executed
[INFO] [init_d test] priority 0 executed
[INFO] [init_d test] priority 1 executed
 _     _        ____ _     ___
| |   (_)_ __  / ___| |   |_ _|
| |   | | '_ \| |   | |    | |
| |___| | | | | |___| |___ | |
|_____|_|_| |_|\____|_____|___|
[INFO] 欢迎使用LinCLI
LinCLI  Copyright (C) 2026  bunnydeny
This program comes with ABSOLUTELY NO WARRANTY; for details type `show -w'.
This is free software, and you are welcome to redistribute it
under certain conditions; type `show -c' for details.

[INFO] [init_d test] priority 10 executed

lin@linCli>
```

可以看到，`-1`（最高优先级）最先执行，`10`（最低优先级）最后执行，且 logo（优先级 `10`）与 license（优先级 `10`）作为同优先级项，其相对顺序由红黑树插入顺序决定。

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

LinCLI 依赖链接器将分散在各个目标文件中的自定义段（`.cli_commands`、`.scheduler` 等）聚合成连续的符号数组。由于不同平台的链接脚本语法和内存模型不同，需要对 `cli.ld` 做少量适配。

#### 适配原理：为什么 MCU 需要 `>FLASH`，而 x86/Linux 不需要？

| 平台 | 内存模型 | 是否需要 `>REGION` |
|------|----------|-------------------|
| **MCU（Cortex-M 等）** | 采用**分离的存储映射**：Flash 存放只读代码/常量，RAM 存放可变数据。链接脚本必须显式声明 `MEMORY { FLASH (rx) : ORIGIN = ... }`，并为每个段指定目标存储器（如 `>FLASH`），否则链接器无法确定该段应落入 Flash 还是 RAM。 | **必须** |
| **x86_64 Linux** | 采用**统一的虚拟地址空间**，操作系统通过页表将 ELF 加载段映射到物理内存。GCC 的默认链接脚本不定义 `FLASH` 这类区域名，而是通过 `SECTIONS` 块内的相对地址（`. = ...`）顺序排布各段。自定义段只要按序放置，链接器就会自动将其纳入可执行文件的相应加载段，无需显式指定存储区域。 | **不需要** |

简言之，`>FLASH` 是**嵌入式链接器脚本显式管理物理存储映射**的语法；在 x86/Linux 上，物理地址由操作系统运行时决定，链接阶段只需关心虚拟地址空间中的排布顺序即可。

#### MCU 适配示例

在 MCU 工程中，修改 `cli.ld`，为每个自定义段追加 `>FLASH`：

```ld
  .cli_commands : {
    _cli_commands_start = .;
    KEEP(*(.cli_commands))
    _cli_commands_end = .;
  } >FLASH
  .cli_cmd_line : {
    _cli_cmd_line_start = .;
    KEEP(*(.cli_cmd_line))
    _cli_cmd_line_end = .;
  } >FLASH
  .scheduler : {
    _scheduler_start = .;
    KEEP(*(.scheduler))
    _scheduler_end = .;
  } >FLASH
  .my_init_d : {
    _init_d_start = .;
    KEEP(*(.my_init_d))
    _init_d_end = .;
  } >FLASH
  .dispose : {
    _dispose_start = .;
    KEEP(*(.dispose))
    _dispose_end = .;
  } >FLASH
  .alias_cmd : {
    _alias_cmd_start = .;
    KEEP(*(.alias_cmd))
    _alias_cmd_end = .;
  } >FLASH
```

然后在 MCU 的链接脚本中通过 `INCLUDE cli.ld` 引入上述定义。以下以 STM32 工程为例：

```ld
  ...
  .fini_array (READONLY) : /* "READONLY" 关键字仅在 GCC 11 及以上版本支持，GCC 10 及以下请删除。 */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* 引入自定义段收集脚本；也可直接将 cli.ld 的内容复制到此位置，效果相同 */
  INCLUDE cli.ld

  /* 供启动文件初始化 .data 段使用 */
  _sidata = LOADADDR(.data);

  /* 已初始化数据段放入 RAM，其加载副本（LMA）紧随代码之后 */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* data 段起始全局符号 */
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)

    . = ALIGN(4);
    _edata = .;        /* data 段结束全局符号 */
  } >RAM AT> FLASH
  ...
```

> **不要畏惧 `default.ld` 的篇幅**：它看起来很长，但其实只是在 GCC 为 x86_64 Linux 平台生成的**默认链接脚本**基础上，增加了寥寥几行段收集代码而已。你真正需要关心的，就是本文档中提到的这几个自定义段及其起始/结束符号。
>
> 如果你使用的是 Keil MDK（ARMCC/AC6 工具链），对应的链接脚本格式是 `.sct`（分散加载文件）。迁移逻辑同样直接——核心思路仍然是「把同一 section 名的内容收集到连续区域，并导出该区域的起始和结束符号」。具体语法细节请自行搜索 "MDK SCT scatter file collect section start end symbol" 等关键词。

### 5. 选择串口终端工具

移植完成后，你需要通过串口终端工具连接单片机，与 LinCLI 进行交互。以下是不同平台下的常用工具推荐：

- **Linux**
  - **`picocom`**：轻量、稳定，推荐首选。安装命令：`sudo apt install picocom`
  - 其他替代：`minicom`、`screen`、`tio` 等

- **Windows**
  - **`PuTTY`**：经典免费串口终端工具，简单易用
  - **`MobaXterm`**：功能丰富，集成串口、SSH、FTP 等多种协议
  - 由于 Keil MDK 等单片机开发工具链多在 Windows 环境下使用，上述工具非常适合搭配开发

- **跨平台（推荐）**
  - **VS Code 插件 `Serial Terminal`**：直接在 VS Code 中打开串口，支持 Windows / Linux / macOS，界面简洁，无需额外安装独立软件

无论使用哪种工具，只需配置好正确的串口号和波特率，连接后即可看到 LinCLI 的命令提示符，开始输入命令。

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
    size_t tags_count;   /* INT_ARRAY 的长度，字段名必须是 tags_count（与 tags 对应） */
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
| 6 | `tags` | **结构体字段名**。解析成功后，结果会写入 `args->tags`。对于 `INT_ARRAY`，该字段必须是 `int *` 类型。框架会自动寻找同名的 `_count` 字段（如 `tags` → `tags_count`）来存放实际解析到的元素个数。 |
| 7 | `8` | **最大参数个数**（仅 `INT_ARRAY` 有效）。表示该数组选项最多接收 8 个整数，同时框架也会在 `arg_buf` 尾部静态预留 `8 × sizeof(int)` 字节的连续空间用于存放解析结果。如果 `arg_buf` 尾部剩余空间不足，解析会直接失败。 |
| 8 | `"!verbose"` | **依赖/互斥字符串**。<br>• 普通字符串（如 `"verbose"`）→ **依赖**：表示该选项只有在 `-v`/`--verbose` 也出现时才合法。<br>• 以 `!` 开头的字符串（如 `"!verbose"`）→ **互斥**：表示该选项与 `-v`/`--verbose` **不能同时出现**。 |
| 9 | `true` | **是否必需**（`required`）。`true` 表示用户必须提供该选项，否则报错。 |

> **INT_ARRAY 使用约束**
>
> 1. **字段名配对**：若 `INT_ARRAY` 的字段名为 `xxx`，则结构体中**必须存在**名为 `xxx_count` 的字段（类型通常为 `size_t`），用于存放实际解析到的数组长度。该字段在结构体中的位置没有强制要求。
> 2. **初始化为 `NULL`**：`int *xxx` 字段在解析前必须保证为 `NULL`（框架会在 `cli_auto_parse` 开始时 `memset(arg_struct, 0, ...)` 清零，因此默认即可满足）。如果用户手动将其设为某个非 `NULL` 指针，框架会直接把解析结果写入该地址，**不再进行任何边界检查**，可能导致越界。
> 3. **静态缓冲区上限**：`max_args` 同时决定了"允许用户输入的最大个数"和"框架静态预留的连续空间大小"。如果尾部剩余空间不足，即使只输入 1 个整数也会直接报错"缓冲区不足"。

对于参数较少的重载，后面的参数依次省略即可。例如上面的 `OPTION('f', "file", STRING, "Log file path", struct log_args, file, true)` 是 **7 参数**形式，最后一个 `true` 表示 `required`；而 `OPTION('t', "tags", INT_ARRAY, "Tag list", struct log_args, tags, 8, "!verbose")` 是 **8 参数**形式，`required` 缺省为 `false`。

### 第 4 步：编译并运行

把这个 `.c` 文件加入你的构建系统后重新编译。无需修改 `main()`，也无需手动注册，链接器会自动把它放入 `.cli_commands` 段。运行程序后即可在终端输入：

```bash
lin@linCli> log -f /tmp/app.log -l 3 -t 1 2 3
LOG command executed!
  file  = /tmp/app.log
  level = 3
  tags  = 1 2 3
lin@linCli>
```

如果缺少必需选项：

```bash
lin@linCli> log -f /tmp/app.log
[ERR] 缺少必需选项: -l/--level
[ERR] 命令解析失败: log
...
lin@linCli>
```

如果触发互斥：

```bash
lin@linCli> log -f /tmp/app.log -l 3 -v -t 1 2 3
[ERR] 选项 -t/--tags 与 verbose 互斥，不能同时使用
[ERR] 命令解析失败: log
...
lin@linCli>
```

也可以只带 `-v` 不带 `-t`：

```bash
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

```bash
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
## alias-命令重命名
通过`CMD_ALIAS`宏来重命名你的命令，这适用于简化已经定义的一些复杂命令
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
	    OPTION('m', "msg", STRING, "Message text", struct string_args, msg),
	    END_OPTIONS);
CMD_ALIAS(echo, "ts --msg");
```
首先通过`CLI_COMMAND宏`注册了一个`ts`命令，这个命令可以通过`ts --msg hello world`向终端打印`hello world`字符串
可以通过下面的方式将`ts --msg`这个较复杂的命令重命名为echo这样的简短命令：
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

## 测试用例与终端操作示例

所有测试用例位于 `tests/` 目录，编译时会被自动收集并链接进 `a.out`。下表列出每个命令的含义、可用选项以及实际终端操作日志。

### 1. `tb` — BOOL 类型选项测试

**命令描述**：测试 BOOL 开关选项。  
**选项**：`-v, --verbose`（启用详细输出）

```bash
lin@linCli> tb -v
BOOL test executed!
  verbose = true
lin@linCli>
```

### 2. `ts` — STRING 类型选项测试

**命令描述**：测试字符串选项。  
**选项**：`-m, --msg <text>`（消息文本）

```bash
lin@linCli> ts -m hello
 hello

lin@linCli>
```

### 3. `ti` — INT 类型选项测试

**命令描述**：测试单个整数选项。  
**选项**：`-n, --num <value>`（整数值）

```bash
lin@linCli> ti -n 42
INT test executed!
  num = 42
lin@linCli>
```

### 4. `td` — DOUBLE 类型选项测试

**命令描述**：测试浮点数选项。  
**选项**：`-f, --factor <value>`（浮点值）

```bash
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

```bash
lin@linCli> ta -v -n 1 2 3
INT_ARRAY test executed!
  verbose = true
  nums =  1  2  3
lin@linCli>
```

如果缺少依赖的 `-v`，会报错：

```bash
lin@linCli> ta -n 1 2 3
[ERR] 选项 -n/--nums 依赖 verbose 但未提供
[ERR] 命令解析失败: ta
...
lin@linCli>
```

### 6. `tc` — CALLBACK 类型选项测试

**命令描述**：测试自定义回调选项，框架仅将原始字符串传给 handler。  
**选项**：`-c, --cfg <raw>`（原始配置字符串）

```bash
lin@linCli> tc -c foo
CALLBACK test executed!
  custom callback triggered with: foo
lin@linCli>
```

### 7. `tr` — REQUIRED 必需选项测试

**命令描述**：测试选项的 `required=true` 校验。  
**选项**：`-f, --file <path>`（STRING，**必需**）

正常输入：

```bash
lin@linCli> tr -f /tmp/data.txt
REQUIRED test executed!
  file = /tmp/data.txt
lin@linCli>
```

缺少必需选项：

```bash
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

```bash
lin@linCli> tcf -n 1 2 3
CONFLICTS test executed!
  nums =  1  2  3
lin@linCli>
```

同时使用互斥选项会报错：

```bash
lin@linCli> tcf -v -n 1 2 3
[ERR] 选项 -n/--nums 与 verbose 互斥，不能同时使用
[ERR] 命令解析失败: tcf
...
lin@linCli>
```

### 9. `tw` — CLI_COMMAND_WITH_BUF 独立缓冲区测试

**命令描述**：演示为命令指定独立的静态参数解析缓冲区（而非共享 `g_cli_cmd_buf`）。  
**选项**：
- `-v, --verbose`
- `-n, --nums <list>`（INT_ARRAY，最多 16 个）

```bash
lin@linCli> tw -v -n 10 20 30
WITH_BUF test executed!
  verbose = true
  nums =  10  20  30
lin@linCli>
```

---

## 高级交互功能

### 命令历史

LinCLI 内置了命令历史记录功能，最多保存 **4 条**命令。空命令不会被记录，重复输入相同命令也不会产生冗余条目。

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
lin@linCli> t
```

如果前缀唯一匹配，则直接补全并自动追加空格：

```bash
lin@linCli> tb<Tab>
lin@linCli> tb
```

#### 命令选项补全

在命令名后按空格再按 `Tab`：

- **若该命令只有一个选项**：直接补全该选项的长选项（或短选项），并追加空格：

  ```bash
  lin@linCli> big1 <Tab>
  lin@linCli> big1 --a 
  ```

  再次按 `Tab` 会响铃（`\a`），不会重复补全。

- **若该命令有多个选项**：列出所有选项：

  ```bash
  lin@linCli> log 
  -f --file  -l --level  -v --verbose  -t --tags  
  lin@linCli> log
  ```

输入 `--` 后按 `Tab`，只列出**长选项**（单选项命令则直接补全）：

```bash
lin@linCli> log --
--file  --level  --verbose  --tags  
lin@linCli> log --
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

---

## 用户可定制接口

LinCLI 在设计上保留了多处**弱定义（`weak`）接口**，用户无需修改框架源码，只需在自己的项目中重新实现同名函数，即可覆盖默认行为。

### 日志系统 `cli_printk`

`cli_printk` 的使用方式**一比一复刻 Linux 内核的日志打印函数**。框架推荐直接在**命令响应函数（handler）**中使用它，也推荐在框架内部调试时使用；但需要注意，由于它内部会调用 `cli_out_push` / `cli_out_sync` 进行输出同步，**不能在中断上下文里调用**。如果你的移植代码把串口接收函数放到了中断里，那么该中断函数中也不应调用 `cli_printk`。

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

```bash
MyDevice>
```

---

## 写在最后

LinCLI 的设计目标是：**让命令注册像定义变量一样简单**，同时保持极低的运行时开销。得益于 GCC `section` 属性 + 自定义链接脚本的组合，开发者只需要关心业务命令和选项的定义，剩下的收集、解析、校验工作全部交给框架自动完成。无论是 Linux 仿真开发还是 MCU 裸机移植，都能快速落地。

如有问题或改进建议，欢迎提交 Issue 或 PR。
