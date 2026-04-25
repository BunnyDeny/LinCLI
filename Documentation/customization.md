# 用户可定制接口


### 日志系统 `cli_printk`

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
#define pr_cont(fmt, ...)   cli_printk(KERN_CONT fmt, ##__VA_ARGS__)
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
| `pr_cont`   | c | 继续上一条日志（不换行） |

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
/* 默认定义在 cli/Src/cli_io.c 中，可被覆盖 */
const char *pre_ERR_gen(void)
{
    return "[ERR] ";  /* 去掉默认红色，改成纯文本 */
}

const char *pre_INFO_gen(void)
{
    return "[INFO] ";
}
```

可供覆盖的函数列表：`pre_EMERG_gen`、`pre_ALERT_gen`、`pre_CRIT_gen`、`pre_ERR_gen`、`pre_WARNING_gen`、`pre_NOTICE_gen`、`pre_INFO_gen`、`pre_DEBUG_gen`、`pre_DEFAULT_gen`、`pre_CONT_gen`。

### 命令提示符样式

命令提示符（`lin@linCli>`）通过 `cli_prompt_print` 输出，它也是一个弱定义函数（默认定义在 `init/scheduler.c`）。你可以在自己的项目中覆盖它，改成你想要的提示符：

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

## 写在最后
