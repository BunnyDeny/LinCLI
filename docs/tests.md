# 🎯 测试用例与终端操作示例

> 📝 **如何开启测试命令**
>
> `tests/` 目录下的所有测试命令默认**不参与编译**。使用前请在 `include/cli_config.h` 中开启：
> ```c
> #define CLI_ENABLE_TESTS 1
> ```
> 此外，部分测试命令依赖对应的子功能模块，请确保相关宏也处于开启状态：
> | 测试命令 | 依赖模块宏 |
> |----------|-----------|
> | `var`（变量系统） | `CLI_ENABLE_VAR 1` |
> | `env`（环境变量） | `CLI_ENABLE_ENV 1` |
> | 其余测试命令 | 仅需 `CLI_ENABLE_TESTS 1` |


### 💡 1. `tb` — BOOL 类型选项测试

**命令描述**：测试 BOOL 开关选项。  
**选项**：`-v, --verbose`（启用详细输出）

```bash
lin@linCli> tb -v
BOOL test executed!
  verbose = true
lin@linCli>
```

### ⚙️ 2. `ts` — STRING 类型选项测试

**命令描述**：测试字符串选项。  
**选项**：`-m, --msg <text>`（消息文本）

```bash
lin@linCli> ts -m hello
 hello

lin@linCli>
```

### 🛡️ 3. `ti` — INT 类型选项测试

**命令描述**：测试单个整数选项。  
**选项**：`-n, --num <value>`（整数值）

```bash
lin@linCli> ti -n 42
INT test executed!
  num = 42
lin@linCli>
```

### 🔹 4. `td` — DOUBLE 类型选项测试

**命令描述**：测试浮点数选项。  
**选项**：`-f, --factor <value>`（浮点值）

```bash
lin@linCli> td -f 3.14
DOUBLE test executed!
  factor = 3.140000
lin@linCli>
```

### 📌 5. `ta` — INT_ARRAY 类型选项测试（带依赖）

**命令描述**：测试整数数组选项，并演示 `depends` 依赖关系。  
**选项**：
- ✅ `-v, --verbose`         （BOOL）
- 🔹 `-n, --nums <list>`     （INT_ARRAY，最多 8 个，**依赖** `verbose`）

```bash
lin@linCli> ta -v -n 1 2 3
INT_ARRAY test executed!
  verbose = true
  nums =  1  2  3
lin@linCli>
```

如果缺少依赖的 `-v`，会报错：

```bash
lin@linCli> ta -n 1 2 3
[ERR] command parsing failed: ta
usage: ta [-v] [-n <nums...>]
[ERR] try 'ta -h' or 'ta --help' for more information.
lin@linCli>
```

### 💡 6. `tc` — CALLBACK 类型选项测试

**命令描述**：测试自定义回调选项，框架仅将原始字符串传给 handler。  
**选项**：`-c, --cfg <raw>`（原始配置字符串）

```bash
lin@linCli> tc -c foo
CALLBACK test executed!
  custom callback triggered with: foo
lin@linCli>
```

### ⚙️ 7. `tr` — REQUIRED 必需选项测试

**命令描述**：测试选项的 `required=true` 校验。  
**选项**：`-f, --file <path>`（STRING，**必需**）

正常输入：

```bash
lin@linCli> tr -f /tmp/data.txt
REQUIRED test executed!
  file = /tmp/data.txt
lin@linCli>
```

缺少必需选项：

```bash
lin@linCli> tr
[ERR] command parsing failed: tr
usage: tr -f <file>
[ERR] try 'tr -h' or 'tr --help' for more information.
lin@linCli>
```

### 🛡️ 8. `tcf` — CONFLICTS 互斥选项测试

**命令描述**：测试选项互斥（`conflicts` 字段）。  
**选项**：
- 📌 `-v, --verbose`         （BOOL）
- 🆕 `-n, --nums <list>`     （INT_ARRAY，与 `verbose` **互斥**）

正常单独使用：

```bash
lin@linCli> tcf -n 1 2 3
CONFLICTS test executed!
  nums =  1  2  3
lin@linCli>
```

同时使用互斥选项会报错：

```bash
lin@linCli> tcf -v -n 1 2 3
[ERR] command parsing failed: tcf
usage: tcf [-v] [-n <nums...>]
[ERR] try 'tcf -h' or 'tcf --help' for more information.
lin@linCli>
```

### 🔹 9. `tw` — CLI_COMMAND_WITH_BUF 独立缓冲区测试

**命令描述**：演示为命令指定独立的静态参数解析缓冲区（而非共享 `g_cli_cmd_buf`）。  
**选项**：
- 💎 `-v, --verbose`
- ✅ `-n, --nums <list>`（INT_ARRAY，最多 16 个）

```bash
lin@linCli> tw -v -n 10 20 30
WITH_BUF test executed!
  verbose = true
  nums =  10  20  30
lin@linCli>
```

### 📌 10. `var` — 变量系统测试（内建命令）

**命令描述**：框架内建的变量读写命令，支持查看和修改所有通过 `CLI_VAR` / `CLI_VAR_CUSTOM` 注册的变量。测试文件位于 `tests/commands/test_cli_var.c`。

**测试覆盖内容**：
- 🔹 内建类型变量：`INT`、`DOUBLE`、`BOOL`、`STRING`
- 📌 自定义类型变量：`point`（二维坐标）、`pid`（PID 参数结构体）
- 🆕 只读保护：`CLI_VAR_RO` 和 `CLI_VAR_CUSTOM_RO`

**列出所有变量：**

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

lin@linCli>
```

**读取变量：**

```bash
lin@linCli> var -r g_origin
g_origin (point) = (10,20)

lin@linCli> var -r g_kp
g_kp (DOUBLE) = 2.500000
```

**写入变量：**

```bash
lin@linCli> var -w g_origin --val 50,60
g_origin = (50,60)

lin@linCli> var -w g_pid --val 2500,200,80
g_pid = kp=2500,ki=200,kd=80
```

**只读变量拒绝写入：**

```bash
lin@linCli> var -w g_target --val 1,2
[ERR] 'g_target' is read-only
```

**Tab 补全：**

```bash
lin@linCli> var -r <Tab>
g_device_name    g_kp    g_ki    g_loop_count    g_origin    g_pid    g_target    g_verbose

lin@linCli> var -w <Tab>
g_device_name    g_kp    g_loop_count    g_origin    g_pid    g_verbose
```

> 📝 `-w` 的补全列表自动排除了 `g_ki` 和 `g_target` 两个只读变量。

---

## 🚀 高级交互功能
