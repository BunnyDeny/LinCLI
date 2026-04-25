# 测试用例与终端操作示例


### 1. `tb` — BOOL 类型选项测试

**命令描述**：测试 BOOL 开关选项。  
**选项**：`-v, --verbose`（启用详细输出）

```bash
lin@linCli> tb -v
BOOL test executed!
  verbose = true
lin@linCli>
```

### 2. `ts` — STRING 类型选项测试

**命令描述**：测试字符串选项。  
**选项**：`-m, --msg <text>`（消息文本）

```bash
lin@linCli> ts -m hello
 hello

lin@linCli>
```

### 3. `ti` — INT 类型选项测试

**命令描述**：测试单个整数选项。  
**选项**：`-n, --num <value>`（整数值）

```bash
lin@linCli> ti -n 42
INT test executed!
  num = 42
lin@linCli>
```

### 4. `td` — DOUBLE 类型选项测试

**命令描述**：测试浮点数选项。  
**选项**：`-f, --factor <value>`（浮点值）

```bash
lin@linCli> td -f 3.14
DOUBLE test executed!
  factor = 3.140000
lin@linCli>
```

### 5. `ta` — INT_ARRAY 类型选项测试（带依赖）

**命令描述**：测试整数数组选项，并演示 `depends` 依赖关系。  
**选项**：
- `-v, --verbose`         （BOOL）
- `-n, --nums <list>`     （INT_ARRAY，最多 8 个，**依赖** `verbose`）

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
[ERR] 选项 -n/--nums 依赖 verbose 但未提供
[ERR] 命令解析失败: ta
...
lin@linCli>
```

### 6. `tc` — CALLBACK 类型选项测试

**命令描述**：测试自定义回调选项，框架仅将原始字符串传给 handler。  
**选项**：`-c, --cfg <raw>`（原始配置字符串）

```bash
lin@linCli> tc -c foo
CALLBACK test executed!
  custom callback triggered with: foo
lin@linCli>
```

### 7. `tr` — REQUIRED 必需选项测试

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
[ERR] 缺少必需选项: -f/--file
[ERR] 命令解析失败: tr
...
lin@linCli>
```

### 8. `tcf` — CONFLICTS 互斥选项测试

**命令描述**：测试选项互斥（`conflicts` 字段）。  
**选项**：
- `-v, --verbose`         （BOOL）
- `-n, --nums <list>`     （INT_ARRAY，与 `verbose` **互斥**）

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
[ERR] 选项 -n/--nums 与 verbose 互斥，不能同时使用
[ERR] 命令解析失败: tcf
...
lin@linCli>
```

### 9. `tw` — CLI_COMMAND_WITH_BUF 独立缓冲区测试

**命令描述**：演示为命令指定独立的静态参数解析缓冲区（而非共享 `g_cli_cmd_buf`）。  
**选项**：
- `-v, --verbose`
- `-n, --nums <list>`（INT_ARRAY，最多 16 个）

```bash
lin@linCli> tw -v -n 10 20 30
WITH_BUF test executed!
  verbose = true
  nums =  10  20  30
lin@linCli>
```

---

## 高级交互功能
