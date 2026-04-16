# LinCLI

LinCLI 是一个面向嵌入式/MCU 场景的 C 语言命令行交互框架。它采用**链接脚本段自动收集**机制，将命令、状态机、初始化函数的注册工作完全交给编译器与链接器，开发者只需用宏定义命令和选项即可。框架具备极低的内存开销、清晰的层次结构，并内建命令历史、Tab 补全、选项依赖/互斥检查、重复选项检测等实用功能。

---

## 目录

1. [环境搭建](#环境搭建)
2. [编译与运行](#编译与运行)
3. [项目结构与核心机制](#项目结构与核心机制)
4. [如何移植到单片机](#如何移植到单片机)
5. [测试用例与终端操作示例](#测试用例与终端操作示例)
6. [高级交互功能](#高级交互功能)
   - [命令历史](#命令历史)
   - [Tab 补全](#tab-补全)

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

## 写在最后

LinCLI 的设计目标是：**让命令注册像定义变量一样简单**，同时保持极低的运行时开销。得益于 GCC `section` 属性 + 自定义链接脚本的组合，开发者只需要关心业务命令和选项的定义，剩下的收集、解析、校验工作全部交给框架自动完成。无论是 Linux 仿真开发还是 MCU 裸机移植，都能快速落地。

如有问题或改进建议，欢迎提交 Issue 或 PR。
