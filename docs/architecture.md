# 🎯 项目结构与核心机制
```
.
├── include/          # 公共头文件（对外 API）
│   ├── cmd_dispose.h
│   ├── cli_io.h
│   ├── cli_cmd_line.h
│   └── ...
├── src/              # 框架源码（CMake OBJECT 库）
│   ├── cli/          # CLI 核心：命令行编辑、状态机、命令分发
│   ├── lib/          # 基础库：状态机、向量、字符串、内存池
│   └── init/         # 初始化与调度器
├── tests/            # 测试
│   ├── commands/     # 集成测试命令（链接到 a.out 演示）
│   └── unit/         # Unity 单元测试
├── examples/         # 移植示例工程
│   ├── pc_linux/     # PC 端 GCC 演示（双 pthread 模拟）
│   ├── stm32_g431/   # STM32G431 GCC 示例
│   ├── stm32f103_keil/  # STM32F103 Keil MDK 示例
│   └── external_demo/   # 外部项目集成演示
├── docs/             # 文档（本目录）
├── cli.ld            # 自定义段收集规则
├── default.ld        # PC 端 GCC 默认链接脚本
├── CMakeLists.txt    # 顶层构建配置
└── Makefile          # 顶层 Makefile
```

### 💡 链接脚本段自动注册

`cli.ld` 中定义了若干自定义段：

- ✅ `.cli_commands`   — 存放所有注册的命令定义
- 🔹 `.cli_cmd_line`  — 存放命令行状态机的状态节点
- 📌 `.scheduler`     — 存放调度器任务
- 🆕 `.my_init_d`     — 存放初始化函数
- 💎 `.cli_vars`      — 存放通过 `CLI_VAR` / `CLI_VAR_CUSTOM` 注册的变量描述符
- ✅ `.cli_var_types` — 存放通过 `CLI_VAR_TYPE` 注册的自定义类型操作回调

每个自定义段内部采用**三段式布局**（`.0.start`、`.1`、`.1.end`）：
- 🔹 `.0.start` 与 `.1.end` 是 `src/init/section_markers.c` 中定义的标记数组，值为 `NULL`，分别位于段的首尾；
- 📌 `.1` 是各 C 文件通过宏注册的实际内容指针数组。

链接阶段由链接脚本按 `*.0.start` → `*.1` → `*.1.end` 的顺序收集成连续数组。框架运行时使用段起始/结束符号（如 `_cli_commands_start` / `_cli_commands_end`）遍历，遍历时遇到 `NULL` 会自动跳过，**无需手动维护注册表**。

---

### ⚠️ 为什么 LinCLI 必须使用对象文件（OBJECT），而不能打包成静态库（STATIC）

LinCLI 的段收集机制看似只是"链接脚本 + 编译器属性"的组合，但它与**静态库的按需提取（lazy extraction）机制**存在根本冲突。如果你把 LinCLI 编译成 `.a` 静态库再链接，段里的内容会**部分甚至全部丢失**，导致命令、初始化函数、状态机等注册项在运行时"神秘消失"。这个问题在 `cli_logo.c`（版本号打印）上尤其隐蔽——编译链接都成功，运行时却没有任何报错，只是少了 banner。

#### 1. 静态库是怎么被链接器处理的

静态库（`liblincli.a`）本质上是一个 `.o` 文件的归档。GNU ld 处理它的规则是：

1. 链接器维护一个**未定义符号表**（UND）。
2. 从左到右扫描输入文件。
3. 遇到 `.a` 时，**只提取那些包含当前 UND 中某个符号定义的对象文件**。
4. 提取出来的 `.o` 会解析其中定义的所有符号；如果有新定义，UND 减少；如果有新引用，UND 增加。
5. 如果一个 `.o` 中的**所有符号都没有出现在 UND 中**，链接器**根本不会把它从 `.a` 里拉出来**。

这一步只看**符号引用关系**，**不看段内容**。链接器在决定"是否提取对象文件"时，完全不知道这个 `.o` 里有哪些自定义段、段里有什么数据。

#### 2. LinCLI 的注册符号恰恰是"无人引用"的

以 `_EXPORT_INIT_SYMBOL` 为例，它在 C 文件里展开后大致长这样：

```c
static struct init_d init_d_logo = { .priority = 21, ... };
static struct init_d *const _init_d_ptr_logo
    __attribute__((used, section(".my_init_d.1"))) = &init_d_logo;
```

关键点：
- `_init_d_ptr_logo` 是 `static` 的，文件外不可见。
- 没有任何 C 代码去 `extern` 引用 `_init_d_ptr_logo`。
- 它的"使用者"是链接脚本里的 `KEEP(*(.my_init_d.1))`，但链接器在**提取阶段**根本不检查段内容。

