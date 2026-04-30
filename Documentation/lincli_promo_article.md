# LinCLI：给嵌入式设备装上一扇"对话窗口"

> **v1.1.1 正式发布** —— 让命令注册像定义变量一样简单

---

## 引子：凌晨两点的客户现场电话

你的手机在床头柜上震了起来。屏幕上显示两个字：**"现场"**。

远在五百公里外的工厂车间里，你设计的工控设备正在产线上轰鸣。它封在一个厚厚的金属外壳里，PCB 上的 JTAG 接口被结构件挡得严严实实。客户说，设备运行三天，必定有一次莫名其妙的复位。你让他把日志导出来，但日志像大海——几万行 `printf` 输出里，你根本找不到那个决定命运的瞬间。

"能加几个打印再烧一版吗？"客户问。

你苦笑。这设备跑的是裸机程序，加一个 `printf` 要重新编译、烧录、再跑三天。更糟的是，加了打印改了时序，bug 可能就躲起来了——**海森堡效应在嵌入式世界里真实存在**。

此刻，你多希望能有一扇窗，能直接探进运行中的设备内部：看一眼那个可疑计数器的实时值，改一下 PID 参数试试，手动触发一次 Flash 自检……**不需要调试器，不需要重新编译，一根 USB 转串口线就够了**。

这扇窗，就是 **CLI（Command Line Interface）**。

---

## 一、`printf` 不是银弹——为什么嵌入式需要交互式 CLI

嵌入式开发者有一个错觉：实验室里 JTAG 连着，断点随便打，变量随便看，天下太平。但真正的战场从来不在实验室。

真正的战场是：
- 设备发到客户现场，调试口根本没引出；
- 故障是**偶发的**，跑三天才出现一次，你无法单步跟踪；
- 你想看的那个结构体，发布时根本没加打印；
- 你加了 `printf` 重新烧录，bug 消失了；
- 产线停一分钟损失几千块，没人有时间等你编译烧录十遍。

`printf` 日志是**单向的、被动的**。它像飞机黑匣子——只能回放预设好的内容。而 **CLI 是一个双向的对话窗口**，在设备全速运行的同时，你可以随时发问、随时干预：

```bash
# 查看当前任务状态
lin@linCli> ps

# 读取传感器原始 ADC 值和校准系数
lin@linCli> sensor --raw --calib

# 电机异响？在线改 PID，立刻听效果
lin@linCli> pid --kp 2.5 --ki 0.1

# 网络偶发掉线？手动触发重连诊断
lin@linCli> net --reconnect --verbose

# Flash 疑似坏块？运行自检，看每一块的状态
lin@linCli> flash --test --sector 0 15

# 日志太吵？现场屏蔽 info，只看错误
lin@linCli> level --err
```

**不需要 IDE，不需要调试器，不需要重新编译。一根串口线，一个 SecureCRT，你就能在设备运行时"解剖"它的内脏。**

CLI 不是花哨功能，它是嵌入式产品从"Demo 玩具"走向"工业现场"的必经之路。

但问题是——**在嵌入式里写一个像样的 CLI，太痛苦了。**

---

## 二、如果你手写过一个 CLI，你一定懂这些痛

### 痛 1：注册地狱

你写了一个漂亮的 `motor_ctrl()` 函数，想在串口终端里调用它。于是你打开 `commands.c`，在 `command_table[]` 里加了一行。三个月后，这个表已经膨胀到两百行，命令分散在十几个源文件里。某次重构你改了一个函数名，忘了更新表——**编译通过，运行时报 "unknown command"**，你花了半小时才发现是注册表漏了。

### 痛 2：参数解析的 if-else 森林

为了解析 `led --on -b 80`，你写了三十行代码：

```c
if (strcmp(argv[i], "--on") == 0) {
    on = 1;
} else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--brightness") == 0) {
    if (i + 1 < argc) brightness = atoi(argv[++i]);
}
// ... 还有 --off，还有错误处理，还有类型检查
```

解析代码比业务代码还长。更可怕的是，`atoi` 溢出了怎么办？用户同时输入 `--on --off` 怎么办？缺少亮度参数怎么办？**这些校验逻辑像杂草一样蔓延。**

### 痛 3：约束校验靠人肉

需求文档明明写了："`--on` 和 `--off` 互斥"、"开灯必须带亮度参数"。但代码里全靠你的 `if` 判断。某天新来的同事加了一个 `--strobe` 选项，忘了检查它和 `--off` 的冲突，于是现场出现了"灯在闪却同时执行关闭"的玄学 bug。

### 痛 4：帮助文档与代码"两张皮"

`--help` 的输出是你三个月前手写的字符串。后来加了 `--verbose`、改了 `--level` 的取值范围、把 `--config` 改成了 `--file`——**但帮助文本早就被忘在脑后了。** 用户对着过时的帮助信息抓狂，你在电话里尴尬地解释："啊，那个文档是旧的……"

### 痛 5：内存焦虑

