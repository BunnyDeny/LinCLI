# Tab 补全候选列表

当你为一个命令注册了 `STRING` 类型的选项后，用户输入时可能需要反复敲打相同的字符串——比如文件路径、设备名、配置项等。LinCLI 提供 `CLI_CANDIDATE` 宏，让你可以为任意命令选项预先定义一组候选值，用户在终端中按 `Tab` 时即可自动补全或列出候选项。

---

## 基本用法

### 1. 注册候选列表

在你的 `.c` 源文件中（通常与对应命令写在同一文件或专门的候选文件里）：

```c
#include "cli_candidate.h"

CLI_CANDIDATE(log_file, "log", "file",
              CANDIDATES("app.log", "app.cfg", "app.txt",
                         "debug.log", "system.log"));
```

**参数说明：**

| 参数 | 含义 |
|------|------|
| `log_file` | 宏实例名，需全局唯一，仅用于生成内部静态符号 |
| `"log"` | 该候选所属的命令名 |
| `"file"` | 该候选对应的长选项名（不带 `--` 前缀） |
| `CANDIDATES(...)` | 候选值字符串数组辅助宏，用法和 `USAGE(...)` 完全一致 |

### 2. 配合命令选项定义

确保对应命令的 `OPTION` 定义中，长选项名与 `CLI_CANDIDATE` 中指定的名称一致：

```c
CLI_COMMAND(log, "log", "Configure logger",
    USAGE("log -f <file> -l <level> [-v] [-t <tags...>]"),
    log_handler, (struct log_args *)0,
    OPTION('f', "file", STRING, "Log file path", struct log_args,
           file, 0, NULL, NULL, true),
    ...
    END_OPTIONS);
```

> **注意**：候选列表仅对 `STRING` 类型选项生效。`BOOL`、`INT`、`DOUBLE` 等类型不需要也不支持候选列表。

---

## 终端效果

定义好候选后，用户在终端中的体验如下：

### 列出所有候选

输入命令和选项后按 `Tab`：

```bash
lin@linCli> log -f <Tab>
app.cfg    app.log    app.txt    debug.log    system.log
```

### 前缀补全

输入部分字符后按 `Tab`，若前缀唯一则直接补全：

```bash
lin@linCli> log -f deb<Tab>
lin@linCli> log -f debug.log 
```

若前缀不唯一则列出所有匹配的候选项，再次按 `Tab` 进入高亮循环模式，可用方向键或 `Tab` 键切换选择。

---

## 高亮循环模式（v1.1.1+）

按 `Tab` 弹出候选列表后，再次按 `Tab` 进入高亮循环模式：

- **`Tab`** / **`↓`** / **`→`**：切换到下一个候选
- **`↑`** / **`←`**：切换到上一个候选
- 选中的候选项会以反白形式显示，并自动替换当前输入的 token

无需手动敲完整个字符串，也不需要记住完整文件名。

---

## 多个候选列表共存

你可以为同一个命令的不同 `STRING` 选项分别定义候选，也可以为不同命令的同名选项分别定义。框架通过 `(cmd_name, long_option)` 二元组唯一匹配，彼此之间互不干扰。

```c
/* log 命令的 --file 候选 */
CLI_CANDIDATE(log_file, "log", "file",
              CANDIDATES("app.log", "app.cfg", "debug.log"));

/* ts 命令的 --msg 候选 */
CLI_CANDIDATE(ts_msg, "ts", "msg",
              CANDIDATES("hello", "world"));
```

运行时 `cli_candidate_init`（优先级 12）会自动遍历所有 `CLI_CANDIDATE` 注册项，将候选数据填充到匹配的命令选项中。

---

## 与运行时动态候选的区别

`CLI_CANDIDATE` 注册的是**编译期固定**的候选列表，适合文件名、配置项等已知常量。

如果候选列表需要在运行时动态生成（例如根据当前挂载的 SD 卡文件列表），则可以在 `init_d` 初始化函数中直接修改对应 `cli_option_t` 的 `candidate_argc` 和 `candidate_argv` 字段。`var` 命令的 `-r`/`-w` 选项就是通过这种方式在运行时从 `.cli_vars` 段动态填充候选列表的。
