# LinCLI 智能补全（Smart Completion）实现文档

> 版本：v1.0
> 状态：设计完成，待实现
> 目标：将现有"前缀优先 + 子字符串兜底"的补全策略，重构为"统一相关列表 + 视觉分组"的优雅策略

---

## 一、设计目标

### 1.1 现有方案的问题

当前 `ohmyzsh` 式策略（前缀优先，无前缀匹配时才子字符串匹配）存在行为不一致性：

```bash
# 场景 A：系统中只有 zhaolin, zhaolinjing
lin@linCli> in<Tab>
~ zhaolin      ~ zhaolinjing      # 子字符串兜底触发

# 场景 B：系统中多了 inline
lin@linCli> in<Tab>
inline                           # 前缀匹配吞掉了子字符串结果
```

用户输入 `in` 的行为因为系统中是否存在 `inline` 而突变，违反最小惊讶原则。

### 1.2 新方案的核心规则

**规则 1（唯一性原则）**：如果当前输入是某个命令/选项的**严格唯一前缀**，直接补全，追加空格。

**规则 2（完整性原则）**：其他所有情况（多前缀匹配 / 前缀无匹配 / 前缀唯一但存在子字符串匹配），统一展示**所有相关结果**，按匹配类型分组，用颜色区分。

```bash
# 前缀唯一 → 直接补全
lin@linCli> inte<Tab>
lin@linCli> internal            # 只有 internal 前缀匹配，直接补全

# 存在歧义 → 统一展示
lin@linCli> in<Tab>
inline              # 前缀匹配（正常白色）
zhaolin             # 子字符串匹配（灰色）
zhaolinjing         # 子字符串匹配（灰色）
```

---

## 二、总体架构

### 2.1 改动范围

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `include/cli_completion.h` | 修改 | 新增匹配类型枚举、分组列表上下文 |
| `src/cli/cli_completion.c` | 重写核心逻辑 | 命令名补全、选项补全、值补全入口重构 |
| `src/cli/cli_cmd_line.c` | 微调 | 确保 Tab 状态机兼容新列表展示逻辑 |
| `src/cli/cli_edit.c` | 不改动 | 现有 token 替换函数完全复用 |

### 2.2 颜色定义（复用现有宏）

```c
// include/cli_io.h 中已定义，直接使用：
#define COLOR_NONE    "\033[0m"
#define COLOR_WHITE   "\033[37m"   // 前缀匹配：正常白色
#define COLOR_GRAY    "\033[90m"   // 子字符串匹配：灰色（需确认终端支持）
```

> 注：若目标终端不支持 `\033[90m`（明亮黑/灰色），可降级使用 `\033[2m`（暗淡属性）或 `\033[37m` + 空格缩进。

---

## 三、数据结构变更

### 3.1 新增匹配类型枚举

在 `include/cli_completion.h` 中新增：

```c
typedef enum {
    MATCH_TYPE_NONE = 0,
    MATCH_TYPE_PREFIX,      /* 严格前缀匹配 */
    MATCH_TYPE_SUBSTRING,   /* 子字符串匹配（非前缀） */
} match_type_t;
```

### 3.2 候选上下文扩展

`struct candidate_ctx` 增加子字符串匹配相关字段：

```c
struct candidate_ctx {
    cand_active_t active;
    char prefix[CMD_LINE_BUF_SIZE];
    int prefix_len;
    const cli_command_t *cmd;
    cli_option_t *opt;
    int highlight_index;
    cand_cycling_t cycling;
    int rows;
    int cols;
    int repl_start;
    
    /* 新增：子字符串匹配列表缓存 */
    int substr_count;           /* 子字符串匹配数量 */
    int total_count;            /* 总匹配数量（前缀 + 子字符串） */
};
```

> 说明：`substr_count` 和 `total_count` 用于高亮循环时计算边界。由于命令/选项数量有限（嵌入式场景通常 <100），无需动态分配数组，直接遍历即可。

---

## 四、核心函数重构

### 4.1 命令名补全入口 `complete_command_name()`

现有逻辑（三段式）：
```
match_cnt == 1 → complete_unique_cmd()
match_cnt > 1  → complete_multi_cmd()  /* LCP 填充或列表 */
match_cnt == 0 → 响铃
```

新逻辑（统一列表 + 唯一性检测）：

```c
void complete_command_name(const char *prefix, int prefix_len)
{
    const cli_command_t *prefix_match = NULL;
    int prefix_cnt = find_cmd_match(prefix, prefix_len, &prefix_match);
    
    const cli_command_t *substr_first = NULL;
    int substr_cnt = find_cmd_substring_match(prefix, prefix_len, &substr_first);
    
    int total = prefix_cnt + substr_cnt;
    
    if (total == 0) {
        /* 无任何匹配 */
        clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
        candidate_ctx_clear();
        cli_out_push((_u8 *)"\a", 1);
        cli_out_sync();
        cmd_line_redraw();
        return;
    }
    
    /* 唯一前缀匹配：直接补全 */
    if (prefix_cnt == 1 && substr_cnt == 0) {
        complete_unique_cmd(prefix_match);
        return;
    }
    
    /* 其他所有情况：展示统一相关列表 */
    display_unified_cmd_list(prefix, prefix_len, prefix_cnt, substr_cnt);
}
```

