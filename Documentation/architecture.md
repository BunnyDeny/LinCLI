# 项目结构与核心机制

cli/
├── cli/              # CLI 核心：命令行编辑、状态机、命令分发
│   ├── Inc/          # 头文件
│   └── Src/          # 源文件
├── init/             # 初始化与线程入口（PC 模拟）
│   ├── main.c        # 主线程 + 双 pthread 模拟输入/调度
│   └── scheduler.c   # 基于状态机的任务调度器
├── lib/              # 基础库：状态机、向量、红黑树
├── tests/            # 测试用例（全部链接到单个 a.out）
├── cli.ld            # 自定义段收集规则
├── default.ld        # PC 端 GCC 默认链接脚本（通过 INCLUDE cli.ld 引入段收集）
└── CMakeLists.txt    # 顶层构建配置
```

### 链接脚本段自动注册

`cli.ld` 中定义了若干自定义段：

- `.cli_commands` — 存放所有注册的命令定义
- `.cli_cmd_line` — 存放命令行状态机的状态节点
- `.scheduler`    — 存放调度器任务
- `.my_init_d`    — 存放初始化函数
- `.dispose`      — 存放 dispose 状态机
- `.alias_cmd`    — 存放命令别名

每个自定义段内部采用**三段式布局**（`.0.start`、`.1`、`.1.end`）：
- `.0.start` 与 `.1.end` 是 `init/section_markers.c` 中定义的标记数组，值为 `NULL`，分别位于段的首尾；
- `.1` 是各 C 文件通过宏注册的实际内容指针数组。

链接阶段由链接脚本按 `*.0.start` → `*.1` → `*.1.end` 的顺序收集成连续数组。框架运行时使用段起始/结束符号（如 `_cli_commands_start` / `_cli_commands_end`）遍历，遍历时遇到 `NULL` 会自动跳过，**无需手动维护注册表**。

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
                   struct conflicts_args, verbose, 0, NULL, NULL, false),
            OPTION('n', "nums", INT_ARRAY, "Number list",
                   struct conflicts_args, nums, 8, NULL, "verbose", false),
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
