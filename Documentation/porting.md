# 移植到单片机


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

### 3. 实现临界区保护 —— 覆盖 `cli_enter_critical` / `cli_exit_critical`

框架在 `cli_io.c` 中定义了一对 **weak** 的空函数作为临界区钩子：

```c
__attribute__((weak)) void cli_enter_critical(void) {}
__attribute__((weak)) void cli_exit_critical(void)  {}
```

所有对 `cli_io.in` / `cli_io.out` 缓冲区的访问（`cli_in_push`、`cli_in_pop`、`cli_get_in_size` 等）都会调用这两个钩子。在 PC 模拟中，我们在 `init/main.c` 里用原子自旋锁实现：

```c
/* PC 模拟层：用原子自旋锁模拟 MCU 的关中断/开中断 */
static volatile int cli_io_spinlock = 0;

void cli_enter_critical(void)
{
    while (__sync_lock_test_and_set(&cli_io_spinlock, 1)) {
    }
}

void cli_exit_critical(void)
{
    __sync_lock_release(&cli_io_spinlock);
}
```

在 MCU 中，**必须覆盖这两个函数**，推荐用保存/恢复 `PRIMASK` 的方式实现（比简单开关中断更安全，不会破坏主程序已有的中断屏蔽状态）：

```c
// 在你的 MCU 项目中实现（如 main.c 或专门的移植层文件）
static uint32_t g_primask_save;

void cli_enter_critical(void)
{
    g_primask_save = __get_PRIMASK();
    __disable_irq();
}

void cli_exit_critical(void)
{
    __set_PRIMASK(g_primask_save);
}
```

> **为什么需要临界区？** 在 PC 上，输入线程和调度线程是真正的多线程并行，必须保护共享的 `cli_io` 缓冲区不被并发破坏。在 MCU 上，串口中断可以随时打断主程序，如果主程序正在 `cli_in_pop` 时被中断里的 `cli_in_push` 插入，同样会破坏 FIFO 的 `head`/`tail`/`size` 指针。通过在进入临界区时关中断、退出时恢复，可以确保对缓冲区的访问是原子的。

> ⚠️ **MCU 上禁止使用 PC 模拟的自旋锁**
>
> PC 模拟中的 `while (__sync_lock_test_and_set(...)) {}` 是一种**忙等待自旋锁**。在 PC 多线程环境下它是安全的，因为一个线程自旋时，另一个线程仍然可以运行并释放锁。
>
> 但在 MCU 上，如果主程序正持有锁（执行 `cli_in_pop` 期间）时触发了串口中断，中断处理程序里也会请求同一把锁。由于中断的优先级高于主程序，中断一旦进入自旋等待，**主程序永远无法恢复执行来释放锁**；同时中断本身也因为等不到锁而一直循环——**这就是死锁**。
>
> 因此，**切勿将 PC 模拟的自旋锁代码原样搬到 MCU 上**。

### 4. 实现单字符输出 —— 替换 `cli_putc`

与临界区钩子一样，`cli_putc` 也是 `cli_io.c` 中定义的 **weak** 函数。PC 模拟层把它和临界区一起放在 `init/main.c` 中：