**关键决策点**：
- `prefix_cnt == 1 && substr_cnt == 0`：严格唯一前缀，直接补全（最高效路径）
- `prefix_cnt >= 1 && substr_cnt >= 1`：前缀匹配存在，子字符串匹配也存在，展示混合列表
- `prefix_cnt == 0 && substr_cnt >= 1`：无前缀匹配，只有子字符串匹配，展示子字符串列表
- `prefix_cnt > 1`：多前缀歧义，展示前缀列表（也可同时展示子字符串匹配）

### 4.2 子字符串匹配函数 `find_cmd_substring_match()`

新增函数，与 `find_cmd_match()` 对称：

```c
int find_cmd_substring_match(const char *prefix, int prefix_len,
                             const cli_command_t **first_match)
{
    int match_cnt = 0;
    const cli_command_t *cmd;
    
    _FOR_EACH_CLI_COMMAND(cmd)
    {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        /* 排除前缀匹配的项（避免重复） */
        if (strncmp(cmd->name, prefix, prefix_len) == 0)
            continue;
        if (strstr(cmd->name, prefix) != NULL) {
            match_cnt++;
            if (match_cnt == 1)
                *first_match = cmd;
        }
    }
    return match_cnt;
}
```

> 注意：必须排除前缀匹配项，避免 `inline` 同时出现在前缀列表和子字符串列表中。

### 4.3 统一列表展示 `display_unified_cmd_list()`

核心展示函数，按分组输出：

```c
void display_unified_cmd_list(const char *prefix, int prefix_len,
                              int prefix_cnt, int substr_cnt)
{
    int old_rows = candidate_ctx.rows;
    clear_and_up(old_rows, old_rows);
    
    /* 保存上下文 */
    candidate_ctx_save(CAND_ACTIVE_CMD, prefix, prefix_len, NULL);
    candidate_ctx.substr_count = substr_cnt;
    candidate_ctx.total_count = prefix_cnt + substr_cnt;
    
    /* 计算布局 */
    int max_len = 0;
    int total = prefix_cnt + substr_cnt;
    
    /* 先遍历算最大长度 */
    _FOR_EACH_CLI_COMMAND(cmd) {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, prefix, prefix_len) == 0 ||
            strstr(cmd->name, prefix) != NULL) {
            int len = strlen(cmd->name);
            if (len > max_len) max_len = len;
        }
    }
    max_len += 3;
    
    int cows = DISPLAY_MAX_COWS / max_len;
    candidate_ctx.rows = (total + cows - 1) / cows;
    candidate_ctx.cols = cows;
    
    /* 分组 1：前缀匹配 */
    if (prefix_cnt > 0) {
        int cur_cow = 0, cur_idx = 0;
        _FOR_EACH_CLI_COMMAND(cmd) {
            if (!cmd->name || !cli_user_cmd_permitted(cmd))
                continue;
            if (strncmp(cmd->name, prefix, prefix_len) == 0) {
                display_one_cmd_grouped(cmd->name, max_len, cur_idx,
                                        MATCH_TYPE_PREFIX, &cur_cow, &cur_idx);
            }
        }
    }
    
    /* 分组 2：子字符串匹配 */
    if (substr_cnt > 0) {
        int cur_cow = 0, cur_idx = prefix_cnt;  /* 索引续接 */
        _FOR_EACH_CLI_COMMAND(cmd) {
            if (!cmd->name || !cli_user_cmd_permitted(cmd))
                continue;
            /* 跳过前缀匹配项 */
            if (strncmp(cmd->name, prefix, prefix_len) == 0)
                continue;
            if (strstr(cmd->name, prefix) != NULL) {
                display_one_cmd_grouped(cmd->name, max_len, cur_idx,
                                        MATCH_TYPE_SUBSTRING, &cur_cow, &cur_idx);
            }
        }
    }
    
    candidate_list_redraw(candidate_ctx.rows);
}
```

### 4.4 分组展示单元 `display_one_cmd_grouped()`

新增函数，支持颜色区分：

```c
void display_one_cmd_grouped(const char *name, int max_len,
                             int idx, match_type_t type,
                             int *cur_cow, int *cur_idx)
{
    if (*cur_cow == 0)
        cli_out_push((_u8 *)"\r\n", 2);
    
    /* 高亮当前选中项 */
    if (*cur_idx == candidate_ctx.highlight_index) {
        cli_out_push((_u8 *)"\033[7m", 4);  /* 反白背景 */
    }
    
    /* 匹配类型颜色 */
    if (type == MATCH_TYPE_SUBSTRING) {
        cli_out_push((_u8 *)"\033[90m", 4);  /* 灰色 */
    }
    
    cli_out_push((_u8 *)name, strlen(name));
    
    /* 重置颜色 */
    if (type == MATCH_TYPE_SUBSTRING || *cur_idx == candidate_ctx.highlight_index) {
        cli_out_push((_u8 *)"\033[0m", 4);
    }
    
    int space_count = max_len - strlen(name);
    while (space_count--)
        cli_out_push((_u8 *)" ", 1);
    
    (*cur_cow)++;
    if (*cur_cow >= candidate_ctx.cols)
        *cur_cow = 0;
    cli_out_sync();
    (*cur_idx)++;
}
```

> **颜色策略**：
> - 前缀匹配：正常终端默认色（白色）
> - 子字符串匹配：`\033[90m` 灰色
> - 选中项：`\033[7m` 反白（覆盖颜色，选中时灰色/白色都变成反白）

