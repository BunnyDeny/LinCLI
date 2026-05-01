# 变量系统

LinCLI 内建一套变量导出系统，允许你把代码中的全局变量注册为 CLI 可读写对象。用户无需重新编译，即可在运行时查看和修改变量值。这对于现场调试、参数整定、状态监控等场景极其有用。

除了 `INT` / `DOUBLE` / `BOOL` / `STRING` 四种内建类型外，框架还支持**用户自定义类型**——你可以为任意结构体注册序列化/反序列化规则，让 `var` 命令直接读写结构体成员。

---

## 注册变量

### 内建类型：可读写变量

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

### 内建类型：只读变量

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

## 自定义类型

当内建的 `INT` / `DOUBLE` / `BOOL` / `STRING` 无法满足需求时，你可以为**任意结构体**注册自定义类型。注册后，该类型可以像内建类型一样通过 `var -r` / `var -w` / `var -l` 操作。

### 为什么要用自定义类型？

嵌入式开发中，很多配置天然是结构体：

- 电机控制的 PID 参数（`kp`, `ki`, `kd` 三个字段）
- 屏幕坐标点（`x`, `y`）
- 网络配置（IP、端口、网关）
- 传感器校准值（多轴偏移、增益矩阵）

如果把这些字段拆成 3~6 个独立的 `INT` / `DOUBLE` 变量，虽然可行，但：
- 语义不连贯，调参时需要记多个独立的名字
- 无法保证参数组的原子一致性（改了一半 kp，ki 还是旧的）
- `var -l` 列表臃肿，可读性差

自定义类型让你可以把整个结构体当作**一个逻辑变量**来读写，终端输入一个字符串即可更新整个结构体。

### 注册自定义类型的三步

#### 第 1 步：定义结构体

```c
typedef struct {
    int x;
    int y;
} point_t;
```

#### 第 2 步：实现序列化/反序列化回调

```c
#include "cli_io.h"

/* from_string: 字符串 → 结构体内存（对应 var -w 写入） */
static int point_from_str(void *addr, size_t size, const char *str)
{
    point_t *p = addr;
    if (sscanf(str, "%d,%d", &p->x, &p->y) != 2) {
        pr_err("point format must be x,y (e.g. 10,20)\r\n");
        return -1;
    }
    return 0;
}

/* to_string: 结构体内存 → 字符串（对应 var -r 读出） */
static int point_to_str(const void *addr, size_t size, char *buf,
                        size_t buf_size)
{
    const point_t *p = addr;
    snprintf(buf, buf_size, "(%d,%d)", p->x, p->y);
    return 0;
}
```

**回调签名详解：**

| 回调 | 参数 | 说明 |
|------|------|------|
| `from_string` | `void *addr` | 目标结构体的内存地址，你需要把解析结果写入这里 |
| | `size_t size` | 结构体的实际大小（`sizeof(symbol)`），用于边界检查 |
| | `const char *str` | 用户在终端输入的原始字符串，如 `"10,20"` |
| `to_string` | `const void *addr` | 结构体内存地址（只读），你需要读取它并生成字符串 |
| | `size_t size` | 结构体大小 |
| | `char *buf` | 框架提供的输出缓冲区，你往里写入格式化后的字符串 |
| | `size_t buf_size` | 缓冲区的总大小，必须确保不溢出 |

**返回值**：成功返回 `0`，失败返回 `-1`。失败时框架会终止当前 `var -w` 操作，并把你通过 `pr_err` 打印的错误信息展示给用户。

#### 第 3 步：注册类型和变量

```c
/* 注册类型（全局只需一次，多个变量可共享同一套回调） */
CLI_VAR_TYPE(point, point_from_str, point_to_str);

/* 注册自定义类型变量 */
static point_t g_origin = {10, 20};
CLI_VAR_CUSTOM(g_origin, "g_origin", "point", "Screen origin point");

/* 只读自定义类型变量 */
static point_t g_target = {100, 200};
CLI_VAR_CUSTOM_RO(g_target, "g_target", "point", "Target point (read-only)");
```

**宏参数说明：**

| 宏 | 参数 | 含义 |
|----|------|------|
| `CLI_VAR_TYPE` | `point` | 类型标识符，宏会用它生成内部静态符号 |
| | `point_from_str` | 反序列化回调（字符串 → 结构体） |
| | `point_to_str` | 序列化回调（结构体 → 字符串） |
| `CLI_VAR_CUSTOM` | `g_origin` | C 变量符号名 |
| | `"g_origin"` | 注册到 CLI 的变量名 |
| | `"point"` | 自定义类型名（必须与 `CLI_VAR_TYPE` 注册时的标识符一致） |
| | `"Screen origin point"` | 变量描述文档 |

> **命名约定**：`CLI_VAR_TYPE` 的第一个参数和 `CLI_VAR_CUSTOM` 的第三个参数（字符串）**必须一致**。框架在运行时通过字符串比较来查找对应的类型操作回调。

