# 异步非阻塞命令

## 为什么需要异步命令？

裸机 MCU 只有一个 `while(1)` 主循环。传统 `CLI_COMMAND` 注册的命令使用单一 handler，框架会**同步阻塞式**调用它——handler 不返回，`while(1)` 就卡在那里。这意味着：

- handler 里写 `delay_ms(500)` → 整个 MCU 冻结 500ms，你自己的业务代码全停摆
- handler 里 `while(!flag)` 空转等待 → CPU 被吃满，ADC 不采样、电机不控制、看门狗不喂
- 即使命令本身只需要"逐步完成"（如电机缓慢升速），也必须一次性阻塞到底

**非阻塞模式的目的只有一个：不要让命令 handler 独占 CPU。**

`task` 每次只执行一个时间片，做完就把 CPU 交还给你的 `while(1)`。你的主循环可以去跑自己的业务逻辑（读传感器、控其他电机、喂看门狗）。下次轮询再回来继续执行这个命令。同一个命令跨多次轮询完成，但**任何时刻都只执行这一个命令**，CLI 不会也不可能去处理新的输入。

一个需要 5 秒才能完成的命令，不会吃掉这 5 秒内的全部 CPU，而是和你的其他代码分时复用这一个 `while(1)`。

---

## 三阶段接口

```c
void (*cmd_entry)(void *);   // 命令入口，只执行一次
int  (*cmd_task) (void *);   // 命令主体，每次调度轮询执行一次
void (*cmd_exit) (void *);   // 命令出口，只执行一次
```

| 阶段 | 调用时机 | 用途 |
|------|---------|------|
| `entry` | 状态进入 `scheduler_cmd_run` 时 | 一次性初始化：打开设备、申请资源、设置初始状态 |
| `task`  | 每次 `scheduler_task()` 轮询时 | 执行业务逻辑的一个时间片：检查状态、推进进度、发送数据 |
| `exit`  | 状态离开 `scheduler_cmd_run` 时 | 一次性清理：关闭设备、释放资源、打印结果 |

---

## task 返回值语义

`cmd_task` 的返回值决定命令的生命周期：

| 返回值 | 含义 | 调度器行为 |
|--------|------|-----------|
| `CLI_CONTINUE` (`1`) | 本次执行完毕，但命令不结束 | 保持当前命令状态，下次轮询再次调用 `task` |
| `0` | 执行成功，命令结束 | 执行 `exit`，该命令结束，回到提示符/下一命令 |
| `< 0` | 执行失败，命令结束 | 同上，框架会在 `exit` 后自动打印 `[ERR] command 'xxx' execution failed, return value: N` |
| `> 1` | 扩展语义，视为成功结束 | 同 `0` |

**设计原则**：`cmd_task` 的返回值语义和普通 C 函数完全一致——`0` 成功，负值失败。只有明确返回 `CLI_CONTINUE` 时才继续执行。

---

## 注册异步命令

使用 `CLI_COMMAND_ASYNC` 宏：

```c
#include "cmd_dispose.h"
#include "cli_io.h"

struct motor_args {
    int target_rpm;
};

static int current_rpm = 0;

static void motor_entry(void *_args)
{
    struct motor_args *args = _args;
    current_rpm = 0;
    cli_printk("Motor starting, target=%d rpm\r\n", args->target_rpm);
}

static int motor_task(void *_args)
{
    struct motor_args *args = _args;

    if (current_rpm < args->target_rpm) {
        current_rpm += 100;
        cli_printk("Motor ramping: %d rpm\r\n", current_rpm);
        return CLI_CONTINUE;   /* 还没到达目标，下次继续 */
    }

    cli_printk("Motor reached target!\r\n");
    return 0;                   /* 成功完成 */
}

static void motor_exit(void *_args)
{
    cli_printk("Motor command done.\r\n");
}

CLI_COMMAND_ASYNC(motor, "motor", "Ramp motor to target RPM",
                  motor_entry, motor_task, motor_exit,
                  (struct motor_args *)0,
                  OPTION('r', "rpm", INT, "Target RPM", struct motor_args,
                         target_rpm, 0, NULL, NULL, true),
                  END_OPTIONS);
```

终端效果：

```bash
lin@linCli> motor -r 500
Motor starting, target=500 rpm
Motor ramping: 100 rpm
Motor ramping: 200 rpm
Motor ramping: 300 rpm
Motor ramping: 400 rpm
Motor ramping: 500 rpm
Motor reached target!
Motor command done.
lin@linCli>
```