### 4.5 高亮循环适配

现有 `cycle_cmd_candidate_highlight()` 按 `cmd_match_total()` 遍历，需要适配统一列表：

```c
void cycle_cmd_candidate_highlight(void)
{
    clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
    
    int total = candidate_ctx.total_count;  /* 使用缓存的总数 */
    normalize_highlight_index(total);
    
    const cli_command_t *target =
        cmd_find_unified_match_by_index(candidate_ctx.highlight_index);
    if (!target)
        return;
    
    int tok_start = get_current_segment_start(cmd_line.buf, cmd_line.size);
    replace_token_at(tok_start, target->name, (int)strlen(target->name), 1);
    
    /* 重绘统一列表 */
    display_unified_cmd_list(candidate_ctx.prefix, candidate_ctx.prefix_len,
                             /* prefix_cnt */ 0, /* substr_cnt */ 0);  /* 内部重新计算 */
    candidate_list_redraw(candidate_ctx.rows);
    candidate_ctx.active = CAND_ACTIVE_CMD;
    candidate_ctx.cycling = CAND_CYCLING_CMD;
}
```

新增 `cmd_find_unified_match_by_index()`：

```c
const cli_command_t *cmd_find_unified_match_by_index(int idx)
{
    int cur = 0;
    const cli_command_t *cmd;
    
    /* 先遍历前缀匹配 */
    _FOR_EACH_CLI_COMMAND(cmd)
    {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, candidate_ctx.prefix,
                    candidate_ctx.prefix_len) == 0) {
            if (cur == idx)
                return cmd;
            cur++;
        }
    }
    
    /* 再遍历子字符串匹配 */
    _FOR_EACH_CLI_COMMAND(cmd)
    {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, candidate_ctx.prefix,
                    candidate_ctx.prefix_len) == 0)
            continue;  /* 跳过前缀匹配 */
        if (strstr(cmd->name, candidate_ctx.prefix) != NULL) {
            if (cur == idx)
                return cmd;
            cur++;
        }
    }
    return NULL;
}
```

---

## 五、选项补全扩展

### 5.1 长选项补全入口 `complete_long_option()`

同理重构：

```c
void complete_long_option(const cli_command_t *cmd,
                           const char *name_prefix, int name_prefix_len)
{
    cli_option_t *prefix_match = NULL;
    int prefix_cnt = 0;
    cli_option_t *substr_match = NULL;
    int substr_cnt = 0;
    
    for (size_t i = 0; i < cmd->option_count; i++) {
        cli_option_t *opt = &cmd->options[i];
        if (!opt->long_opt)
            continue;
        if (strncmp(opt->long_opt, name_prefix, name_prefix_len) == 0) {
            prefix_cnt++;
            if (prefix_cnt == 1) prefix_match = opt;
        } else if (strstr(opt->long_opt, name_prefix) != NULL) {
            substr_cnt++;
            if (substr_cnt == 1) substr_match = opt;
        }
    }
    
    int total = prefix_cnt + substr_cnt;
    
    if (total == 0) {
        cli_out_push((_u8 *)"\a", 1);
        cli_out_sync();
        return;
    }
    
    /* 唯一前缀匹配 → 直接补全 */
    if (prefix_cnt == 1 && substr_cnt == 0) {
        replace_long_option_only(prefix_match->long_opt,
                                 (int)strlen(prefix_match->long_opt));
        cmd_line_redraw();
        return;
    }
    
    /* 统一展示 */
    display_unified_option_list(cmd, name_prefix, name_prefix_len,
                                prefix_cnt, substr_cnt);
}
```

### 5.2 统一选项列表展示

```c
void display_unified_option_list(const cli_command_t *cmd,
                                  const char *prefix, int prefix_len,
                                  int prefix_cnt, int substr_cnt)
{
    int old_rows = candidate_ctx.rows;
    clear_and_up(old_rows, old_rows);
    candidate_ctx_save(CAND_ACTIVE_LONG_OPTS, prefix, prefix_len, cmd);
    
    int cows = 0;
    int idx = 0;
    
    /* 前缀匹配组 */
    for (size_t i = 0; i < cmd->option_count; i++) {
        cli_option_t *opt = &cmd->options[i];
        if (opt->long_opt &&
            strncmp(opt->long_opt, prefix, prefix_len) == 0) {
            display_one_option_grouped(opt, idx++, MATCH_TYPE_PREFIX, &cows);
        }
    }
    
    /* 子字符串匹配组 */
    for (size_t i = 0; i < cmd->option_count; i++) {
        cli_option_t *opt = &cmd->options[i];
        if (!opt->long_opt)
            continue;
        if (strncmp(opt->long_opt, prefix, prefix_len) == 0)
            continue;
        if (strstr(opt->long_opt, prefix) != NULL) {
            display_one_option_grouped(opt, idx++, MATCH_TYPE_SUBSTRING, &cows);
        }
    }
    
    candidate_ctx.rows = cows;
    candidate_ctx.cols = 1;
    candidate_list_redraw(candidate_ctx.rows);
}
```

### 5.3 分组选项展示单元