### 自定义类型的只读支持

如果你希望某个自定义类型变量只能查看、不能修改，有两种方式：

1. **变量级只读**：使用 `CLI_VAR_CUSTOM_RO` 宏注册。框架会在 `var -w` 时拦截并提示 `"xxx is read-only"`。
2. **类型级只写禁用**：把 `CLI_VAR_TYPE` 的 `from_str` 参数设为 `NULL`。这样所有使用该类型的变量都无法写入，尝试 `var -w` 时会提示 `"type 'xxx' does not support write"`。

两种方式的区别：
- `CLI_VAR_CUSTOM_RO` 是**变量粒度**的控制，同类型的其他变量仍可写。
- `from_str = NULL` 是**类型粒度**的控制，影响所有该类型的变量。

### 自定义回调的鲁棒性：警惕 `sscanf` 的贪婪解析

很多开发者写自定义 `from_string` 回调时会自然地用 `sscanf`：

```c
/* 不安全的写法 */
static int point_from_str(void *addr, size_t size, const char *str)
{
    point_t *p = addr;
    if (sscanf(str, "%d,%d", &p->x, &p->y) != 2)
        return -1;
    return 0;
}
```

这有一个隐蔽的陷阱：`sscanf` 的 `%d` 是**贪婪匹配**的，它只会解析到第一个非数字字符就停止，**不会检查后面是否还有垃圾字符**。如果你输入 `10,20abc`，`sscanf` 会成功返回 `2`，把 `x=10, y=20` 写入内存，然后框架显示 `point = (10,20)`——用户完全不知道自己的输入其实不合法。

**正确的做法**：在 `sscanf` 格式字符串末尾加上 `%n`，记录实际消耗的字符数，然后检查输入字符串是否被完全消耗：

```c
static int point_from_str(void *addr, size_t size, const char *str)
{
    point_t *p = addr;
    int n = 0;
    if (sscanf(str, "%d,%d%n", &p->x, &p->y, &n) != 2) {
        pr_err("point format must be x,y (e.g. 10,20)\r\n");
        return -1;
    }
    /* 关键：检查尾部是否还有多余字符 */
    if (str[n] != '\0') {
        pr_err("point format must be x,y (e.g. 10,20)\r\n");
        return -1;
    }
    return 0;
}
```

同样适用于多字段结构体：

```c
static int pid_from_str(void *addr, size_t size, const char *str)
{
    pid_params_t *pid = addr;
    int n = 0;
    if (sscanf(str, "%d,%d,%d%n", &pid->kp, &pid->ki, &pid->kd, &n) != 3) {
        pr_err("pid format must be kp,ki,kd (e.g. 2000,100,50)\r\n");
        return -1;
    }
    if (str[n] != '\0') {
        pr_err("pid format must be kp,ki,kd (e.g. 2000,100,50)\r\n");
        return -1;
    }
    return 0;
}
```

> **为什么框架内建类型不会有这个问题？**
>
> 框架的 `INT` / `DOUBLE` 内建类型底层调用的是 `cli_parse_int()` / `cli_parse_double()`，这两个函数内部使用 `strtol()` / `strtod()` 并严格检查 `endptr`，确保输入字符串被**完全消耗**后才会返回成功。因此 `var -w g_loop_count --val 300a0` 会直接报错 `"300a0" is not a valid integer`。
>
> 自定义类型由用户自己实现解析逻辑，框架只负责把原始字符串透传进来，因此**鲁棒性需要用户自行保证**。

---

## 终端操作

### 读取变量

```bash
lin@linCli> var -r g_kp
g_kp (DOUBLE) = 2.500000

lin@linCli> var -r g_origin
g_origin (point) = (10,20)
```

### 写入变量

```bash
lin@linCli> var -w g_kp --val 3.5
g_kp = 3.500000

lin@linCli> var -w g_verbose --val true
g_verbose = true

lin@linCli> var -w g_device_name --val motor-ctrl
g_device_name = "motor-ctrl"

lin@linCli> var -w g_origin --val 50,60
g_origin = (50,60)
```

> **自定义类型写入**：输入的字符串格式完全由你的 `from_string` 回调决定。上述 `point` 类型要求 `"x,y"` 格式，而你可以自由定义 `"key=val,key2=val2"`、JSON 子集、甚至 Base64 编码——框架只负责把原始字符串原样传给你的回调。

### 列出所有变量

```bash
lin@linCli> var -l

NAME                 TYPE       VALUE                    ATTR DESCRIPTION
--------------------------------------------------------------------------
g_loop_count         INT        0                             Main loop counter
g_kp                 DOUBLE     2.5000                        PID Kp parameter
g_ki                 DOUBLE     0.1000                   RO   PID Ki parameter (read-only)
g_verbose            BOOL       false                         Verbose output flag
g_device_name        STRING     "lincli-dev"                  Device name
g_origin             point      (10,20)                       Screen origin point
g_target             point      (100,200)                RO   Target point (read-only)
g_pid                pid        kp=2000,ki=100,kd=50          Motor PID parameters
```

