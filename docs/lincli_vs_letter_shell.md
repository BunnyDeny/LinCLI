# ⚖️ LinCLI vs letter-shell：选型指南

> 💡 本文档旨在帮助开发者在 **LinCLI** 与 **letter-shell** 之间做出适合自身项目的选择。两款框架均为 🇨🇳 国产优秀的嵌入式 C 语言 CLI 解决方案，采用 MIT 许可证，但在设计哲学上走了两条截然不同的道路。
>
> ✨ 本文尽可能客观，不贬低任何一方。最终选择权在你。

---

## 🎯 一、一句话定位

| 🏷️ 项目 | 💬 一句话描述 |
|--------|-------------|
| **LinCLI** | 🎛️ 一个面向产品的**微型 GNU 风格命令行解析器**，强调选项约束、自动生成帮助、异步调度 |
| **letter-shell** | 🎮 一个面向调试的**远程函数调用框架**，强调快速导出 C 函数、运行时参数转换 |

---

## 🧠 二、核心设计哲学对比

### 🎛️ LinCLI：命令是接口

LinCLI 把终端输入视为**带语法和语义的命令行**，类似于 Linux 下的 `ls -la /tmp`。

- ✅ 命令有**命名选项**（`--on`、`-b 80`）
- ✅ 选项之间有**依赖、互斥、必需**等约束关系
- ✅ 框架负责解析、校验、填充结构体，handler 只处理业务逻辑
- ✅ 自动生成 `--help`，用户看到的是专业的用法提示

💡 **适合 mindset**："我在设计一个给**人**用的命令行界面。"

### 🎮 letter-shell：终端是遥控器

letter-shell 把终端输入视为**对程序内部函数的远程调用**，类似于 RPC。

- 🔹 核心抽象是"导出 C 函数"
- 🔹 支持 `main(argc, argv)` 形式，也支持普通 C 函数形式（`func(int a, char *s)`）
- 🔹 运行时根据函数签名推断参数类型并进行字符串转换
- 🔹 命令格式更接近函数调用：`led 1 80` 而不是 `led --on -b 80`

💡 **适合 mindset**："我在给内部函数加一个**串口入口**，方便调试。"

---

## 📊 三、功能矩阵

| 🔧 功能 | 🎛️ LinCLI | 🎮 letter-shell |
|--------|:---------:|:--------------:|
| 命令自动收集（GCC section） | ✅ | ✅ |
| 运行时零注册（无需手动维护表） | ✅ | ✅ |
| **命名选项解析**（`--opt` / `-x`） | ✅ | ❌ |
| **选项依赖/互斥/必需检查** | ✅ | ❌ |
| **自动生成 --help** | ✅ | ⚠️ 快速帮助（双击 Tab） |
| **选项级 Tab 补全** | ✅ | ❌ |
| **候选值 Tab 补全**（STRING 选项） | ✅ | ❌ |
| **高亮循环导航**（↑↓←→选候选） | ✅ | ❌ |
| 命令级 Tab 补全 | ✅ | ✅ |
| 命令历史 | ✅ | ✅ |
| 光标移动 | ✅ | ✅ |
| **命令链**（`cmd1 && cmd2`） | ✅ | ❌ |
| **环境变量**（`$NAME`） | ✅ | ✅ |
| **变量导出**（运行时读写全局变量） | ✅ | ✅ |
| 用户管理 | ✅ | ✅ |
| 权限管理 | ✅ | ✅ |
| **异步非阻塞命令**（entry/task/exit） | ✅ 原生支持 | ❌ 需自行实现 |
| **开机自动执行命令** | ✅ | ❌ |
| **日志分级过滤**（`pr_err`/`pr_info`） | ✅ | ❌ |
| 执行未导出函数（`exec addr`） | ❌ | ✅ |
| 快捷键绑定（F1-F12 等） | ❌ | ✅ |
| 伴生对象（per-shell 私有数据） | ❌ | ✅ |
| 代理函数/自定义参数解析器 | ⚠️ CALLBACK 透传 | ✅ |
| 数组参数 | ✅ `INT_ARRAY` | ✅ `[1,2,3]` |
| 工作队列（workqueue） | ✅ 内置 | ❌ |
| 尾行模式打印 | ✅ | ✅ |
| 配置系统 | ✅ Kconfig（menuconfig） | ⚠️ `shell_cfg.h` 宏 |
| 构建系统 | ✅ CMake + CTest | ⚠️ 手动集成 |
| 单元测试 | ✅ Unity 框架 | ❌ |
| 体积测量工具 | ✅ `measure_size.sh` | ❌ |

