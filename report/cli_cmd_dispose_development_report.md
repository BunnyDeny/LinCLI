# CLI 框架命令解析模块开发报告

> **开发范围**：`cli/Inc/cmd_dispose.h` + `cli/Src/cmd_dispose.c` + `default.ld`  
> **目标**：实现一套基于链接脚本段收集、宏注册的开发者友好型命令行解析框架。

---

## 1. 项目背景

本项目致力于实现一个 **C 语言版本的嵌入式风格 CLI（Command Line Interface）框架**。核心设计理念是：

- **开发者只需用宏定义命令和选项**，无需手动维护注册表；
- **框架自动完成字符串解析、类型转换、选项校验、依赖检查、帮助生成**；
- **底层采用状态机 + 多线程 + 链接脚本段收集** 的架构，保证可移植性和运行时零开销注册。

本次开发聚焦于命令解析的**最后一环**：将 `cmd_dispose.h` 中的设计蓝图落地为可编译、可运行、具备完整解析能力的 `cmd_dispose.c` 实现。

---

## 2. 本次开发内容概览

| 文件 | 动作 | 主要内容 |
|------|------|----------|
| `cli/Inc/cmd_dispose.h` | **重构** | 修复宏参数名冲突、废弃不可靠的变参重载、改为显式命名宏 |
| `default.ld` | **修改** | 新增 `.cli_commands` 段，与 `.scheduler` / `.my_init_d` 风格统一 |
| `cli/Src/cmd_dispose.c` | **重写** | 实现字符串切分、选项查找、核心解析器 `cli_auto_parse`、帮助打印、状态机入口 |

---

## 3. 关键修复：`cmd_dispose.h` 宏设计缺陷

### 3.1 缺陷一：宏参数名与结构体成员名冲突（致命编译错误）

**原代码写法**：

```c
#define OPTION(short_opt, ...) { .short_opt = short_opt, ... }
```

当调用 `OPTION('v', "verbose", ...)` 时，C 预处理器会把**指定初始化器**中的 `short_opt` 也当成宏参数替换，展开后变成：

```c
{ .'v' = 'v', ... }  // 编译失败
```

**修复方案**：所有宏参数名前统一加下划线前缀，确保不与 `cli_option_t` / `cli_command_t` 的任何成员名重合：

```c
#define OPTION(_sopt, _lopt, _type, _help, _stype, _field) \
    { .short_opt = _sopt, .long_opt = _lopt, ... }
```

### 3.2 缺陷二：`GET_MACRO` 变参重载存在根本缺陷

**原代码企图**：通过参数数量重载实现 `OPTION_5`、`OPTION_6`、`OPTION_7`、`OPTION_8`。

**问题**：`OPTION_6`（带 `required`）和 `OPTION_7`（带 `max_args`）都接受 **7 个实参**，C 预处理器**无法通过参数数量区分**二者，导致重载映射错乱，编译行为不可预期。

**修复方案**：废弃 `GET_MACRO` 变参重载，改为语义清晰的**显式命名宏**：

| 宏名称 | 用途 |
|--------|------|
| `OPTION(...)` | 基础版，无额外属性 |
| `OPTION_REQ(..., req)` | 带 `required` 开关 |
| `OPTION_ARRAY(..., max)` | 数组类型，带最大参数个数 |
| `OPTION_ARRAY_DEP(..., max, dep)` | 数组类型 + 依赖选项 |

这样彻底消除了预处理器歧义，同时让开发者调用时意图更明确。

---

## 4. 架构改造：链接脚本段收集机制

### 4.1 原有方式的问题

原设计使用 GCC 的 `__attribute__((constructor))` 将命令注册到全局数组 `g_cli_commands[]`：

```c
__attribute__((constructor)) static void _cli_register_xxx(void) {
    g_cli_commands[g_cli_command_count++] = &_cli_cmd_def_xxx;
}
```

这与项目整体架构风格（`init_d.h`、`stateM.h` 均采用**链接脚本段收集**）不一致，且依赖运行时构造函数顺序，存在潜在不确定性。

### 4.2 改造为链接脚本段收集

**步骤 1：在 `default.ld` 中新增段定义**

