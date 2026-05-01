---
name: lincli-dev
description: LinCLI 嵌入式 CLI 框架项目开发规范。用于指导在该项目中编写 C 代码、提交更改、管理版本号等开发活动。当 Kimi 在该项目（/home/bunnydeny/programming/LinCLI）中执行代码修改、提交或任何开发任务时触发使用。
---

# LinCLI 项目开发规范

## 1. 版本号管理

版本号定义在 `include/cli_config.h`：

```c
#define CLI_VERSION_MAJOR 1
#define CLI_VERSION_MINOR 3
#define CLI_VERSION_PATCH N
```

### dev 分支（日常开发）

**任何提交前必须自增 `PATCH` 字段**（`N += 1`）。dev 是活跃分支，每次提交代表一个补丁，不得遗漏。

### 合并到 main 分支

使用 `--no-ff` 进行非快进合并：

```bash
git merge --no-ff dev
```

合并时同时执行版本号升级：

1. `MINOR += 1`
2. `PATCH = 0`

这代表一个正式小版本的发布。

## 2. 提交日志语言

所有 Git 提交日志必须使用中文。禁止使用英文提交信息。

## 3. 内存池使用规范

栈上临时缓冲区若大于 32 字节，优先改用项目内存池：

```c
char *buf = cli_mpool_alloc();
if (!buf) { /* 处理 OOM */ }
/* 使用 buf */
cli_mpool_free(buf);
```

- 每次 `cli_mpool_alloc()` 必须在同一代码路径内配对 `cli_mpool_free()`
- 分配失败时打印 `<oom>` 并安全返回，禁止继续使用 NULL 指针

## 4. 函数行数限制

任何函数不得超过 **25 行**（物理行数，含空行与注释）。超过时必须拆分为更小的函数。

拆分原则：
- 每个子函数职责单一
- 子函数命名清晰表达其操作
- 不得为了凑行数而故意合并逻辑

## 5. 代码风格

遵循项目根目录 `.clang-format` 配置。不得引入与其冲突的格式。

## 6. 关键宏语义

### CLI_COMMAND 注册命令

```c
CLI_COMMAND(name, cmd_str, brief_str, _usage_arr, parse_cb, arg_struct_ptr, ...)
```

- `name`：C 标识符，仅用于生成内部静态符号
- `cmd_str`：终端实际输入的命令名
- `arg_struct_ptr`：必须传 `(struct xxx *)0`，严禁传 NULL

### OPTION 定义选项

统一 10 参数：

```c
OPTION(short_opt, long_opt, TYPE, help, stype, field, max_args, depends, conflicts, required)
```

- `depends`：空格分隔的长选项名列表，必须全部出现本选项才合法
- `conflicts`：空格分隔的长选项名列表，任一出现则本选项非法
- 互斥是单向声明的，只需在其中一个选项的 `conflicts` 中声明即可

### 链接脚本段自动收集

- `.cli_commands`、`.cli_vars`、`.scheduler` 等段通过 `__attribute__((section(...)))` 自动收集
- 开发者只需写宏，无需在 `main()` 中手动注册