```c
void display_one_option_grouped(cli_option_t *opt, int idx,
                                match_type_t type, int *cows)
{
    cli_out_push((_u8 *)"\r\n", 2);
    (*cows)++;
    
    if (idx == candidate_ctx.highlight_index) {
        cli_out_push((_u8 *)"\033[7m", 4);
    }
    
    if (type == MATCH_TYPE_SUBSTRING) {
        cli_out_push((_u8 *)"\033[90m", 4);
    }
    
    if (opt->short_opt) {
        char buf[4] = { '-', opt->short_opt, ' ', '\0' };
        cli_out_push((_u8 *)buf, 3);
    }
    if (opt->long_opt) {
        cli_out_push((_u8 *)"--", 2);
        cli_out_push((_u8 *)opt->long_opt, strlen(opt->long_opt));
    }
    
    if (idx == candidate_ctx.highlight_index || type == MATCH_TYPE_SUBSTRING) {
        cli_out_push((_u8 *)"\033[0m", 4);
    }
    cli_out_sync();
}
```

---

## 六、值补全扩展（STRING 选项候选值）

值补全同样适用统一列表策略，但由于候选值已经是 `char **` 数组，实现更简单。

### 6.1 `do_complete_string_value()` 重构

```c
void do_complete_string_value(cli_option_t *opt,
                               const char *prefix, int prefix_len)
{
    char *prefix_first = NULL;
    int prefix_cnt = 0;
    char *substr_first = NULL;
    int substr_cnt = 0;
    
    for (int i = 0; i < opt->candidate_argc; i++) {
        char *val = opt->candidate_argv[i];
        if (strncmp(val, prefix, prefix_len) == 0) {
            if (prefix_cnt == 0) prefix_first = val;
            prefix_cnt++;
        } else if (strstr(val, prefix) != NULL) {
            if (substr_cnt == 0) substr_first = val;
            substr_cnt++;
        }
    }
    
    int total = prefix_cnt + substr_cnt;
    
    if (total == 0) {
        cli_out_push((_u8 *)"\a", 1);
        cli_out_sync();
        return;
    }
    
    /* 唯一前缀匹配 → 直接补全 */
    if (prefix_cnt == 1 && substr_cnt == 0) {
        replace_cmdline_token(prefix_first, (int)strlen(prefix_first), 1);
        cmd_line_redraw();
        return;
    }
    
    /* 统一展示 */
    display_unified_value_list(opt, prefix, prefix_len, prefix_cnt, substr_cnt);
}
```

---

## 七、状态机兼容性

### 7.1 Tab 按键状态流

现有状态流完全兼容，无需修改 `cli_cmd_line.c`：

```
按下 Tab
    │
    ▼
candidate_ctx.cycling != NONE ? ──是──► STATE_ID_tab_cycle (继续循环)
    │ 否
    ▼
candidate_ctx.active != NONE ? ──是──► STATE_ID_tab_cycle_enter (首次进入循环)
    │ 否
    ▼
STATE_ID_tab_complete (首次补全)
    │
    ▼
complete_command_name() / complete_option() / complete_string_value()
    │
    ▼
唯一前缀匹配 ? ──是──► 直接补全，不进入列表状态
    │ 否
    ▼
展示统一列表，candidate_ctx.active 设为对应类型
```

### 7.2 高亮循环

方向键状态也完全兼容。`completer_cycle()` 通过 `get_completer()` 获取对应类型的 completer，调用 `cycle()` 函数。

需要确保：
- `cmd_completer.cycle = cycle_cmd_candidate_highlight` → 适配统一列表
- `long_opts_completer.cycle = cycle_long_option_highlight` → 适配统一列表
- `values_completer.cycle = cycle_value_highlight` → 适配统一列表

所有 `cycle_*()` 函数的核心改动点：
1. `normalize_highlight_index()` 的参数改为 `candidate_ctx.total_count`
2. 查找函数改为统一版本（先遍历前缀匹配，再遍历子字符串匹配）

### 7.3 输入字符清屏逻辑

`valid_char_task()` 中：

```c
if (candidate_ctx.active != CAND_ACTIVE_NONE) {
    clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
    candidate_ctx_clear();
    cmd_line_redraw();
}
```

此逻辑完全复用，无需修改。用户输入新字符时，列表正常清除。

### 7.4 回车/清屏逻辑

`enter_press()` 和 `clear_handler()` 中的 `candidate_ctx_clear()` 和 `candidate_redraw()` 完全复用。

---

## 八、终端兼容性处理

### 8.1 灰色降级方案

`\033[90m`（明亮黑/灰色）在部分嵌入式串口终端（如 Putty、SecureCRT 默认配置）可能不支持。

**方案 A（推荐）**：在 `cli_config.h` 中通过 Kconfig 提供颜色开关：

```c
/* cli_config.h 或 cli_kconfig.h */
#define CLI_COMPLETION_GRAY_SUBSTRING 1  /* 默认开启 */
```

**方案 B**：运行时降级。先发送灰色序列，如果终端无响应则后续不再发送。但嵌入式场景难以检测终端能力。

**方案 C**：使用 `\033[2m`（暗淡属性），大多数 VT100 兼容终端支持：

```c
#define COLOR_GRAY "\033[2m"  /* 暗淡 */
```

> 推荐方案 C 作为默认实现，因为 `\033[2m` 兼容性优于 `\033[90m`。

### 8.2 无颜色终端

当 `COLOR_TERMINAL_EN == 0` 时，所有颜色宏为空，此时子字符串匹配项无前缀缩进或颜色区分。可通过**前置标记符**弥补：