于是链接器看到 `cli_logo.o` 时，检查 UND 表——没有人引用 `cli_logo.o` 里的任何符号——**直接跳过，整个对象文件被丢弃**。即使链接脚本里写了 `KEEP(*(.my_init_d.1))`，也保不住一个根本没被提取出来的 `.o`。

`CLI_COMMAND`、`_EXPORT_STATE_SYMBOL` 等宏同理：它们产生的 `_cli_cmd_ptr_xxx`、`_state_ptr_xxx` 都是 `static` 且无人显式引用的符号，在静态库场景下全部面临被丢弃的风险。

#### 3. 对象文件直接传递为什么能工作

当对象文件直接传给链接器（而不是藏在 `.a` 里），链接器必须处理它的所有段：

- 符号表被读取（即使没有外部引用）。
- 自定义段的内容被保留（因为链接脚本里有 `KEEP`）。
- `.my_init_d` 段最终包含 `_init_d_start` + `_init_d_ptr_logo` + `_init_d_ptr_xxx` + `_init_d_end`，运行时遍历就能正常找到所有初始化函数。

这正是 CMake `OBJECT` 库的工作方式：编译生成 `.o`，但不归档成 `.a`，而是直接把对象文件列表传递给最终链接命令。

#### 4. CMake 中的实现

```cmake
# src/CMakeLists.txt
add_library(lincli_obj OBJECT ${LINCLI_SRC})
# ... include dirs ...

# INTERFACE 包装器：对外保持 target_link_libraries(my_app PRIVATE lincli) 的 API
add_library(lincli INTERFACE)
target_sources(lincli INTERFACE $<TARGET_OBJECTS:lincli_obj>)
target_include_directories(lincli INTERFACE ...)
```

- `lincli_obj` 是内部 `OBJECT` 库，编译但不打包。
- `lincli` 是 `INTERFACE` 库，通过 `$<TARGET_OBJECTS:lincli_obj>` 把对象文件暴露给消费者。
- 外部项目（以及内部测试、演示程序）仍然可以写 `target_link_libraries(my_app PRIVATE lincli)`，用法和静态库完全一致。
- 对 CMake 3.10 完全兼容。

#### 5. 如果外部项目坚持用静态库怎么办

如果你的构建系统只能产生 `.a`（例如要通过包管理器分发预编译库），唯一可靠的补救手段是在**消费者端**使用链接器的 `--whole-archive`（GNU ld / LLVM lld）或 `-force_load`（Apple ld）选项强制提取静态库中的所有对象文件：

```cmake
target_link_libraries(my_app PRIVATE
    $<$<PLATFORM_ID:Linux,Darwin>:-Wl,--whole-archive>
    lincli_static
    $<$<PLATFORM_ID:Linux,Darwin>:-Wl,--no-whole-archive>
)
```

但这会引入平台相关的链接器标志，且会把库中所有 `.o`（包括你可能用不到的模块）全部链接进最终镜像，增加体积。因此**不推荐**——LinCLI 的设计意图就是通过源码级 `add_subdirectory()` 集成，让 OBJECT 库自然工作。

### ⚙️ `CLI_COMMAND` 宏 —— 一行代码注册一个命令

LinCLI 的核心理念是：**命令注册应当像定义变量一样简单**。你不需要在 `main()` 里写任何初始化代码，也不需要调用注册函数，只要在 C 文件里用 `CLI_COMMAND` 宏写一次，链接器就会自动把它收集到系统的命令表中。

下面以 `tests/commands/test_conflicts.c` 为例（需先在 `make menuconfig` 中开启 `CLI_ENABLE_DEMO_CONFLICTS` 并重新编译才能体验），这是一个包含**互斥选项**、**整数数组**和**依赖关系**的相对复杂的命令：

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
            USAGE("tcf [-v] [-n <nums...>]"),
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

### 🛡️ 双线程/双角色模型（PC 模拟）

| 角色 | 文件 | 职责 |
|------|------|------|
| 输入生产者 | `examples/pc_linux/main.c` 的 `cli_in_entry` | 从 `getchar()` 读取单字节，推入 `cli_io.in` FIFO |
| 调度消费者 | `examples/pc_linux/main.c` 的 `cli_task_thread_entry` | 循环执行 `scheduler_task()`，从 FIFO 中消费字符并驱动状态机 |

这种输入/处理解耦的设计，使得向 MCU 移植时只需替换输入端（把 `cli_in_push` 放到 UART 中断里即可）。

---

## 🚀 开机初始化函数 `_EXPORT_INIT_SYMBOL`