你决定在解析参数时用 `malloc` 动态分配缓冲区。在 Linux 上跑得好好的，移植到 STM32F103 上跑了两周，**堆碎片导致 HardFault**。换静态数组吧，又不知道到底该开多大。256B？万一用户输了个超长命令呢。1KB？RAM 本来就不够花。

### 痛 6：交互体验停留在 90 年代

你终于把 CLI 跑起来了。用户输了一半命令，忘了选项叫什么，狂按 `Tab` 没有任何反应。手抖打错了一个字母，没有历史记录，只能从头再敲一遍。想连续执行 `flash erase && write && verify`？不支持，只能等一条跑完再输下一条。

那一刻，你的嵌入式 CLI 像一台没有自动补全的 DOS 机，而用户刚用惯了 Linux 的 Bash。

### 痛 7：耗时命令卡死系统

Flash 擦写要 2 秒，电机回零要 5 秒。你的 CLI 是轮询阻塞的，命令一执行，**整个串口假死，看门狗差点复位**。你自己拆状态机吧：`entry()` 发指令，`task()` 轮询状态，`exit()` 收尾——为了跑一条命令，你写了一个迷你操作系统。

### 痛 8：`main()` 里的 init 堆积

开机初始化要调十几个函数：`init_uart()`、`init_flash()`、`init_sensor()`、`init_network()`……`main.c` 里排成一排。顺序错了系统起不来，加一个新模块要改 `main.c`，每次合并代码都冲突。

---

这些痛，但凡你中过三枪以上，你就知道：**手写 CLI 的工作量，有时比写业务本身还大。**

但等等——如果这些苦力活，本来就可以交给编译器和链接器呢？

---

## 三、LinCLI 登场：让命令注册像定义变量一样简单

LinCLI 的核心理念就一句话：**你只管描述命令长什么样，剩下的收集、解析、校验、帮助生成，全部交给编译器与链接器。**

它利用 GCC 的 `section` 属性配合自定义链接脚本，在**链接阶段**自动把散落在各个 `.c` 文件中的命令定义汇总成一张连续的表。你不需要中央注册表，不需要在 `main()` 里初始化，甚至不需要头文件里 `extern` 声明。

来看一个真实对比：

### 以前你写的（手写 CLI）

```c
// 1. 定义命令结构体（central registry）
static cmd_t commands[] = {
    {"led", led_handler, led_help},
    // ... 两百行后
};

// 2. 在 main() 里注册
cmd_register(commands, ARRAY_SIZE(commands));

// 3. 参数解析写在 handler 里
int led_handler(int argc, char **argv) {
    int on = 0, off = 0, brightness = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--on") == 0) on = 1;
        else if (...) // 三十行 if-else
    }
    if (on && off) { printf("error\n"); return -1; }
    // ...
}
```

### 现在你用 LinCLI

```c
struct led_args {
    bool on;
    bool off;
    int brightness;
};

static int led_handler(void *_args)
{
    struct led_args *args = _args;
    if (args->on)  cli_printk("LED ON, brightness=%d\r\n", args->brightness);
    if (args->off) cli_printk("LED OFF\r\n");
    return 0;
}

CLI_COMMAND(led, "led", "Control LED", led_handler, (struct led_args *)0,
    OPTION(0, "on",  BOOL, "Turn LED on",  struct led_args, on,  0, "brightness", "off", false),
    OPTION(0, "off", BOOL, "Turn LED off", struct led_args, off, 0, NULL,         "on",  false),
    OPTION('b', "brightness", INT, "Brightness 0-100", struct led_args, brightness, 0, "on", NULL, false),
    END_OPTIONS);
```

就这些。**没有注册表，没有 `main()` 修改，没有解析代码，没有校验代码，没有 help 字符串。** `CLI_COMMAND` 宏自动完成：

1. **推导参数结构体类型**和大小；
2. **生成选项数组**并计算结构体偏移量；
3. **放入 `.cli_commands` 链接段**；
4. 运行时框架自动遍历该段完成收集。

框架在调用 `led_handler` 之前，已经帮你做完了：
- ✅ 自动解析 `--on`、`-b 80` 并填入结构体对应字段；
- ✅ 检查 `--on` 和 `--off` 是否同时出现（互斥）；
- ✅ 检查 `--on` 是否带了 `--brightness`（依赖）；
- ✅ 检查重复选项；
- ✅ 未指定字段自动清零（`memset` 保证）。

你的 `led_handler` 里**只需要写业务逻辑**。

---

## 四、十分钟上手：从 LED 命令看全貌

把上面那段代码编译烧录，打开串口终端：

### 正常执行

```bash
lin@linCli> led --on -b 80
LED ON, brightness=80
lin@linCli>
```

### 缺少依赖（只开灯不带亮度）

```bash
lin@linCli> led --on
[ERR] option - /--on depends on brightness but not provided
[ERR] command parsing failed: led
```

### 互斥冲突（同时开和关）

```bash
lin@linCli> led --on --off
[ERR] option - /--on conflicts with off, cannot be used together
[ERR] command parsing failed: led
```

### 自动生成帮助