```ld
.cli_commands : {
    _cli_commands_start = .;
    *(.cli_commands)
    _cli_commands_end = .;
}
```

该段与 `.scheduler`、`.my_init_d` 等其他段放在同一块数据区域，风格完全统一。

**步骤 2：在头文件中定义导出宏**

```c
#define _EXPORT_CLI_COMMAND_SYMBOL(_obj, _cmd_str, _doc_str, _size, _opts, _opts_cnt, _vld, _section) \
    static cli_command_t _cli_cmd_def_##_obj __attribute__((            \
        used, section(_section), aligned(sizeof(long)))) = {        \
        .name = _cmd_str,                                           \
        .doc = _doc_str,                                            \
        .arg_struct = NULL,                                         \
        .arg_struct_size = _size,                                   \
        .options = _opts,                                           \
        .option_count = _opts_cnt,                                  \
        .validator = _vld,                                          \
    }
```

**步骤 3：提供遍历与查找 API**

```c
extern cli_command_t _cli_commands_start;
extern cli_command_t _cli_commands_end;

#define _FOR_EACH_CLI_COMMAND(_start, _end, _cmd) \
    for ((_cmd) = (_start); (_cmd) < (_end); (_cmd)++)

static inline const cli_command_t *cli_command_find(const char *_name)
{
    cli_command_t *_cmd;
    _FOR_EACH_CLI_COMMAND(&_cli_commands_start, &_cli_commands_end, _cmd)
    {
        if (_cmd->name && strcmp(_cmd->name, _name) == 0)
            return _cmd;
    }
    return NULL;
}
```

这样，开发者每写一个 `CLI_COMMAND(...)`，链接器就会自动将其汇入 `.cli_commands` 段，上电后通过 `_cli_commands_start` / `_cli_commands_end` 遍历即可，**零运行时注册开销，零手动维护注册表**。

---

## 5. 核心实现：`cmd_dispose.c` 命令解析器

### 5.1 字符串切分 `tokenize()`

按空格/制表符将输入命令行切分为 `argc` / `argv` 形式，`argv[0]` 固定为命令名。

```c
static int tokenize(char *line, char **argv, int max_argv);
```

### 5.2 选项查找 `find_option()`

支持短选项 `-v` 和长选项 `--verbose`：

```c
static const cli_option_t *find_option(const cli_command_t *cmd, const char *arg);
```

### 5.3 核心解析 `cli_auto_parse()`

**函数签名**：

```c
int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv, void *arg_struct);
```

**解析流程**：

1. **清零 `arg_struct`**，避免残留数据；
2. 遍历 `argv`：
   - 遇到 `-` 开头则查找对应 `cli_option_t`；
   - 非 `-` 开头则作为当前选项的值，按 `type` 进行类型转换并写入 `arg_struct + offset`；
3. **类型支持**：
   - `CLI_TYPE_BOOL`：无参，直接置 `true`；
   - `CLI_TYPE_STRING`：保存字符串指针；
   - `CLI_TYPE_INT`：`atoi` 转换；
   - `CLI_TYPE_DOUBLE`：`atof` 转换；
   - `CLI_TYPE_INT_ARRAY`：连续读取数值，自动分配数组内存，并更新 `offset_count` 对应的计数器；
4. **完整性校验**：
   - 检查末尾选项是否缺参；
   - 检查 `required` 选项是否缺失；
   - 检查 `depends` 依赖选项是否已提供。

### 5.4 命令级解析入口 `cli_parse()`

```c
int cli_parse(const char *cmd_name, int argc, char **argv, void *arg_struct);
```

先通过 `cli_command_find()` 查表，再调用 `cli_auto_parse()`。

### 5.5 帮助打印 `cli_print_help()`

自动遍历命令的选项数组，输出命令名、描述、各选项的短/长名称、帮助文本、是否必需、依赖关系等信息。

### 5.6 状态机入口 `dispose_start_task()`

这是调度器状态机 `.dispose` 段的起点任务，完成以下闭环：

```
tokenize(cmd) → cli_command_find → cli_auto_parse → validator → return dispose_exit
```

解析失败时，自动打印 `cli_print_help()` 作为用户提示。

---

## 6. 测试验证

在 `cmd_dispose.c` 中注册了一个测试命令 `hello`：