```bash
lin@linCli> in<Tab>
inline
~ zhaolin
~ zhaolinjing
```

但此方案需要额外判断 `COLOR_TERMINAL_EN`，增加分支复杂度。**建议保持简单**：颜色终端用颜色区分，无颜色终端所有项同等显示，用户仍可通过输入更精确的前缀来筛选。

---

## 九、内存与性能评估

### 9.1 内存开销

| 项目 | 开销 | 说明 |
|------|------|------|
| `struct candidate_ctx` 扩展 | +8 bytes | `substr_count` + `total_count` |
| `match_type_t` 枚举 | 0 bytes | 仅局部变量使用 |
| 新增函数代码 | ~2 KB Flash | 4 个新增展示函数 + 3 个遍历函数 |

总增量：**< 2.5 KB Flash**，**0 RAM**（除 ctx 扩展外）。

### 9.2 性能评估

- 命令数量通常 < 50，选项数量通常 < 20
- 每次 Tab 最多遍历命令列表 2 次（前缀 + 子字符串）
- 时间复杂度：O(N)，N 为命令/选项总数
- 对嵌入式调度器无影响

---

## 十、测试用例

### 10.1 命令名补全测试

```bash
# 测试 1：唯一前缀匹配 → 直接补全
lin@linCli> inte<Tab>
lin@linCli> internal               # 直接补全，追加空格

# 测试 2：多前缀匹配 → 展示前缀列表
lin@linCli> inter<Tab>
internal   interrupt               # 两者都是前缀匹配

# 测试 3：前缀无匹配，子字符串有匹配 → 展示灰色列表
lin@linCli> hao<Tab>
~ zhaolin    ~ zhaolinjing         # 灰色显示

# 测试 4：前缀 + 子字符串混合 → 统一列表
lin@linCli> in<Tab>
inline                          # 白色 = 前缀匹配
~ zhaolin                       # 灰色 = 子字符串匹配
~ zhaolinjing                   # 灰色 = 子字符串匹配

# 测试 5：高亮循环
lin@linCli> in<Tab>
inline                          # 第一次 Tab
~ zhaolin                       # 第二次 Tab（高亮 inline）
~ zhaolinjing                   # 第三次 Tab（高亮 zhaolin）
```

### 10.2 选项补全测试

```bash
# 测试 6：长选项混合匹配
lin@linCli> log --fil<Tab>
--file                          # 前缀匹配
--config-file                   # 子字符串匹配（灰色）
```

### 10.3 值补全测试

```bash
# 测试 7：候选值混合匹配
lin@linCli> log -f deb<Tab>
debug.log                       # 前缀匹配
~ system-debug.log              # 子字符串匹配（灰色）
```

---

## 十一、实现顺序建议

建议按以下顺序实现，每步都可独立编译测试：

1. **Step 1**：修改 `include/cli_completion.h`，新增枚举和 ctx 字段
2. **Step 2**：实现命令名补全的 `find_cmd_substring_match()` 和 `display_unified_cmd_list()`
3. **Step 3**：修改 `complete_command_name()`，接入统一列表
4. **Step 4**：修改 `cycle_cmd_candidate_highlight()` 和 `cmd_find_unified_match_by_index()`
5. **Step 5**：实现选项补全的 `display_unified_option_list()`
6. **Step 6**：实现值补全的 `display_unified_value_list()`
7. **Step 7**：全量测试，验证高亮循环、方向键、清屏逻辑

---

## 十二、附录：完整新增/修改函数清单

### 新增函数（`src/cli/cli_completion.c`）

```c
int find_cmd_substring_match(const char *prefix, int prefix_len,
                             const cli_command_t **first_match);
const cli_command_t *cmd_find_unified_match_by_index(int idx);
void display_unified_cmd_list(const char *prefix, int prefix_len,
                              int prefix_cnt, int substr_cnt);
void display_one_cmd_grouped(const char *name, int max_len,
                             int idx, match_type_t type,
                             int *cur_cow, int *cur_idx);
void display_unified_option_list(const cli_command_t *cmd,
                                  const char *prefix, int prefix_len,
                                  int prefix_cnt, int substr_cnt);
void display_one_option_grouped(cli_option_t *opt, int idx,
                                match_type_t type, int *cows);
void display_unified_value_list(cli_option_t *opt,
                                 const char *prefix, int prefix_len,
                                 int prefix_cnt, int substr_cnt);
```

### 修改函数（`src/cli/cli_completion.c`）

```c
void complete_command_name(const char *prefix, int prefix_len);
void cycle_cmd_candidate_highlight(void);
void complete_long_option(const cli_command_t *cmd,
                           const char *name_prefix, int name_prefix_len);
void cycle_long_option_highlight(void);
void do_complete_string_value(cli_option_t *opt,
                               const char *prefix, int prefix_len);
void cycle_value_highlight(void);
```

### 不修改的文件

- `src/cli/cli_cmd_line.c`：状态机逻辑完全兼容
- `src/cli/cli_edit.c`：token 替换函数完全复用
- `include/cli_candidate.h` / `src/cli/cli_candidate.c`：候选值注册绑定逻辑无关


---

## 十三、子字符串 LCP 填充规则（新增）

### 13.1 设计动机

当多个子字符串匹配结果存在公共部分时，与其直接列出候选列表，不如先帮用户填充这段公共部分。填充后用户有三种选择：

