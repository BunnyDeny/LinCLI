# 🎯 实时数据示波器：通用 CSV 桥接与实时绘图

LinCLI 不仅是一个命令行框架，还提供了一套**零配置上上位机的实时数据桥接协议**。只要你的命令在输出中插入 **`$SCOPE_START`** 握手信号，并以 **`\r`**（回车不换行）刷新 CSV 数据，PC 端的 `tools/lincli_csv_bridge.py` 脚本就会自动弹出实时曲线、记录 CSV。

框架内置的 **`scope`** 命令是一个开箱即用的示例——它输出正弦波与三角波供你体验。但**任何自定义命令**都可以复用同一套机制观测自己的传感器或控制数据。

> 🚀 **核心价值**：一行命令启动，零配置上上位机，数据自动落盘。`scope` 只是示例，协议才是能力。

> 🚀 **核心价值**：一行命令启动，零配置上上位机，数据自动落盘。

---

## 📡 能做什么

| 能力 | 说明 |
|---|---|
| 📈 **实时曲线** | 任何命令发送 `$SCOPE_START` 后，PC 端自动弹出 matplotlib 实时波形窗口 |
| 📁 **CSV 记录** | 所有 `\r` 刷新的数据自动写入 CSV，可用 Excel / Python / MATLAB 二次分析 |
| 🔄 **反复启动** | 每次重新发送 `$SCOPE_START` 都会**清空旧 CSV、重建绘图窗口**，无需重启脚本 |
| 🔌 **双模式支持** | 既支持 PC 仿真（PTY 子进程），也支持真实串口（`/dev/ttyUSB0` 等） |
| 🧩 **命令无关** | `scope` 只是示例；你可以为自己的电机、传感器、PID 命令添加同样的绘图能力 |

---

## 🛠️ 安装依赖

脚本依赖 `pyserial`（串口通信）和 `matplotlib`（实时绘图）。在 Ubuntu/Debian 上执行：

```bash
sudo apt update && sudo apt install python3-serial python3-matplotlib -y
```

> 📝 **提示**：如果你只使用 **子进程模式**（PC 仿真），`pyserial` 不是必需的；但建议一并安装，方便后续切换到真实硬件。

---

## 🚀 快速开始（PC 仿真模式）

这是最推荐的入门方式，无需任何硬件。

### 1️⃣ 编译并启动桥接脚本

```bash
cd LinCLI
make
python3 tools/lincli_csv_bridge.py --exec ./build/bin/a.out output.csv --plot
```

终端会进入正常的 CLI 交互界面：

```
lin@linCli>
```

> 💡 **此时不会弹出任何窗口**，因为绘图窗口是**懒加载**的——只有收到 MCU 发来的 `$SCOPE_START` 后才会创建。

### 2️⃣ 启动 scope（体验示例）

在 CLI 提示符下输入：

```bash
lin@linCli> scope
```

**神奇的事情发生了：**

1. 🖥️ PC 端立刻弹出一个 matplotlib 实时绘图窗口
2. 📝 `output.csv` 被自动创建（或清空），开始记录数据
3. 📉 终端上同一行原地刷新数值（尾行模式），不会破坏你的命令输入

> 📝 `scope` 是框架自带的演示命令，用于展示桥接协议的效果。下文会介绍如何把这套能力移植到你自己的命令中。

### 3️⃣ 观察数据

绘图窗口默认显示三条曲线：

| 通道 | 含义 | 波形 |
|---|---|---|
| `ch1` (theta) | 正弦波 | 64 点查表生成，幅度 0 ~ 6.283 |
| `ch2` (speed) | 三角波 | 1000 ~ 2000 线性往返 |
| `ch3` (iq) | 三角波 | 500 ~ 3500 线性往返 |

X 轴为时间（秒），数据来自 `scope` 命令第一列输出的毫秒时间戳。

### 4️⃣ 关闭与重启

- **关闭绘图窗口**：直接点窗口的 ❌，窗口会关闭且**不会自动重新弹出**
- **停止 scope**：在终端内按 `Ctrl+D`（发送 ASCII 4），scope 任务退出，回到提示符
- **再次启动 scope**：重新输入 `scope`，命令会再次发送 `$SCOPE_START`，脚本随即**清空旧 CSV** 并**重建新绘图窗口**

---

## 🔌 真实串口模式（连接硬件）

如果你已经把 LinCLI 移植到了真实 MCU，只需把桥接对象的 `--exec` 换成串口参数：

```bash
python3 tools/lincli_csv_bridge.py /dev/ttyUSB0 115200 output.csv --plot
```

参数说明：

