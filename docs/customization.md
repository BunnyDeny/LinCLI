# 🎯 用户可定制接口


### 💡 日志系统 `cli_printk`

`cli_printk` 的使用方式**一比一复刻 Linux 内核的日志打印函数**。框架推荐直接在**命令响应函数（handler）**中使用它，也推荐在框架内部调试时使用；但需要注意，由于它内部会调用 `cli_out_push` / `cli_out_sync` 进行输出同步，**不能在中断上下文里调用**。如果你的移植代码把串口接收函数放到了中断里，那么该中断函数中也不应调用 `cli_printk`。

```c
/* cli_io.h */
#define pr_emerg(fmt, ...)  cli_printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)  cli_printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)   cli_printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)    cli_printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)   cli_printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...) cli_printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)   cli_printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)  cli_printk(KERN_DEBUG fmt, ##__VA_ARGS__)
```

#### 日志级别含义

| 宏 | Level | 说明 |
|----|-------|------|
| `pr_emerg`  | 0 | 系统不可用，最严重 |
| `pr_alert`  | 1 | 必须立即处理 |
| `pr_crit`   | 2 | 严重故障 |
| `pr_err`    | 3 | 错误 |
| `pr_warn`   | 4 | 警告 |
| `pr_notice` | 5 | 正常但重要 |
| `pr_info`   | 6 | 一般信息 |
| `pr_debug`  | 7 | 调试信息 |

#### 日志过滤

框架通过全局变量 `log_level` 控制日志过滤阈值：

```c
/* cli_io.h */
extern char log_level[3];
```

默认值为 `"8"`（即不过滤，全部输出）。你可以在自己的初始化代码中修改它：

```c
/* 只显示警告及以上级别（level <= 4） */
log_level[0] = '4';
```

#### 自定义日志前缀和颜色

每条日志级别的前缀文本和颜色都是通过一组 `__attribute__((weak))` 函数生成的。如果你想统一修改样式（例如去掉前缀、改成自己的项目配色），只需在工程中重新实现对应函数：

```c
/* 默认定义在 src/cli/cli_io.c 中，可被覆盖 */
const char *pre_ERR_gen(void)
{
    return "[ERR] ";  /* 去掉默认红色，改成纯文本 */
}

const char *pre_INFO_gen(void)
{
    return "[INFO] ";
}
```

可供覆盖的函数列表：`pre_EMERG_gen`、`pre_ALERT_gen`、`pre_CRIT_gen`、`pre_ERR_gen`、`pre_WARNING_gen`、`pre_NOTICE_gen`、`pre_INFO_gen`、`pre_DEBUG_gen`、`pre_DEFAULT_gen`。

### ⚙️ 命令提示符样式

命令提示符（`lin@linCli>`）通过 `cli_prompt_print` 输出，它也是一个弱定义函数（默认定义在 `src/init/scheduler.c`）。你可以在自己的项目中覆盖它，改成你想要的提示符：

```c
void cli_prompt_print(void)
{
    cli_printk(COLOR_BOLD COLOR_RED "MyDevice" COLOR_NONE "> ");
}
```

修改后，终端提示符将变为：

```bash
MyDevice>
```

---

## 🚀 写在最后

## ✂️ 裁剪与体积优化

LinCLI 采用模块化条件编译设计，所有功能都可通过 Kconfig 开关按需裁剪。下面是各配置项速查表和常用裁剪组合的体积参考。

### 🎯 LinCLI Core 配置速查

| 🏷️ 配置项（menuconfig 中的名字） | 🔤 对应 C 宏 | 📐 类型 | 🔘 默认值 | 📝 说明 |
|------------------------------|-----------|:----:|:------:|------|
| Enable user system | `CLI_ENABLE_USER` | bool | ✅ | 用户管理系统（`su` / 权限检查） |
| Enable environment variables | `CLI_ENABLE_ENV` | bool | ✅ | 环境变量系统（`env` / `$NAME`） |
| Enable variable system | `CLI_ENABLE_VAR` | bool | ✅ | 变量导出系统（`var`） |
| Enable advanced tab completion | `CLI_ENABLE_ADVANCED_COMPLETION` | bool | ✅ | 高级 Tab 补全（选项补全、候选值、高亮循环） |
| Enable help system | `CLI_ENABLE_HELP` | bool | ✅ | `--help` 自动生成与用法提示 |
| Enable command chaining (&&) | `CLI_ENABLE_CMD_CHAIN` | bool | ✅ | 命令链式执行 |
| Enable auto-run commands | `CLI_ENABLE_AUTO_RUN` | bool | ✅ | 开机自动执行命令 |
| Command history entries | `HISTORY_MAX` | int | 4 | 命令历史记录条数，嵌入式建议保持较小 |
| Max columns for tab completion | `DISPLAY_MAX_COWS` | int | 50 | Tab 补全显示的最大列数 |
| Command argument shared buffer size | `CLI_CMD_BUF_SIZE` | int | 128 | 命令参数全局共享缓冲区 |
| Memory pool block count | `CLI_MPOOL_COUNT` | int | 6 | 内存池块数量 |