1. **继续补全前缀**：在已填充内容的左侧补充不同前缀
2. **继续补全后缀**：在已填充内容的右侧补充不同后缀
3. **再次按 Tab**：如果不确定前后该填什么，再按一次 Tab 列出候选列表

### 13.2 核心规则

**规则 3（子字符串 LCP 填充原则）**：

- 当且仅当匹配结果**全部为纯子字符串匹配**（即 `前缀匹配数量 == 0`）时，才计算并填充 LCP
- 当存在**前缀匹配 + 子字符串匹配的混合情况**时，**跳过 LCP 填充**，直接展示统一列表（规则 2）

> 混合情况不做 LCP 的原因：前缀匹配的 LCP 从命令开头计算，子字符串匹配的 LCP 从匹配位置计算，两者的语义原点不同，混在一起会导致不可预测的行为。

### 13.3 LCP 计算方式

对于纯子字符串匹配的结果集，LCP 以**匹配位置为锚点**计算：

1. 找到输入子字符串在每个命令名中的起始位置 `pos`
2. **向前扩展检查**：如果所有结果的 `pos > 0`，且 `pos-1` 位置的字符都相同，则将该字符纳入 LCP，并继续向前检查
3. **向后 LCP**：从锚点位置开始，向后计算所有命令名的公共前缀
4. 最终 LCP = 向前扩展部分 + 输入子字符串 + 向后公共部分

```
示例：输入 "int"，匹配 1_int_2, 2_int_3, 4_int_5

1_int_2 中 "int" 的位置 pos = 2
2_int_3 中 "int" 的位置 pos = 2
4_int_5 中 "int" 的位置 pos = 2

向前扩展：pos=1 都是 '_' → 包含；pos=0 分别是 '1','2','4' → 停止
向后 LCP：int_2, int_3, int_5 → 公共前缀 "int_"

最终 LCP = "_int_"（向前 '_' + 输入 'int' + 向后 '_'）
```

### 13.4 交互示例

```bash
# ===== 示例 1：纯子字符串匹配，有公共部分 =====
lin@linCli> int<Tab>
# 系统中：1_int_2, 2_int_3, 4_int_5（无前缀匹配）
# LCP = "_int_"
lin@linCli> _int_<cursor>       # 填充 _int_，不展示列表

# 用户接下来可以：
#   - 按 Home 移光标到开头，输入 "1_" → 1_int_
#   - 直接输入 "_2" → _int__2（不对，需要删掉多余下划线）
#   - 或者：再次按 Tab，列出候选列表

# ===== 示例 2：纯子字符串匹配，无公共部分 =====
lin@linCli> ha<Tab>
# 系统中：zhaolin, zhaolinjing, what（无前缀匹配）
# ha 在 zhaolin 中 pos=2，在 what 中 pos=1
# pos-1 不同（'a' vs 'w'），向后 LCP 只有 "ha"
# LCP 长度 == 输入长度，不填充，直接展示列表：
~ zhaolin    ~ zhaolinjing    ~ what

# ===== 示例 3：混合匹配，不做 LCP 填充 =====
lin@linCli> int<Tab>
# 系统中：int_45（前缀）, 1_int_2（子字符串）, 2_int_3（子字符串）
# 前缀匹配存在 + 子字符串匹配存在 → 混合情况
# 直接展示统一列表，不做任何 LCP 填充：
int_45
~ 1_int_2
~ 2_int_3

# ===== 示例 4：唯一子字符串匹配 =====
lin@linCli> hao<Tab>
# 系统中：zhaolin（无前缀匹配，唯一子字符串匹配）
# 唯一匹配 → 直接补全完整命令名，追加空格
lin@linCli> zhaolin<cursor>
```

### 13.5 交互流程图

```
用户输入 token，按 Tab
    │
    ▼
┌─────────────────────────────────────────────────────────────┐
│ Step 1: 前缀匹配                                            │
│   prefix_cnt = 严格前缀匹配数量                             │
│   substr_cnt = 子字符串匹配数量（排除前缀匹配项）           │
└─────────────────────────────────────────────────────────────┘
    │
    ▼
prefix_cnt == 0 ?
    ├── 否 ──► prefix_cnt == 1 && substr_cnt == 0 ?
    │            ├── 是 ──► 直接补全唯一前缀匹配项（现有行为）
    │            └── 否 ──► 展示统一列表（混合/多前缀歧义）
    │
    └── 是 ──► substr_cnt == 0 ?
                 ├── 是 ──► 响铃（无任何匹配）
                 └── 否 ──► substr_cnt == 1 ?
                               ├── 是 ──► 直接补全唯一子字符串匹配项
                               └── 否 ──► 子字符串 LCP > 输入长度 ?
                                             ├── 是 ──► 填充 LCP，不展示列表
                                             └── 否 ──► 展示子字符串列表
```

### 13.6 关键函数设计

#### `compute_substring_lcp()`