```c
struct hello_args {
    bool verbose;
    char *name;
    int *numbers;
    size_t numbers_count;
};

CLI_COMMAND(hello, "hello", "Print greeting message with optional name and number array", hello_handler, (struct hello_args *)0,
    OPTION('v', "verbose", BOOL, "Enable verbose", struct hello_args, verbose),
    OPTION('n', "name", STRING, "Your name", struct hello_args, name),
    OPTION_ARRAY('a', "array", INT_ARRAY, "Number array", struct hello_args, numbers, 8),
    END_OPTIONS);
```

### 6.1 正常命令测试

**输入**：`hello -v -n Alice`

**输出**：
```
[NOTICE] Hello command executed!
[NOTICE]   verbose = true
[NOTICE]   name    = Alice
```

### 6.2 数组参数测试

**输入**：`hello -a 1 2 3`

**输出**：
```
[NOTICE] Hello command executed!
[NOTICE]   numbers = 1 2 3
```

### 6.3 缺少参数错误测试

**输入**：`hello -n`

**输出**：
```
[ERR] 选项 -n/--name 缺少参数
[ERR] 命令解析失败: hello
[NOTICE] 命令: hello
[NOTICE] 选项:
  -v, --verbose          Enable verbose
  -n, --name             Your name
  -a, --array            Number array
```

### 6.4 未知命令测试

**输入**：`unknown_cmd`

**输出**：
```
[ERR] 未知命令: unknown_cmd
```

---

## 7. 开发者使用指南

开发者定义一个 CLI 命令只需三步：

### 步骤 1：定义参数结构体

```c
struct my_args {
    bool verbose;
    char *output;
    int *numbers;
    size_t numbers_count;   // 用于 OPTION_ARRAY 自动计数
};
```

### 步骤 2：实现处理函数

```c
int my_cmd_handler(void *args)
{
    struct my_args *a = args;
    if (a->verbose)
        pr_info("verbose enabled\n");
    return 0;
}
```

### 步骤 3：用宏注册命令

```c
CLI_COMMAND(my_cmd, "my_cmd", "My custom command description", my_cmd_handler, (struct my_args *)0,
    OPTION('v', "verbose", BOOL, "Enable verbose", struct my_args, verbose),
    OPTION('o', "output", STRING, "Output file", struct my_args, output),
    OPTION_ARRAY('n', "numbers", INT_ARRAY, "Numbers", struct my_args, numbers, 10),
    END_OPTIONS);
```

框架会自动完成：
- 链接段收集注册
- 命令行分词
- 选项类型转换与校验
- 依赖/必需检查
- 调用 `my_cmd_handler`

---

## 8. 后续可扩展方向

| 方向 | 说明 |
|------|------|
| **位置参数** | `POSITIONAL` 宏尚未在解析器中被识别，可扩展 `cli_auto_parse` 支持非选项参数 |
| **互斥选项** | `conflicts` 字段已定义，但校验逻辑暂未实现 |
| **引号与转义** | `tokenize()` 目前按简单空格切分，可支持 `"` 包裹含空格的参数 |
| **更多类型** | 如 `CLI_TYPE_UINT64`、`CLI_TYPE_HEX`、`CLI_TYPE_IP` 等 |
| `--help` 自动响应 | 在 `dispose_start_task` 中检测到 `--help` 时自动调用 `cli_print_help` |
| **内存泄漏防护** | `INT_ARRAY` 使用 `calloc` 分配，可引入命令生命周期结束后的统一释放接口 |

---

## 9. 结论

本次开发完成了 CLI 框架从“设计蓝图”到“可运行代码”的关键一跃：

1. **彻底修复了宏系统的两个致命缺陷**（参数名冲突 + 变参重载歧义），使开发者可以稳定地使用宏注册命令；
2. **统一了链接脚本段收集机制**，与项目已有架构风格完全一致；
3. **实现了完整的命令解析闭环**，覆盖分词、查表、类型转换、校验、执行、错误提示、帮助打印等环节；
4. **通过了多组实际命令输入测试**，验证了框架的可用性。

当前框架已具备基础 CLI 功能，开发者可通过简单的宏声明快速扩展新命令。
