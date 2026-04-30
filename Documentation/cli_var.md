# 变量系统

LinCLI 内建一套变量导出系统，允许你把代码中的全局变量注册为 CLI 可读写对象。用户无需重新编译，即可在运行时查看和修改变量值。这对于现场调试、参数整定、状态监控等场景极其有用。

---

## 注册变量

### 可读写变量

```c
#include "cli_var.h"

static int g_loop_count = 0;
CLI_VAR(g_loop_count, "g_loop_count", INT, "Main loop counter");

static double g_kp = 2.5;
CLI_VAR(g_kp, "g_kp", DOUBLE, "PID Kp parameter");

static bool g_verbose = false;
CLI_VAR(g_verbose, "g_verbose", BOOL, "Verbose output flag");

static char g_device_name[32] = "lincli-dev";
CLI_VAR(g_device_name, "g_device_name", STRING, "Device name");
```

### 只读变量

```c
static double g_ki = 0.1;
CLI_VAR_RO(g_ki, "g_ki", DOUBLE, "PID Ki parameter (read-only)");
```

**宏参数说明：**

| 参数 | 含义 |
|------|------|
| `g_loop_count` | C 变量符号名（直接使用变量本身） |
| `"g_loop_count"` | 注册到 CLI 的变量名 |
| `INT` | 变量类型，可选 `INT` / `DOUBLE` / `BOOL` / `STRING` |
| `"Main loop counter"` | 变量描述文档，显示在 `var -l` 列表中 |

> **类型匹配要求**：`CLI_VAR` 的 `TYPE` 必须与 C 变量的实际类型严格对应。`STRING` 类型要求变量为 `char[]` 数组（如 `char buf[32]`），不能使用 `char *` 指针——因为框架内部通过 `sizeof(symbol)` 获取变量大小来做边界检查，数组能返回正确的长度，而指针只能返回指针本身的大小（如 8 字节）。

---

## 终端操作

### 读取变量

```bash
lin@linCli> var -r g_kp
g_kp (DOUBLE) = 2.500000
```

### 写入变量

```bash
lin@linCli> var -w g_kp --val 3.5
g_kp = 3.500000

lin@linCli> var -w g_verbose --val true
g_verbose = true

lin@linCli> var -w g_device_name --val motor-ctrl
g_device_name = "motor-ctrl"
```

### 列出所有变量

```bash
lin@linCli> var -l

NAME                 TYPE       VALUE                    ATTR DESCRIPTION
g_loop_count         INT        0                             Main loop counter
g_kp                 DOUBLE     2.5000                        PID Kp parameter
g_ki                 DOUBLE     0.1000                   RO    PID Ki parameter (read-only)
g_verbose            BOOL       false                         Verbose output flag
g_device_name        STRING     "lincli-dev"                  Device name
```

- `ATTR` 列中 `RO` 表示只读变量，不可写入
- 所有变量的当前值会实时显示

---

## Tab 补全支持

`var` 命令的 `-r` 和 `-w` 选项支持 Tab 补全：

- `-r` 的候选列表包含**所有**已注册变量（含只读）
- `-w` 的候选列表仅包含**可读写**变量（排除 `CLI_VAR_RO`）

候选列表在运行时从 `.cli_vars` 段自动遍历生成，新增变量无需额外注册补全数据。

```bash
lin@linCli> var -r <Tab>
g_device_name    g_kp    g_ki    g_loop_count    g_verbose

lin@linCli> var -w <Tab>
g_device_name    g_kp    g_loop_count    g_verbose
```

> 注意：`g_ki` 是 `CLI_VAR_RO` 注册的只读变量，因此不会出现在 `-w` 的补全列表中。

---

## 写入值的类型规则

| 目标类型 | 合法输入示例 | 说明 |
|----------|-------------|------|
| `INT` | `42`, `-10`, `0xFF` | 支持十进制和十六进制前缀 `0x` |
| `DOUBLE` | `3.14`, `-0.5`, `2e3` | 标准浮点格式 |
| `BOOL` | `true`, `false`, `1`, `0` | 大小写敏感，仅接受这四种形式 |
| `STRING` | `hello`, `motor-ctrl` | 任意字符串，超过数组长度会自动截断并打印警告 |

---

## 使用场景

- **现场 PID 调参**：电机控制场景下，在线修改 `g_kp`、`g_ki` 观察响应，无需重新编译烧录
- **开关功能调试**：通过 `g_verbose` 动态开启/关闭详细日志，排查完成后关闭，避免日志风暴
- **状态监控**：周期性查看 `g_loop_count` 确认主循环是否正常运行
- **设备标识修改**：通过 `g_device_name` 修改设备名，写入后持久化到 Flash（需用户自行实现持久化逻辑）

---

## 注意事项

1. **变量生命周期**：`CLI_VAR` 注册的变量地址在运行时必须始终有效。如果变量定义在函数局部作用域中，函数返回后地址失效，会导致未定义行为。

2. **STRING 类型边界**：写入字符串时，如果长度超过 `sizeof(buf) - 1`，框架会自动截断并打印警告。这是为了防止缓冲区溢出。

3. **只读保护**：`CLI_VAR_RO` 注册的变量在 `var -l` 中标记为 `RO`，尝试写入时会报错 `"xxx is read-only"`。

4. **无需手动注册 `var` 命令**：`var` 命令是框架内建命令，只要项目中包含了 `cli_var.c`（或链接了 `cli` 库），即可直接使用。