- `ATTR` 列中 `RO` 表示只读变量，不可写入
- 自定义类型的 `TYPE` 列直接显示类型名（如 `point`、`pid`）
- 所有变量的当前值会实时显示

---

## Tab 补全支持

`var` 命令的 `-r` 和 `-w` 选项支持 Tab 补全：

- `-r` 的候选列表包含**所有**已注册变量（含只读，含自定义类型）
- `-w` 的候选列表仅包含**可读写**变量（排除 `CLI_VAR_RO` 和 `CLI_VAR_CUSTOM_RO`）

候选列表在运行时从 `.cli_vars` 段自动遍历生成，新增变量无需额外注册补全数据。

```bash
lin@linCli> var -r <Tab>
g_device_name    g_kp    g_ki    g_loop_count    g_origin    g_pid    g_target    g_verbose

lin@linCli> var -w <Tab>
g_device_name    g_kp    g_loop_count    g_origin    g_pid    g_verbose
```

> 注意：`g_ki`、`g_target` 是只读变量，因此不会出现在 `-w` 的补全列表中。

---

## 写入值的类型规则

| 目标类型 | 合法输入示例 | 说明 |
|----------|-------------|------|
| `INT` | `42`, `-10`, `0xFF` | 支持十进制和十六进制前缀 `0x` |
| `DOUBLE` | `3.14`, `-0.5`, `2e3` | 标准浮点格式 |
| `BOOL` | `true`, `false`, `1`, `0` | 大小写敏感，仅接受这四种形式 |
| `STRING` | `hello`, `motor-ctrl` | 任意字符串，超过数组长度会自动截断并打印警告 |
| **自定义类型** | **由你的 `from_string` 决定** | 框架把原始字符串透传给回调，格式完全自定义 |

---

## 使用场景

### 内建类型适用场景

- **现场 PID 调参**：电机控制场景下，在线修改 `g_kp`、`g_ki` 观察响应，无需重新编译烧录
- **开关功能调试**：通过 `g_verbose` 动态开启/关闭详细日志，排查完成后关闭，避免日志风暴
- **状态监控**：周期性查看 `g_loop_count` 确认主循环是否正常运行
- **设备标识修改**：通过 `g_device_name` 修改设备名，写入后持久化到 Flash（需用户自行实现持久化逻辑）

### 自定义类型适用场景

- **结构体参数整定**：把整个 PID 结构体（`kp/ki/kd`）作为一个变量 `g_pid` 来读写，输入 `var -w g_pid --val 2500,200,80` 即可一次性更新全部参数，避免逐个字段修改时的不一致状态
- **多维坐标/向量**：把 `point_t`、`velocity_t` 等几何/物理量封装成自定义类型，终端输入 `10,20` 即可更新坐标
- **配置结构体快照**：把网络配置、传感器校准参数等打包成一个结构体，通过自定义类型实现"一键查看/一键修改"
- **寄存器组映射**：把外设的一组相关寄存器映射到一个结构体，通过 `to_string` 以 `"REG_A=0x01,REG_B=0xFF"` 的格式展示，方便硬件调试

---

## 注意事项

1. **变量生命周期**：`CLI_VAR` / `CLI_VAR_CUSTOM` 注册的变量地址在运行时必须始终有效。如果变量定义在函数局部作用域中，函数返回后地址失效，会导致未定义行为。

2. **STRING 类型边界**：写入字符串时，如果长度超过 `sizeof(buf) - 1`，框架会自动截断并打印警告。这是为了防止缓冲区溢出。

3. **只读保护**：`CLI_VAR_RO` / `CLI_VAR_CUSTOM_RO` 注册的变量在 `var -l` 中标记为 `RO`，尝试写入时会报错 `"xxx is read-only"`。

4. **无需手动注册 `var` 命令**：`var` 命令是框架内建命令，只要项目中包含了 `cli_var.c`（或链接了 `cli` 库），即可直接使用。

5. **自定义类型的 `size` 参数**：框架在调用 `from_string` 时会传入 `sizeof(symbol)`。如果你的结构体包含变长数组或柔性数组，建议在该回调中显式检查 `size`，防止用户注册的变量大小与回调预期不符导致越界。

6. **自定义类型的 `to_string` 缓冲区**：`to_string` 的 `buf` 缓冲区在 `var -l` 中只有 32 字节，在 `var -r` 中有 64 字节。如果你的格式化字符串较长，建议精简输出，或在 `to_string` 中截断处理（`snprintf` 天然安全）。

7. **类型名一致性**：`CLI_VAR_TYPE(type_name, ...)` 中的 `type_name` 标识符，与 `CLI_VAR_CUSTOM(var, "var_name", "type_name", ...)` 中的类型名字符串必须严格一致（包括大小写）。框架运行时通过字符串比较来匹配类型操作回调。