---

## 🗺️ 四、典型场景选型建议

### 🏭 场景 A：产品交付 —— 选 LinCLI

**📋 特征**：
- 🎯 设备卖给客户，终端界面是产品的一部分
- 🎯 产线工人、现场工程师、系统集成商会通过串口配置设备
- 🎯 命令有复杂的参数约束（如"擦除 Flash 时必须指定 `--chip` 或 `--sector`，但不能同时指定"）
- 🎯 需要自动生成帮助文档，降低培训成本

**💎 理由**：
- ✅ `led --on -b 80` 比 `led 1 80` 更具自解释性
- ✅ `--help` 自动显示选项类型、依赖关系、互斥关系
- ✅ 选项级 Tab 补全让用户无需记忆具体选项名
- ✅ 命令链支持批量操作流程（`calibrate --adc && save --params`）
- ✅ Kconfig 条件编译让不同 SKU 可以裁剪不同功能组合

### 🔧 场景 B：研发调试

**📋 特征**：
- 🔹 开发阶段需要快速验证硬件功能
- 🔹 团队内部使用，不面向外部客户
- 🔹 已有大量 C 函数（`read_adc()`、`set_pwm()`），希望快速暴露到串口

**💎 letter-shell 更适合的情况**：
- ✅ `SHELL_EXPORT_CMD(func, func, "test")` 三秒导出一个函数，零学习成本
- ✅ `exec 0x08028621` 调试未导出函数是底层开发的救命稻草
- ✅ 快捷键绑定（如 F1 触发复位、F2 打印状态）在调试时效率极高

**💎 LinCLI 更适合的情况**：
- ✅ 调试命令参数复杂、选项多，Tab 补全和 `--help` 比死记硬背参数顺序更方便
- ✅ 需要反复切换不同调试模式（`--verbose` / `--debug` / `--profile`），选项级补全减少输入错误
- ✅ 多人协作调试，自动生成帮助降低沟通成本
- ✅ 调试命令最终会直接变成产品命令，无需二次重写

> 💡 **实际上**，LinCLI 在调试场景下往往**更方便**——尤其是当命令带有多个选项时，`--help` + Tab 补全比回忆函数签名省力得多。

### ⚙️ 场景 C：RTOS/Linux 嵌入式子系统 —— 选 LinCLI

**📋 特征**：
- 🎯 CLI 作为系统的一个模块，需要与日志、任务调度、文件系统协同
- 🎯 需要异步执行耗时命令（电机运动、传感器等待、Flash 擦写）
- 🎯 需要环境变量、变量导出、工作队列等系统级抽象

**💎 理由**：
- ✅ 异步三阶段命令（`entry`/`task`/`exit` + `CLI_CONTINUE`）天然适配 RTOS 调度
- ✅ `pr_err`/`pr_info`/`pr_debug` 日志分级与系统日志体系一致
- ✅ 内置 workqueue 支持延迟任务投递
- ✅ 双状态机架构（命令行状态机 + 调度器状态机）让输入处理与命令执行解耦

### 🪶 场景 D：极简资源约束（< 8KB Flash）—— 选 letter-shell 或更轻方案

**📋 特征**：
- 🔹 MCU Flash 极小（如 STM32F031 的 32KB Flash）
- 🔹 只需要"输入命令名 → 执行函数"，不需要选项解析

**💎 理由**：
- ✅ letter-shell 核心更紧凑，可通过减小缓冲区进一步裁剪
- ✅ 如果连 letter-shell 都嫌大，可以考虑 **microrl**（只做行编辑）或 **Google esh**（<4KB）
- ⚠️ LinCLI 最小化约 ~11.5KB，对于极端资源场景可能仍显奢侈