### 🧪 Tests & Demos 配置速查

| 🏷️ 配置项 | 🔤 对应 C 宏 | 📐 类型 | 🔘 默认值 | 📝 说明 |
|-----------|-----------|:----:|:------:|------|
| Enable inline tail print test | `CLI_ENABLE_SCHEDULER_TICK_PRINT` | bool | ❌ | 调度器尾行打印测试 |
| Enable Unity unit tests | `CLI_ENABLE_UNIT_TESTS` | bool | ✅ | 编译 Unity 单元测试可执行文件 |
| Enable auto cmd demo | `CLI_ENABLE_DEMO_AUTO_CMD` | bool | ❌ | `auto_cmd` 演示命令 |
| Enable bool test demo | `CLI_ENABLE_DEMO_BOOL` | bool | ❌ | `tb` 布尔测试命令 |
| Enable buf insufficient test demo | `CLI_ENABLE_DEMO_BUF_INSUFFICIENT` | bool | ❌ | 缓冲区不足测试命令 |
| Enable callback test demo | `CLI_ENABLE_DEMO_CALLBACK` | bool | ❌ | 回调测试命令 |
| Enable cli var test demo | `CLI_ENABLE_DEMO_CLI_VAR` | bool | ❌ | 变量系统测试命令 |
| Enable conflicts test demo | `CLI_ENABLE_DEMO_CONFLICTS` | bool | ❌ | 互斥选项测试命令 |
| Enable double test demo | `CLI_ENABLE_DEMO_DOUBLE` | bool | ❌ | 浮点数测试命令 |
| Enable env test demo | `CLI_ENABLE_DEMO_ENV` | bool | ❌ | 环境变量测试命令 |
| Enable init_d test demo | `CLI_ENABLE_DEMO_INIT_D` | bool | ❌ | 初始化函数测试命令 |
| Enable int test demo | `CLI_ENABLE_DEMO_INT` | bool | ❌ | 整数测试命令 |
| Enable int array test demo | `CLI_ENABLE_DEMO_INT_ARRAY` | bool | ❌ | 整数数组测试命令 |
| Enable key interaction test demo | `CLI_ENABLE_DEMO_KEY_INTERACTION` | bool | ❌ | 键盘交互测试命令 |
| Enable led test demo | `CLI_ENABLE_DEMO_LED` | bool | ❌ | LED 演示命令 |
| Enable log test demo | `CLI_ENABLE_DEMO_LOG` | bool | ❌ | 日志测试命令 |
| Enable motor test demo | `CLI_ENABLE_DEMO_MOTOR` | bool | ❌ | 电机演示命令 |
| Enable required test demo | `CLI_ENABLE_DEMO_REQUIRED` | bool | ❌ | 必需选项测试命令 |
| Enable scope test demo | `CLI_ENABLE_DEMO_SCOPE` | bool | ❌ | 示波器测试命令 |
| Enable string test demo | `CLI_ENABLE_DEMO_STRING` | bool | ❌ | 字符串测试命令 |
| Enable with buf test demo | `CLI_ENABLE_DEMO_WITH_BUF` | bool | ❌ | 独立缓冲区测试命令 |

> 💡 **关于演示命令**：`tests/commands/` 下的每个 `.c` 文件都有自己的独立宏开关。开启后该命令会被注册到 CLI 中；关闭后源码仍会被编译，但命令不会出现在终端里。这让你可以按需选择想体验的演示功能。

### 📏 常用裁剪组合与体积参考

以下数据基于 ARM Cortex-M4（`arm-none-eabi-gcc -Os`，无 LTO）：

| ⚙️ 配置 | 💾 Flash |
|------|:-----:|
| **全开（默认）** | **~22.4 KB** |
| 关高级补全 | ~17.7 KB |
| 关高级补全 + 环境变量 + 变量导出 | ~13.9 KB |
| 关上述三项 + 帮助 + 命令链 + 自动运行 | ~12.8 KB |
| **全部关闭（仅核心骨架）** | **~11.5 KB** |
