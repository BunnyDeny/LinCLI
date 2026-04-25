# 开机初始化函数


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
