# 🎮 Raw 命令（argc/argv 模式）

> 📝 **开关**：`make menuconfig` → `LinCLI Core → CLI Features → Enable raw argument commands`（默认关闭）。
>
> 💡 **不熟悉 Kconfig？** 请参考 [**Kconfig 配置完全指南**](kconfig_user_guide.md)。

---

## 🤔 什么时候需要 Raw 命令？

LinCLI 的核心设计是**选项解析范式**——用 `OPTION` 宏定义 `--on`、`-b` 等带名字的参数，框架自动帮你做解析、校验、补全。这对配置类命令非常友好。

但有些场景更适合**传统 Shell 风格**：

```bash
cp src.txt dst.txt
mv old.txt new.txt
cat log.txt | grep error
```

这些命令的参数是**位置敏感的**（第一个是源文件，第二个是目标文件），没有 `--src=` 这种选项前缀。用 `OPTION` 宏来表达会很别扭。

**Raw 命令**就是为这种场景设计的：它跳过选项解析，直接把整行输入拆成 `argc/argv`，交给你的 handler 自由处理。

---

## 🆚 Raw 命令 vs 选项解析命令

| 特性 | 选项解析命令 (`CLI_COMMAND`) | Raw 命令 (`CLI_RAW_COMMAND`) |
|------|------------------------------|------------------------------|
| 参数风格 | `--option value` | `value1 value2 value3` |
| 解析方式 | 框架自动解析到结构体字段 | 直接传 `argc/argv` |
| handler 签名 | `int handler(void *args)` | `int handler(char **argv, int argc)` |
| Tab 补全 | 选项名 + 选项值 | 仅参数值 |
| 帮助系统 | ✅ 自动生成 | ✅ 自动生成（含 usage） |
| 异步支持 | ✅ (`CLI_COMMAND_ASYNC`) | ✅ (`CLI_RAW_COMMAND_ASYNC`) |

---

## ⚡ 同步 Raw 命令

> 参考源码：`tests/commands/test_raw_cmd.c`

### 第 1 步：实现 handler

```c
#include "cmd_dispose.h"
#include "cli_io.h"

static int cp_handler(char **argv, int argc)
{
    cli_printk("argc=%d\r\n", argc);
    for (int i = 0; i < argc; i++)
        cli_printk("argv[%d]=%s\r\n", i, argv[i]);
    return 0;
}
```

handler 接收 `argc/argv`，跟 `main()` 一模一样。`argv[0]` 是命令名本身（这里是 `"cp"`）。

### 第 2 步：注册命令

```c
CLI_RAW_COMMAND(cp_cmd, "cp", "Copy files",
                USAGE("cp <src> <dst>"),
                cp_handler,
                "file1.txt", "file2.txt", "file3.txt");
```

宏参数详解：

| 参数 | 含义 |
|------|------|
| `cp_cmd` | C 标识符名，用于生成内部静态符号 |
| `"cp"` | 命令字符串，终端实际输入的名字 |
| `"Copy files"` | 命令简介，`-h` 时显示 |
| `USAGE(...)` | 用法示例数组 |
| `cp_handler` | handler 函数，`int (*)(char **, int)` |
| `"file1.txt"`, ... | **候选值**（变参），Tab 补全时会列出 |

### 第 3 步：终端体验

```bash
lin@linCli> cp src.txt dst.txt
argc=3
argv[0]=cp
argv[1]=src.txt
argv[2]=dst.txt
```

**Tab 补全参数值**：

```bash
lin@linCli> cp fi<Tab>
# 补全为 file1.txt / file2.txt / file3.txt
```

**查看帮助**：

```bash
lin@linCli> cp -h
cp - Copy files
use: cp <src> <dst>
  - , --raw          raw command (no options)
```

---

## 🔄 异步 Raw 命令

> 参考源码：`tests/commands/test_raw_cmd_async.c`

耗时操作（如大文件移动、网络传输）不应阻塞 CLI 主循环。Raw 命令同样支持 `entry/task/exit` 三阶段异步模型。

### 实现三阶段 handler

```c
#include "cmd_dispose.h"
#include "cli_io.h"

static int mv_tick;

static void mv_entry(char **argv, int argc)
{
    cli_printk("[entry] argc=%d\r\n", argc);
    for (int i = 0; i < argc; i++)
        cli_printk("  argv[%d]=%s\r\n", i, argv[i]);
    mv_tick = 0;
}

static int mv_task(char **argv, int argc)
{
    cli_printk("[task] argc=%d tick=%d\r\n", argc, mv_tick);
    mv_tick++;
    if (mv_tick >= 3)
        return 0;           /* 完成，进入 exit */
    return CLI_CONTINUE;    /* 下次调度继续执行 */
}

static void mv_exit(char **argv, int argc)
{
    cli_printk("[exit] argc=%d\r\n", argc);
}
```

### 注册异步命令

```c
CLI_RAW_COMMAND_ASYNC(mv_cmd, "mv", "Move files",
                      USAGE("mv <old> <new>"),
                      mv_entry, mv_task, mv_exit,
                      "doc1.txt", "doc2.txt");
```

### 终端体验

```bash
lin@linCli> mv old.txt new.txt
[entry] argc=3
  argv[0]=mv
  argv[1]=old.txt
  argv[2]=new.txt
[task] argc=3 tick=0
[task] argc=3 tick=1
[task] argc=3 tick=2
[exit] argc=3
```

三阶段语义与 `CLI_COMMAND_ASYNC` 完全一致：
- 🔹 `entry` — 一次性初始化，参数已就绪
- 🔄 `task` — 多次调度，返回 `CLI_CONTINUE` 表示继续，返回 `0` 表示完成
- 🔹 `exit` — 清理收尾

---

## 🧩 技术细节

### 零侵入设计

Raw 命令**没有新增任何 linker section**，也没有新增 `cli_command_t` 字段。它复用 `.cli_commands` 段和现有的 `options[0]` 机制：

- 宏内部自动生成一个隐藏的 `--raw` 选项
- 候选值写进 `--raw` 选项的 `candidate_argv`，补全逻辑直接复用值补全引擎
- 同步 raw 检测 `cmd_entry == NULL`，异步 raw 通过 `void *` 包装函数传递 `raw_cmd_args_t`

### 配置开关

```bash
make menuconfig
# LinCLI Core → CLI Features
#   [*] Enable raw argument commands
#
# Tests & Demos → Demo Commands
#   [*] Enable raw command test demo        (cp)
#   [*] Enable async raw command test demo  (mv)
```

Demo 开关通过 `select CLI_ENABLE_RAW_COMMAND` 自动拉通底层依赖，无需手动逐层开启。

---

## 💡 最佳实践

1. **配置类命令用 `CLI_COMMAND`** — 参数有明确语义（`--brightness`），享受框架的依赖/互斥/校验
2. **操作类命令用 `CLI_RAW_COMMAND`** — 参数是位置敏感的（`cp src dst`），handler 自由处理
3. **始终传入 `USAGE(...)`** — 帮助信息对用户体验至关重要，不要留空
4. **提供候选值** — 即使只是示例文件名，也能让 Tab 补全发挥价值