### 🌐 场景 E：需要网络远程访问 —— 选 pshell（第三方）

如果需要通过 **UDP/TCP/Unix Domain Socket** 远程访问设备 CLI：
- ✅ **pshell** 是此领域的专业方案，支持多会话、RPC 式调用
- ❌ LinCLI 和 letter-shell 均基于 UART/串口抽象，不支持网络传输层

---

## 💻 五、开发者体验对比

### 📝 写第一个命令的代码量

**🎛️ LinCLI**：
```c
struct led_args { bool on; int brightness; };

static int led_handler(void *_args)
{
    struct led_args *args = _args;
    cli_printk("on=%d brightness=%d\r\n", args->on, args->brightness);
    return 0;
}

CLI_COMMAND(led, "led", "Control LED",
    USAGE("led --on -b <brightness>"),
    led_handler, (struct led_args *)0,
    OPTION(0, "on", BOOL, "Turn on", struct led_args, on, 0, "brightness", NULL, false),
    OPTION('b', "brightness", INT, "Brightness", struct led_args, brightness, 0, "on", NULL, false),
    END_OPTIONS);
```

**🎮 letter-shell**：
```c
void led(int on, int brightness)
{
    printf("on=%d brightness=%d\r\n", on, brightness);
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
    led, led, Control LED);
```

> 💡 **结论**：letter-shell 的首个命令更短。但当命令数量超过 10 个、选项关系变得复杂时，LinCLI 的声明式约束会显著减少 handler 内的校验代码。

### 📖 帮用户写命令

**🎛️ LinCLI**：
```bash
lin@linCli> led --help
 command     : led
 description : Control LED
 usage       : led --on -b <brightness>
 option      :
  - , --on               Turn on [depends:brightness]
  -b, --brightness       Brightness [depends:on]
```

**🎮 letter-shell**：
```bash
letter:/$ led[TAB][TAB]
led - Control LED
```

> 💡 **结论**：LinCLI 的帮助信息更丰富，letter-shell 的帮助更简洁。取决于你的用户是谁。

---

## 🧹 六、常见误区澄清

### ⚠️ 误区 1："LinCLI 比 letter-shell 大很多，所以资源受限应该选 letter-shell"

**✅ 事实**：LinCLI 最小化配置（关闭全部可选模块）约 **~11.5KB**，与 letter-shell 核心体积在同一量级。LinCLI 的"大"来自于**高级补全**（~4.7KB）和**变量系统**（~2.3KB）等可选功能，这些可以通过 Kconfig 关闭。

### ⚠️ 误区 2："letter-shell 的函数签名可以替代 LinCLI 的选项解析"

**✅ 事实**：函数签名（`"isc"`）解决的是**参数类型转换**问题，不是**命令语义约束**问题。它无法表达"`--erase` 和 `--write` 互斥"、"`--on` 依赖 `--brightness`"这类关系。这是两种不同层面的能力。

### ⚠️ 误区 3："两个框架可以互相替代"

**✅ 事实**：它们是**正交**的。letter-shell 是"函数导出器"，LinCLI 是"命令行解析器"。有些团队甚至在同一项目中混用：letter-shell 用于开发调试，LinCLI 用于产品化命令。

---

## 🏁 七、总结

| 🎯 你的核心诉求 | 💎 推荐选择 |
|--------------|-----------|
| 快速导出 C 函数用于调试（零学习成本） | 🎮 **letter-shell** |
| 构建面向客户/产线的专业 CLI | 🎛️ **LinCLI** |
| 需要异步非阻塞命令执行 | 🎛️ **LinCLI** |
| 需要选项依赖/互斥/自动帮助 | 🎛️ **LinCLI** |
| 需要网络远程访问 | 🌐 **pshell**（第三方） |
| 资源极端受限（< 8KB） | 🪶 **Google esh** 或 **microrl** |
| 调试复杂命令且最终要产品化 | 🎛️ **LinCLI** |
| 两个都想要 | 🎮🎛️ **letter-shell 调试用 + LinCLI 产品用** |

---

*📝 本文档由 LinCLI 项目维护。如有偏颇之处，欢迎提交 Issue 指正。*
