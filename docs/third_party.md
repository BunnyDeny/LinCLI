# 🔌 第三方组件（工作队列 workqueue）

> 🎯 本文介绍 LinCLI 内置的第三方工作队列组件，帮助你在 CLI 线程之外安全地投递异步任务～ ✨

---

## 🧩 1. 组件概述

**workqueue** 🚀 是 LinCLI 从 Linux 内核工作队列机制移植而来的轻量级实现，专为 MCU / 裸机环境裁剪。它的**核心定位**是：

> 📍 **在中断上下文（或其他不适合做复杂操作的场景）投递任务，由 LinCLI 调度器线程在任务上下文中统一执行。**

这样可以把中断里不宜做的复杂逻辑（如链表操作、打印输出、状态机推进）安全地推迟到调度器里处理，避免在中断中停留过久。⚠️ 但请注意：**只有执行具体的 work 回调函数时会阻塞 CLI**，`schedule_delayed_work()` 的等待期间只是每 tick 检查一次到期时间，**不会阻塞**命令解析和按键输入。

它提供以下能力：

| 特性 | 说明 |
|------|------|
| 📦 零依赖 | 仅依赖同仓库的 `list` 组件（链表），无外部库 |
| 🚀 异步投递 | 在中断等不方便执行复杂操作的上下文里投递任务，由调度器上下文统一回调执行 |
| ⏱️ 延迟调度 | 支持指定 ticks 后执行，链表按过期时间自动升序排列 |
| 🔄 取消任务 | 提供 `cancel_work_sync()` / `cancel_delayed_work()` 安全撤销 |
| 🧹 flush 能力 | `flush_workqueue()` 等待队列中所有任务完成 |

> ⚠️ **执行上下文提醒** 🔔：workqueue 的回调运行在 **LinCLI 调度器的任务上下文**中（非中断上下文），但调度器是单线程顺序执行的。**只有执行具体的 work 回调函数期间会阻塞 CLI 响应**，`delayed_work` 的延迟等待阶段只是每 tick 检查到期时间，并不会阻塞。因此 work 回调本身不宜占用过长时间。💡

---

## ⚙️ 2. 开启配置（make menuconfig）

```bash
cd /path/to/LinCLI
make menuconfig
```

在菜单中依次进入：

```
Third Party Components
  └── [*] Enable workqueue support
```

✅ 勾选后按 `S` 保存，按 `Q` 退出。重新编译即可生效：

```bash
make
```

> 💡 **依赖自动处理**：`WORKQUEUE` 配置会 `select LIST`，勾选 workqueue 后 list 组件会被**自动连带启用** 🎉，无需手动操作～

---

## 🧪 3. 测试命令 wqtest

编译并运行后，在终端中输入：

```
admin@linCli> wqtest
Scheduling workqueue test jobs...
Jobs scheduled at tick 241, watch tick loop output
[WQ] immediate work executed at tick 242

[WQ] delayed work (300 ticks) executed at tick 541
[WQ] delayed work (600 ticks) executed at tick 841
```

**结果解读** 📊：

| 任务 | 调度时刻 | 执行时刻 | 延迟 |
|------|---------|---------|------|
| ⚡ 立即执行 | tick 241 | tick 242 | ~1 tick |
| ⏰ 延迟 300 ticks | tick 241 | tick 541 | 300 ticks |
| ⏰ 延迟 600 ticks | tick 241 | tick 841 | 600 ticks |

三个任务按预期在各自的时间点执行 🎯，延迟链表的有序排列保证了 FIFO 顺序～

---

## 📄 4. 测试用例源码

以下为 `tests/commands/test_workqueue.c` 的完整代码：

```c
#include "cli_config.h"

#if CLI_ENABLE_DEMO_WORKQUEUE
#include "cmd_dispose.h"
#include "cli_io.h"
#include "workqueue.h"

static void work_fn_immediate(struct work_struct *work)
{
    (void)work;
    cli_printk("[WQ] immediate work executed at tick %u\r\n", jiffies);
}

static void work_fn_delayed_300(struct work_struct *work)
{
    (void)work;
    cli_printk("[WQ] delayed work (300 ticks) executed at tick %u\r\n", jiffies);
}

static void work_fn_delayed_600(struct work_struct *work)
{
    (void)work;
    cli_printk("[WQ] delayed work (600 ticks) executed at tick %u\r\n", jiffies);
}

static int wqtest_handler(void *_args)
{
    (void)_args;
    static struct work_struct wq_immediate;
    static struct delayed_work wq_delayed_300;
    static struct delayed_work wq_delayed_600;

    INIT_WORK(&wq_immediate, work_fn_immediate);
    INIT_DELAYED_WORK(&wq_delayed_300, work_fn_delayed_300);
    INIT_DELAYED_WORK(&wq_delayed_600, work_fn_delayed_600);

    cli_printk("Scheduling workqueue test jobs...\r\n");
    schedule_work(&wq_immediate);
    schedule_delayed_work(&wq_delayed_300, 300);
    schedule_delayed_work(&wq_delayed_600, 600);
    cli_printk("Jobs scheduled at tick %u, watch tick loop output\r\n", jiffies);
    return 0;
}

CLI_COMMAND_NO_STRUCT(wqtest, "wqtest", "Test workqueue scheduling",
                      wqtest_handler);

#endif /* CLI_ENABLE_DEMO_WORKQUEUE */
```

---

## 🔧 5. 核心 API 详解

使用工作队列前，请先包含头文件：

```c
#include "workqueue.h"
```

### 5.1 🔨 INIT_WORK — 初始化立即执行的工作

```c
#define INIT_WORK(_work, _func)
```

