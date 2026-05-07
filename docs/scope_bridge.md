# 🎯 实时数据示波器：scope 命令与 CSV 桥接

LinCLI 不仅是一个命令行框架，还能让你的 **MCU / 仿真程序秒变实时数据源**。通过内置的 `scope` 测试命令，配合 `tools/lincli_csv_bridge.py` 桥接脚本，你可以在 PC 端实时观测曲线、记录 CSV，而无需编写任何上位机代码。

> 🚀 **核心价值**：一行命令启动，零配置上上位机，数据自动落盘。

---

## 📡 能做什么

| 能力 | 说明 |
|---|---|
| 📈 **实时曲线** | 运行 `scope` 后，PC 端自动弹出 matplotlib 实时波形窗口 |
| 📁 **CSV 记录** | 所有数据自动写入 CSV，可用 Excel / Python / MATLAB 二次分析 |
| 🔄 **反复启动** | 每次重新运行 `scope` 都会**清空旧 CSV、重建绘图窗口**，无需重启脚本 |
| 🔌 **双模式支持** | 既支持 PC 仿真（PTY 子进程），也支持真实串口（`/dev/ttyUSB0` 等） |

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

> 💡 **此时不会弹出任何窗口**，因为绘图窗口是**懒加载**的——只有收到 MCU 的绘图指令后才会创建。

### 2️⃣ 启动 scope

在 CLI 提示符下输入：

```bash
lin@linCli> scope
```

**神奇的事情发生了：**

1. 🖥️ PC 端立刻弹出一个 matplotlib 实时绘图窗口
2. 📝 `output.csv` 被自动创建（或清空），开始记录数据
3. 📉 终端上同一行原地刷新数值（尾行模式），不会破坏你的命令输入

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
- **再次启动 scope**：重新输入 `scope`，脚本会**清空旧 CSV** 并**重建新绘图窗口**

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

`scope` 命令被设计为**异步命令**（`CLI_COMMAND_ASYNC`），它由三个阶段组成：

- `scope_entry`：命令启动时执行一次
- `scope_task`：被调度器周期性调用，输出实时数据
- `scope_exit`：本命令未使用

在 `scope_entry` 中，MCU 会发送一条特殊的**握手信号**：

```c
cli_printk("\r$SCOPE_START\r\n");
```

Python 桥接脚本在数据流中识别到 `$SCOPE_START` 后，立即执行：

1. 🗑️ `csv.reset()` —— 清空 CSV 文件（避免旧数据堆积）
2. 🖥️ `plotter.reset()` —— 关闭旧窗口、清空缓存、创建新窗口

这就是每次运行 `scope` 都能获得「全新」体验的原因。

### 2. 数据输出方式：直接打印 + `\r` 单行刷新

在 `scope_task` 中，MCU 以固定周期输出数据：

```c
cli_printk("\r %d,%d,%d,%d", timestamp, theta, speed, iq);
```

注意这里的 **`\r`**（回车，不换行）。它的作用是：

- 📌 **在终端上**：光标回到行首，同一行原地刷新数值，看起来像个实时计数器
- 📌 **在 Python 脚本中**：`\r` 被用作**数据帧分隔符**。脚本按 `\r` 分割数据流，提取每一帧并解析成 CSV

这种设计巧妙地复用了「尾行模式打印」机制（详见 [inline_print.md](inline_print.md)）：后台数据刷新与前台命令输入互不干扰。

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

### 修改波形或添加新通道

`scope` 命令的源码位于 `tests/commands/test_scope.c`，你可以直接修改：

- 📝 替换 `sin_table[]` 或 `scope_compute()` 中的算法，输出你自己的传感器数据
- 📝 增加更多字段（如 `voltage`、`current`），只需在 `cli_printk` 格式字符串中追加，Python 脚本会自动识别为新的 `ch4`、`ch5`...

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

`scope` 命令是 LinCLI **异步非阻塞命令** 与 **尾行模式打印** 两个核心特性的最佳结合示例：

- ✅ 用 `CLI_COMMAND_ASYNC` 把周期性采样任务拆成 `entry/task` 两阶段，不阻塞 CLI 主循环
- ✅ 用 `\r` 单行刷新实现"示波器式"实时显示，同时被 Python 脚本捕获为结构化数据
- ✅ 用 `$SCOPE_START` 握手信号实现 MCU 与 PC 上位机的简单协议同步

如果你想为自己的项目添加类似的数据观测功能，复制 `test_scope.c` 并修改数据生成逻辑即可，**无需改动 Python 脚本的一行代码**。