`motor_task` 被调用了 6 次（每次调度周期 10ms，实际间隔取决于你的 `scheduler_task()` 调用频率）。每次执行完 motor_task 后，CPU 控制权交还给你的 `while(1)`，你可以去跑自己的业务代码。下次轮询再回来继续执行这个 motor 命令——**注意：这期间 CLI 不会处理新的输入，始终只执行这一个命令。**

---

## 与旧命令的对比

| 特性 | `CLI_COMMAND`（旧） | `CLI_COMMAND_ASYNC`（新） |
|------|---------------------|---------------------------|
| 注册宏 | `CLI_COMMAND(...)` | `CLI_COMMAND_ASYNC(...)` |
| 响应函数 | 单 `validator(void *)` | `entry/task/exit` 三阶段 |
| 执行方式 | 同步阻塞，一次完成 | 非阻塞，跨多次轮询分片执行 |
| 返回值 | `0`=成功，`<0`=失败 | `0`=成功退出，`<0`=失败退出，`CLI_CONTINUE`=继续 |
| CPU 独占 | 是（handler 不返回，CPU 被吃满） | 否（执行一个时间片后交出 CPU） |
| 适用场景 | 瞬时操作（点灯、读寄存器） | 耗时操作（电机控制、擦 Flash、等待传感器） |

**向后兼容**：旧命令**无需任何修改**。框架通过 `entry == NULL && exit == NULL` 自动识别旧命令，仍然执行一次即退出。

---

## 状态流转

异步命令的执行被封装在 `scheduler_cmd_run` 状态中：

```
scheduler_dispose（解析命令参数）
        │
        ▼ state_switch
scheduler_cmd_run
        │
        ├── entry()  ─────────────────────────┐
        │                                      │ 只执行一次
        ├── task() → CLI_CONTINUE ────────────┤
        │   ↑                                  │
        │   └── 下次 scheduler 轮询再次进来 ────┤
        │                                      │
        ├── task() → 0 或 <0 ─────────────────┘
        │
        └── exit()
                │
                ▼
        回到 scheduler_dispose（命令链下一个）
        或 scheduler_get_char（回到提示符）
```

- `entry` 和 `exit` 利用状态机的 `state_entry` / `state_exit` 机制保证**只执行一次**
- `task` 每次返回 `CLI_CONTINUE` 时，状态不切换，下次 `scheduler_task()` 轮询继续执行
- 只有 `task` 返回 `0`、负值或 `> 1` 时，状态机才会离开 `scheduler_cmd_run`，触发 `exit`

---

## 错误处理

当 `cmd_task` 返回负值时，调度器在 `exit` 阶段会自动打印错误信息：

```bash
lin@linCli> motor -r 500
Motor starting, target=500 rpm
[ERR] command 'motor' execution failed, return value: -3
Motor command done.
lin@linCli>
```

错误码由 `cmd_task` 自行定义（如 `-1` = 设备离线，`-2` = 参数超限，`-3` = 超时等），框架只负责记录和打印，不解释具体含义。

---

## 注意事项

1. **不要在中断里调用 `CLI_COMMAND_ASYNC` 命令**：`task` 的执行上下文是调度器主循环，必须保证可重入性。如果命令需要和 ISR 交换数据，请用环形缓冲区或全局标志位。

2. **`arg_buf` 的生命周期**：`entry` → 所有 `task` → `exit` 期间，`arg_buf` 始终有效。`exit` 结束后框架会自动释放动态分配的缓冲区。不要在命令结束后持有 `arg_buf` 指针。

3. **长时间阻塞仍然不好**：即使使用了异步接口，单次 `task` 的执行时间也应该尽量短（建议 < 1ms）。如果 `task` 里做了 `delay_ms(100)` 或大量浮点运算，仍然会推迟你的 `while(1)` 下一次迭代。

4. **命令链中的异步命令**：`cmd1 && cmd2` 中如果 `cmd1` 是异步命令，它会完整执行完（`entry` → 多次 `task` → `exit`）之后，才会开始执行 `cmd2`。这是正确的行为——命令链的语义是"前一个成功后才执行后一个"。

5. **自启动命令支持异步**：`cli_auto_cmds[]` 中的命令可以是异步命令，框架会正确等待它完成后再执行下一个自启动命令。