| 参数 | 说明 |
|------|------|
| `_work` | 指向 `struct work_struct` 的指针 |
| `_func` | 回调函数，原型 `void func(struct work_struct *work)` |

**示例** 📝：

```c
static struct work_struct my_work;

static void my_handler(struct work_struct *work)
{
    (void)work;
    cli_printk("Work executed!\r\n");
}

INIT_WORK(&my_work, my_handler);
```

### 5.2 ⏳ INIT_DELAYED_WORK — 初始化延迟工作

```c
#define INIT_DELAYED_WORK(_dwork, _func)
```

| 参数 | 说明 |
|------|------|
| `_dwork` | 指向 `struct delayed_work` 的指针 |
| `_func` | 回调函数，原型 `void func(struct work_struct *work)` |

**示例** 📝：

```c
static struct delayed_work my_dwork;

INIT_DELAYED_WORK(&my_dwork, my_handler);
```

### 5.3 📤 schedule_work — 投递立即执行的工作

```c
bool schedule_work(struct work_struct *work);
```

| 参数 | 说明 |
|------|------|
| `work` | 已初始化的 `struct work_struct` 指针 |

| 返回值 | 含义 |
|--------|------|
| ✅ `true` | 投递成功 |
| ❌ `false` | 该工作已在队列中（重复投递会被拒绝） |

**示例** 📝：

```c
schedule_work(&my_work);
```

### 5.4 📤⏰ schedule_delayed_work — 投递延迟工作

```c
bool schedule_delayed_work(struct delayed_work *dwork, unsigned long delay_ticks);
```

| 参数 | 说明 |
|------|------|
| `dwork` | 已初始化的 `struct delayed_work` 指针 |
| `delay_ticks` | 延迟的 tick 数，到期后自动转入立即执行队列 |

| 返回值 | 含义 |
|--------|------|
| ✅ `true` | 投递成功 |
| ❌ `false` | 该延迟工作已在队列中 |

**示例** 📝：

```c
schedule_delayed_work(&my_dwork, 300);  /* 300 ticks 后执行 */
```

---

## ⚠️ 6. 使用注意事项

### 🔴 栈变量陷阱

`work_struct` / `delayed_work` **必须定义为全局变量、文件静态变量或长生命周期结构体成员**。绝对禁止在栈上定义后调用 `schedule_work()`：

```c
/* ❌ 错误示范 — 栈变量，函数返回后内存失效 */
void bad_example(void)
{
    struct work_struct w;
    INIT_WORK(&w, handler);
    schedule_work(&w);  /* 悬垂指针，运行时崩溃 💥 */
}

/* ✅ 正确示范 — 静态全局变量 */
static struct work_struct w;

void good_example(void)
{
    INIT_WORK(&w, handler);
    schedule_work(&w);  /* 安全 😊 */
}
```

### 🖨️ 打印格式限制

LinCLI 的 `cli_vsnprintf` 仅支持 `%d %u %s %c %%`，**不支持 `%lu`**。打印 tick 值时请使用 `%u`：

```c
/* ❌ 不支持 %lu，输出可能异常 */
cli_printk("tick = %lu\r\n", jiffies);

/* ✅ 使用 %u 打印 */
cli_printk("tick = %u\r\n", jiffies);
```

### 🔗 调度器集成

workqueue 已与 LinCLI 调度器完成集成，用户无需手动调用 tick handler：

```c
/* src/init/scheduler.c 中已自动完成 */
scheduler_task()
{
    jiffies = scheduler_ticks;
    workqueue_tick_handler(system_wq, jiffies);  /* 扫描延迟链表 */
    workqueue_run_one(system_wq);                /* 执行一个立即任务 */
}
```

你只需要在业务代码中调用 `schedule_work()` 或 `schedule_delayed_work()` 投递任务即可～ 🎉

### ⏱️ 避免耗时操作

再次提醒 ⚠️：**只有执行 work 回调函数期间会阻塞 CLI 响应**，`delayed_work` 的延迟等待阶段并不会阻塞。因此 work 回调本身不宜占用过长时间。😅

---

## 📋 7. 完整使用流程

```c
#include "workqueue.h"

/* 1️⃣ 定义全局/静态工作结构体 */
static struct work_struct led_work;
static struct delayed_work sensor_work;

/* 2️⃣ 实现回调函数 */
static void led_toggle(struct work_struct *work)
{
    (void)work;
    gpio_toggle(LED_PIN);
}

static void sensor_read(struct work_struct *work)
{
    (void)work;
    adc_start_conversion();
}

/* 3️⃣ 在初始化函数中注册 */
void app_init(void)
{
    INIT_WORK(&led_work, led_toggle);
    INIT_DELAYED_WORK(&sensor_work, sensor_read);
}

/* 4️⃣ 在命令处理或中断中投递 */
static int led_cmd_handler(void *args)
{
    (void)args;
    schedule_work(&led_work);                   /* ⚡ 立即翻转 LED */
    schedule_delayed_work(&sensor_work, 100);   /* ⏰ 100 ticks 后读取传感器 */
    return 0;
}
```

---

## 📝 8. 相关配置宏

| 宏 | 来源 | 说明 |
|----|------|------|
| `WORKQUEUE` | `cli_kconfig.h` | workqueue 组件总开关 |
| `LIST` | `cli_kconfig.h` | 链表组件开关（被 WORKQUEUE 自动 select） |
| `CLI_ENABLE_DEMO_WORKQUEUE` | `cli_kconfig.h` | `wqtest` 测试命令开关 |

---

> 🎉 至此，你已经掌握了 LinCLI workqueue 的全部用法！快去试试吧～ 如有问题欢迎提交 Issue 或 PR！ 🚀