```c
/*
 * 计算纯子字符串匹配结果的 LCP
 * 
 * 参数：
 *   prefix      - 用户输入的子字符串
 *   prefix_len  - 子字符串长度
 *   lcp_buf     - 输出缓冲区
 *   lcp_buf_size - 缓冲区大小
 * 
 * 返回值：
 *   LCP 长度，0 表示无额外公共部分
 */
int compute_substring_lcp(const char *prefix, int prefix_len,
                          char *lcp_buf, int lcp_buf_size)
{
    const cli_command_t *first = NULL;
    int first_pos = -1;
    int match_cnt = 0;
    const cli_command_t *cmd;
    
    /* 第一轮：收集第一个匹配项及其位置 */
    _FOR_EACH_CLI_COMMAND(cmd) {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, prefix, prefix_len) == 0)
            continue;  /* 跳过前缀匹配 */
        const char *pos = strstr(cmd->name, prefix);
        if (pos) {
            if (!first) {
                first = cmd;
                first_pos = pos - cmd->name;
            }
            match_cnt++;
        }
    }
    
    if (match_cnt < 2)
        return 0;  /* 只有一个匹配，不需要 LCP */
    
    /* 初始化 LCP 为第一个匹配项的命令名 */
    int lcp_len = (int)strlen(first->name);
    if (lcp_len > lcp_buf_size) lcp_len = lcp_buf_size;
    memcpy(lcp_buf, first->name, lcp_len);
    
    /* 第二轮：与每个匹配项计算 LCP */
    _FOR_EACH_CLI_COMMAND(cmd) {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, prefix, prefix_len) == 0)
            continue;
        const char *pos = strstr(cmd->name, prefix);
        if (!pos) continue;
        
        int cmd_pos = pos - cmd->name;
        
        /* 如果匹配位置不同，只能 LCP 从匹配位置开始 */
        int start = (cmd_pos < first_pos) ? cmd_pos : first_pos;
        
        /* 重新对齐：找两个字符串从 start 开始的公共前缀 */
        int offset1 = start - (first_pos - start);
        int offset2 = start - (cmd_pos - start);
        
        /* 简化实现：直接以匹配位置为锚点，只保留匹配位置及之后的公共部分 */
        int new_lcp_len = cmd_pos + prefix_len;
        int max_new = (int)strlen(cmd->name);
        for (int i = cmd_pos + prefix_len; i < lcp_len && i < max_new; i++) {
            if (lcp_buf[i] != cmd->name[i]) {
                new_lcp_len = i;
                break;
            }
        }
        if (new_lcp_len < lcp_len) lcp_len = new_lcp_len;
    }
    
    return lcp_len;
}
```

> 注：上述伪代码为示意，实际实现需要处理 `first_pos` 与 `cmd_pos` 不一致时的对齐逻辑。更简洁的做法是：以**最短的匹配位置**为起点，只计算从该位置开始向后的 LCP。

#### 简化版 `compute_substring_lcp()`

```c
int compute_substring_lcp(const char *prefix, int prefix_len,
                          char *lcp_buf, int lcp_buf_size)
{
    const cli_command_t *first = NULL;
    int first_pos = -1;
    const cli_command_t *cmd;
    
    /* 找第一个匹配项 */
    _FOR_EACH_CLI_COMMAND(cmd) {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, prefix, prefix_len) == 0)
            continue;
        const char *p = strstr(cmd->name, prefix);
        if (p) {
            first = cmd;
            first_pos = p - cmd->name;
            break;
        }
    }
    if (!first) return 0;
    
    /* 初始化 LCP 为第一个匹配项（从开头到匹配位置向后延伸） */
    int lcp_len = (int)strlen(first->name);
    if (lcp_len > lcp_buf_size) lcp_len = lcp_buf_size;
    memcpy(lcp_buf, first->name, lcp_len);
    
    /* 与其他匹配项求 LCP */
    _FOR_EACH_CLI_COMMAND(cmd) {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, prefix, prefix_len) == 0)
            continue;
        const char *p = strstr(cmd->name, prefix);
        if (!p) continue;
        
        int pos = p - cmd->name;
        int start = (pos < first_pos) ? pos : first_pos;
        
        /* 对齐：两个字符串都从 start 位置开始比较 */
        int off1 = start + (first->name + first_pos - (first->name + start));
        /* 简化：直接取 min(pos, first_pos) 作为起始，但这样要求两个命令名在该位置之前有相同的填充 */
        
        /* 更简化的策略：LCP 只从 min(pos, first_pos) 开始算，
         * 但要求两个命令名在 min_pos 之前的内容完全一样 */
        int min_pos = (pos < first_pos) ? pos : first_pos;
        int cpl = str_common_prefix_len(lcp_buf, cmd->name);
        if (cpl < lcp_len) lcp_len = cpl;
    }
    
    return lcp_len;
}
```

> 实际上最简洁的工程实现：直接复用 `str_common_prefix_len()`，但要求所有匹配命令名的**对齐版本**有公共前缀。考虑到嵌入式场景的简单性，建议采用**从匹配位置开始向后算 LCP** 的简化策略：