| 参数 | 含义 |
|---|---|
| `/dev/ttyUSB0` | 串口设备（Windows 下可能是 `COM3`） |
| `115200` | 波特率 |
| `output.csv` | 输出 CSV 文件名 |
| `--plot` | 启用实时 matplotlib 绘图（可选） |

> 📝 **提示**：串口模式下，脚本同样会透传你的键盘输入到 MCU，你可以像普通串口助手一样在终端里输入任何命令。只有以 `\r`（回车不换行）刷新的数据行会被捕获为 CSV。

---

## 🔧 工作原理

### 1. 触发绘图指令：`$SCOPE_START`

这套桥接协议的**唯一约定**是：你的命令在准备输出实时数据前，发送一行特殊的握手信号：

```c
cli_printk("\r$SCOPE_START\r\n");
```

Python 桥接脚本在数据流中识别到 `$SCOPE_START` 后，立即执行：

1. 🗑️ `csv.reset()` —— 清空 CSV 文件（避免旧数据堆积）
2. 🖥️ `plotter.reset()` —— 关闭旧窗口、清空缓存、创建新窗口

**任何命令**——无论是 `scope`、`motor`、`sensor` 还是你自己定义的 `pid_tune`——只要发送这行握手，就能获得同样的「全新绘图会话」体验。

### 2. 数据输出方式：直接打印 + `\r` 单行刷新

握手完成后，你的命令以固定周期输出数据即可：

```c
cli_printk("\r %d,%d,%d,%d", timestamp, theta, speed, iq);
```

注意这里的 **`\r`**（回车，不换行）。它的作用是：

- 📌 **在终端上**：光标回到行首，同一行原地刷新数值，看起来像个实时计数器
- 📌 **在 Python 脚本中**：`\r` 被用作**数据帧分隔符**。脚本按 `\r` 分割数据流，提取每一帧并解析成 CSV

这种设计巧妙地复用了「尾行模式打印」机制（详见 [inline_print.md](inline_print.md)）：后台数据刷新与前台命令输入互不干扰。**你不需要关心 Python 脚本怎么解析，只需要在 C 代码里按上面的格式打印数据。**

### 3. 数据格式

每帧输出四个整数字段（均为 ×1000 缩放后的定点数，Python 端自动处理）：

```
timestamp, theta, speed, iq
```

示例：

```
0,3141,1000,500
10,3449,1010,545
20,3754,1020,590
```

Python 脚本把第一列当作毫秒时间戳（除以 1000 转为秒），后续列映射为 `ch1`、`ch2`、`ch3` 曲线。

---

## 💡 进阶提示

### 为自己的命令添加实时绘图

**`scope` 只是一个示例**。把这套能力移植到你自己的命令（如 `motor`、`sensor`、`pid_tune`）只需要三步：

1. 📝 在命令的 `entry` 函数中发送握手：
   ```c
   cli_printk("\r$SCOPE_START\r\n");
   ```
2. 📝 在 `task` 函数中按 `\r` 前缀输出 CSV 数据：
   ```c
   cli_printk("\r %d,%d,%d", timestamp, current, voltage);
   ```
3. 📝 无需修改 Python 脚本的一行代码。

`tests/commands/test_scope.c` 是最佳参考模板。你可以直接复制它的骨架，把 `scope_compute()` 里的波形生成替换成你的真实业务数据（ADC 采样、编码器读数、PID 输出等），增加或减少字段数量均可。

### 无图形界面环境

如果你在 WSL / 远程服务器 / 无显示器环境中运行，可以去掉 `--plot`：

```bash
python3 tools/lincli_csv_bridge.py --exec ./build/bin/a.out output.csv
```

此时只记录 CSV，不产生任何 GUI 窗口。后续可用 Excel 或 Python 离线绘图分析。

### CSV 文件查看

```bash
# 实时跟踪最后几行
tail -f output.csv

# 用 pandas 快速绘图
python3 -c "import pandas as pd; df = pd.read_csv('output.csv'); df.plot()"
```

---

## 🎓 学习价值

这套桥接协议是 LinCLI **异步非阻塞命令** 与 **尾行模式打印** 两个核心特性的最佳结合示例：

- ✅ 用 `CLI_COMMAND_ASYNC` 把周期性采样任务拆成 `entry/task` 两阶段，不阻塞 CLI 主循环
- ✅ 用 `\r` 单行刷新实现"示波器式"实时显示，同时被 Python 脚本捕获为结构化数据
- ✅ 用 `$SCOPE_START` 握手信号实现 MCU 与 PC 上位机的简单协议同步

`scope` 命令只是展示这套协议的标准示例。如果你想为自己的项目添加实时数据观测功能，复制 `test_scope.c` 的骨架并修改数据生成逻辑即可，**无需改动 Python 脚本的一行代码**。