```bash
lin@linCli> led --help
 command     : led
 description : Control LED
 option      :
      - , --off              Turn LED off
      - , --on               Turn LED on [depends:brightness] [conflicts:off]
      -b, --brightness       Brightness 0-100 [depends:on]
```

注意：**帮助文本里的 `[depends:brightness]` 和 `[conflicts:off]` 是框架自动从 `OPTION` 宏里提取的**。你不需要维护任何字符串。

---

## 五、不止于基础：产品级 CLI 该有的样子

### 异步非阻塞命令

Flash 擦写 2 秒？电机回零 5 秒？用 `CLI_COMMAND_ASYNC` 注册三阶段命令：

```c
CLI_COMMAND_ASYNC(upgrade, "upgrade", "OTA firmware upgrade",
    upgrade_entry, upgrade_task, upgrade_exit,
    (struct upgrade_args *)0, ...);
```

- `entry()`：擦除 Flash，只执行一次；
- `task()`：每次调度器轮询写入一块数据，返回 `CLI_CONTINUE` 继续；
- `exit()`：校验 CRC，收尾。

**CLI 主循环始终响应，看门狗安心睡觉。**

### 现代交互体验

- **命令历史**：`↑` `↓` 翻页，自动去重；
- **Tab 补全**：命令名前缀补全、选项名补全，歧义前缀自动填充 LCP；
- **候选列表高亮选择（v1.1.1 新增）**：按 `Tab` 弹出候选列表后，再次按 `Tab` 进入高亮循环模式，通过方向键或 `Tab` 键切换反白候选项，选中后自动替换命令行，无需手敲完整内容；
- **命令链 `&&`**：`flash erase && write && verify`，前一条失败后续自动停止；
- **命令别名**：`CMD_ALIAS(echo, "ts --msg")` 把复杂命令简写。

### 尾行模式

后台代码通过 `pr_err()` / `cli_printk()` 输出日志时，如果用户正处于命令输入状态，框架会自动清行、输出日志、再**完整重绘命令提示符和已输入内容**（包括 Tab 补全的候选列表），光标位置自动恢复。

你不需要做任何配置。日志来了，输入行不会被打烂。

### 日志级别过滤

现场噪音太大？一键屏蔽：

```bash
lin@linCli> level --err    # 只看错误及以上
lin@linCli> level          # 恢复全级别
```

### 开机自动执行

上电后自动跑自检序列？定义一个弱符号数组：

```c
const char * const cli_auto_cmds[] = {
    "sensor --calib",
    "flash --test",
    "net --check",
};
```

调度器初始化完毕后自动顺序执行。某条失败则停止并进入提示符。不需要开关宏，不定义就自动跳过。

### 开机初始化自动收集

告别 `main()` 里的 init 堆积。用 `_EXPORT_INIT_SYMBOL` 宏把初始化函数放入 `.my_init_d` 段，支持按优先级排序执行。加新模块？**不需要碰 `main.c`。**

---

## 六、从 PC 仿真到 MCU 量产：一条串口线的距离

LinCLI 的源码结构清晰分层：

- `cli/` —— 命令解析、命令行编辑、终端交互
- `lib/` —— 状态机、红黑树、内存池、错误码（纯逻辑，无平台相关）
- `init/` —— PC 模拟入口（`main.c` + `scheduler.c`）
- `tests/` —— 内置测试命令

PC 端用 `pthread` 双线程模拟输入/调度，裸终端直接跑。想切到 STM32？

**只需要做四件事**：
1. 把 `cli_in_push()` 放到 UART 中断接收里；
2. 实现 `cli_putc()` 发单字符；
3. 实现 `cli_enter_critical()` / `cli_exit_critical()`（关中断/开中断或者互斥锁）。
4. 在main函数或者rtos任务中执行cli任务

框架自带 STM32F103 + Keil MDK 移植示例工程，GCC 和 MDK 链接脚本模板均已配好。

---

## 结语：没有调试器的深夜，CLI 是你的底气

嵌入式开发最终都要面对一个残酷事实：**实验室里的调试器，到不了客户的机柜里。** 当设备封死在外壳中、当故障三天才出现一次、当产线每一秒都在烧钱——你需要一种不依赖编译烧录、不依赖 IDE、甚至不依赖 JTAG 的调试与控制能力。

CLI 就是那扇窗。而 LinCLI 想做的，就是让开这扇窗的成本，**趋近于零**。

你不需要维护注册表，不需要写解析器，不需要校验 `if-else`，不需要手动同步文档。你只需要定义一个结构体，写一个 handler，用一行宏注册——剩下的，交给链接器。

**LinCLI v1.1.1 现已发布。** 如果你正在为一个嵌入式项目手写 CLI，或者在 printf 的海洋里捞一个三天才出现一次的 bug，不妨试试给它装上一扇对话窗口。

```bash
# 获取源码
git clone https://github.com/BunnyDeny/LinCLI.git

# linux PC 端直接体验
cd LinCLI && mkdir build && cd build
cmake .. && make -j
./bin/a.out
```

---

**LinCLI** —— 面向嵌入式/MCU 的 C 语言命令行交互框架。让命令注册像定义变量一样简单。

*License: GPL v3*