```c
/* 最终简化版 */
int compute_substring_lcp(const char *prefix, int prefix_len,
                          char *lcp_buf, int lcp_buf_size)
{
    const cli_command_t *first = NULL;
    const char *first_match_ptr = NULL;
    const cli_command_t *cmd;
    
    _FOR_EACH_CLI_COMMAND(cmd) {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, prefix, prefix_len) == 0)
            continue;
        const char *p = strstr(cmd->name, prefix);
        if (p) {
            first = cmd;
            first_match_ptr = p;
            break;
        }
    }
    if (!first) return 0;
    
    /* 从匹配位置开始算 LCP */
    int lcp_len = (int)strlen(first_match_ptr);
    if (lcp_len > lcp_buf_size) lcp_len = lcp_buf_size;
    memcpy(lcp_buf, first_match_ptr, lcp_len);
    
    _FOR_EACH_CLI_COMMAND(cmd) {
        if (!cmd->name || !cli_user_cmd_permitted(cmd))
            continue;
        if (strncmp(cmd->name, prefix, prefix_len) == 0)
            continue;
        const char *p = strstr(cmd->name, prefix);
        if (!p) continue;
        int cpl = str_common_prefix_len(lcp_buf, p);
        if (cpl < lcp_len) lcp_len = cpl;
    }
    
    return lcp_len;
}
```

**示例验证**：
- `1_int_2` 中 `int` 的指针指向 `int_2`
- `2_int_3` 中 `int` 的指针指向 `int_3`
- `4_int_5` 中 `int` 的指针指向 `int_5`
- `str_common_prefix_len("int_2", "int_3")` = 4 (`int_`)
- `str_common_prefix_len("int_", "int_5")` = 4 (`int_`)
- 最终 LCP = `int_`，长度 4（输入 `int` 长度 2，所以有 2 个额外字符可填充）

如果用户想要包含前面的 `_`（即 `_int_`），则需要输入 `_int` 而不是 `int`：
- `1_int_2` 中 `_int` 的指针指向 `_int_2`
- `2_int_3` 中 `_int` 的指针指向 `_int_3`
- LCP = `_int_`

### 13.7 `complete_command_name()` 最终逻辑

```c
void complete_command_name(const char *prefix, int prefix_len)
{
    const cli_command_t *prefix_match = NULL;
    int prefix_cnt = find_cmd_match(prefix, prefix_len, &prefix_match);
    
    const cli_command_t *substr_first = NULL;
    int substr_cnt = find_cmd_substring_match(prefix, prefix_len, &substr_first);
    
    /* 无任何匹配 */
    if (prefix_cnt == 0 && substr_cnt == 0) {
        clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
        candidate_ctx_clear();
        cli_out_push((_u8 *)"\a", 1);
        cli_out_sync();
        cmd_line_redraw();
        return;
    }
    
    /* 唯一前缀匹配 → 直接补全 */
    if (prefix_cnt == 1 && substr_cnt == 0) {
        complete_unique_cmd(prefix_match);
        return;
    }
    
    /* 唯一子字符串匹配 → 直接补全完整命令名 */
    if (prefix_cnt == 0 && substr_cnt == 1) {
        complete_unique_cmd(substr_first);
        return;
    }
    
    /* 混合情况（前缀 + 子字符串） → 直接展示统一列表，不做 LCP */
    if (prefix_cnt > 0 && substr_cnt > 0) {
        display_unified_cmd_list(prefix, prefix_len, prefix_cnt, substr_cnt);
        return;
    }
    
    /* 多前缀匹配 → 展示前缀列表（现有行为） */
    if (prefix_cnt > 1) {
        char *lcp = cli_mpool_alloc();
        if (!lcp) { pr_err("out of memory\r\n"); return; }
        complete_multi_cmd(prefix_match, prefix, prefix_len, lcp);
        cli_mpool_free(lcp);
        return;
    }
    
    /* 纯多子字符串匹配 → 先尝试 LCP 填充 */
    if (prefix_cnt == 0 && substr_cnt > 1) {
        char *lcp = cli_mpool_alloc();
        if (!lcp) { pr_err("out of memory\r\n"); return; }
        
        int lcp_len = compute_substring_lcp(prefix, prefix_len, lcp, CMD_LINE_BUF_SIZE);
        
        if (lcp_len > prefix_len) {
            /* LCP 更长 → 填充 */
            replace_cmdline_token(lcp, lcp_len, 0);
            cmd_line_redraw();
        } else {
            /* 无额外公共部分 → 展示子字符串列表 */
            display_unified_cmd_list(prefix, prefix_len, 0, substr_cnt);
        }
        cli_mpool_free(lcp);
        return;
    }
}
```

### 13.8 对选项补全和值补全的扩展

上述 LCP 填充逻辑同样适用于：

- **长选项补全**：纯子字符串匹配时长选项的 LCP 填充
- **值补全**：纯子字符串匹配时候选值的 LCP 填充

实现方式与命令名补全对称，只需将遍历对象从 `cli_command_t` 改为 `cli_option_t` 或 `char **` 即可。

### 13.9 关于光标位置的说明

子字符串 LCP 填充后，光标位于填充内容的**末尾**。例如：

```bash
lin@linCli> int<Tab>
lin@linCli> int_<cursor>        # LCP 填充后
```

如果用户需要补全前缀（如 `1_`），需要自行将光标移到开头（如按 Home 键）。这是子字符串补全的固有限制——匹配点位于命令名中间，系统无法预判用户想补前缀还是后缀。

**缓解措施**：由于 LCP 填充后 `candidate_ctx.active == CAND_ACTIVE_NONE`，用户**再次按 Tab** 会重新触发补全流程。此时：
- 如果输入已变成 `int_`，系统会重新计算匹配（可能是前缀匹配或子字符串匹配）
- 如果仍然多匹配，展示候选列表
- 如果变成唯一匹配，直接补全

这样用户始终可以通过"输入 → Tab → 观察 → 再 Tab"的循环来逐步逼近目标命令。