```c
void cli_putc(char ch)
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

### 5. 链接脚本适配

LinCLI 依赖链接器将分散在各个目标文件中的自定义段聚合成连续的符号数组。由于不同平台的链接脚本语法和内存模型不同，需要对 `cli.ld` 做少量适配。

> **段收集采用三段式布局**
>
> 每个自定义段由 `.0.start`（起始标记）、`.1`（实际内容）、`.1.end`（结束标记）三个子段组成。起始/结束符号 `_xxx_start[]` / `_xxx_end[]` 是在 `init/section_markers.c` 中定义的 C 数组，链接器只需按顺序收集这三个子段即可。

#### 适配原理：为什么 MCU 需要 `>FLASH`，而 x86/Linux 不需要？

| 平台 | 内存模型 | 是否需要 `>REGION` |
|------|----------|-------------------|
| **MCU（Cortex-M 等）** | 采用**分离的存储映射**：Flash 存放只读代码/常量，RAM 存放可变数据。链接脚本必须显式声明 `MEMORY { FLASH (rx) : ORIGIN = ... }`，并为每个段指定目标存储器（如 `>FLASH`），否则链接器无法确定该段应落入 Flash 还是 RAM。 | **必须** |
| **x86_64 Linux** | 采用**统一的虚拟地址空间**，操作系统通过页表将 ELF 加载段映射到物理内存。GCC 的默认链接脚本不定义 `FLASH` 这类区域名，而是通过 `SECTIONS` 块内的相对地址（`. = ...`）顺序排布各段。自定义段只要按序放置，链接器就会自动将其纳入可执行文件的相应加载段，无需显式指定存储区域。 | **不需要** |

简言之，`>FLASH` 是**嵌入式链接器脚本显式管理物理存储映射**的语法；在 x86/Linux 上，物理地址由操作系统运行时决定，链接阶段只需关心虚拟地址空间中的排布顺序即可。

#### 参考示例工程（强烈推荐）

**如果你不确定如何在keil5 MDK工具链中完成stm32的链接脚本适配，请直接参考本项目提供的示例工程：**

```
example_project/LinCLI_1_0_stm32f103c8t6_keil5_mdk_examply_project/
```

该工程是一个基于 **STM32F103C8T6 + Keil MDK (AC5)** 的完整移植示例，展示了：
- `mdk_cli.sct` — 分散加载文件中自定义段的收集规则；
- `Core/Src/main.c` — `cli_enter_critical` / `cli_exit_critical` / `cli_putc`、UART 中断输入（`HAL_UARTEx_RxEventCallback`）以及主循环调度（`scheduler_init` / `scheduler_task`）的完整实现。

你可以将此工程作为模板，替换为目标芯片对应的 HAL 库和启动文件即可快速上手。

#### GCC (LD) 工具链适配示例

在 GCC 环境下（如 STM32CubeIDE、PlatformIO），修改 `cli.ld` 或将其内容直接复制到 MCU 的链接脚本中。每个自定义段需按 `.0.start` → `.1` → `.1.end` 的顺序收集，并指定 `>FLASH`：

```ld
  .cli_commands : {
    KEEP(*(.cli_commands.0.start))
    KEEP(*(.cli_commands.1))
    KEEP(*(.cli_commands.1.end))
  } >FLASH
  .cli_cmd_line : {
    KEEP(*(.cli_cmd_line.0.start))
    KEEP(*(.cli_cmd_line.1))
    KEEP(*(.cli_cmd_line.1.end))
  } >FLASH
  .scheduler : {
    KEEP(*(.scheduler.0.start))
    KEEP(*(.scheduler.1))
    KEEP(*(.scheduler.1.end))
  } >FLASH
  .my_init_d : {
    KEEP(*(.my_init_d.0.start))
    KEEP(*(.my_init_d.1))
    KEEP(*(.my_init_d.1.end))
  } >FLASH
  .dispose : {
    KEEP(*(.dispose.0.start))
    KEEP(*(.dispose.1))
    KEEP(*(.dispose.1.end))
  } >FLASH
  .alias_cmd : {
    KEEP(*(.alias_cmd.0.start))
    KEEP(*(.alias_cmd.1))
    KEEP(*(.alias_cmd.1.end))
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

> **不要畏惧 `default.ld` 的篇幅**：它看起来很长，但其实只是在 GCC 为 x86_64 Linux 平台生成的**默认链接脚本**基础上，通过 `INCLUDE cli.ld` 引入段收集规则而已。你真正需要关心的，就是本文档中提到的这几个自定义段及其子段顺序。

#### Keil MDK (AC6 / ARMCC) 适配示例

Keil MDK 使用 `.sct`（分散加载文件）作为链接脚本。核心思路同样是「按顺序收集三个子段，确保标记数组位于首尾」。以下示例直接取自本项目的 STM32 示例工程：

```sct
  ; -----------------------------------------------------------
  ; 自定义段区域：等价于 cli.ld 中的 KEEP(*(.xxx))
  ; 作为独立 Execution Region 紧跟在 ER_IROM1 之后，
  ; 以便 ARM 链接器为每个段生成独立的 Base/Limit 符号。
  ; 顺序必须位于 .ANY (+RO) 之前，否则会被 .ANY 优先吸收。
  ; -----------------------------------------------------------
  ER_CLI_COMMANDS +0 {
    *(.cli_commands.0.start)
    *(.cli_commands.1)
    *(.cli_commands.1.end)
  }
  ER_CLI_CMD_LINE +0 {
    *(.cli_cmd_line.0.start)
    *(.cli_cmd_line.1)
    *(.cli_cmd_line.1.end)
  }
  ER_SCHEDULER +0 {
    *(.scheduler.0.start)
    *(.scheduler.1)
    *(.scheduler.1.end)
  }
  ER_INIT_D +0 {
    *(.my_init_d.0.start)
    *(.my_init_d.1)
    *(.my_init_d.1.end)
  }
  ER_DISPOSE +0 {
    *(.dispose.0.start)
    *(.dispose.1)
    *(.dispose.1.end)
  }
  ER_ALIAS_CMD +0 {
    *(.alias_cmd.0.start)
    *(.alias_cmd.1)
    *(.alias_cmd.1.end)
  }
  ER_RO_REST +0 {
    .ANY (+RO)
    .ANY (+XO)
  }
```

各段之间使用 `+0` 表示紧跟前一个区域排布，无需额外填充。放置顺序**必须在** `.ANY (+RO)` 之前，否则普通目标文件中的段会被 `.ANY` 优先吸收，导致自定义段分散到不同位置甚至丢失。

### 6. 选择串口终端工具

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
