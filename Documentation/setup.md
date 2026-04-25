# 环境搭建与编译运行


- **GCC** (`gcc`)
- **Make** (`make`)
- **CMake** (`cmake`，建议 >= 3.10)

### Ubuntu / Debian 安装

```bash
sudo apt update
sudo apt install build-essential cmake
```

### CentOS / Fedora / openEuler 安装

```bash
sudo yum install gcc make cmake
# 或
sudo dnf install gcc make cmake
```

### macOS 安装

```bash
xcode-select --install
brew install cmake
```

安装完成后，验证版本：

```bash
cmake --version
gcc --version
make --version
```

---

## 编译与运行

### 1. 克隆并进入项目目录

```bash
cd /path/to/cli
```

### 2. 创建构建目录并编译

```bash
mkdir build && cd build
cmake ..
make
```

编译成功后，可执行文件位于 `build/bin/a.out`。

### 3. 运行 PC 模拟程序

```bash
./bin/a.out
```

程序启动后，终端进入 **raw 模式**（禁用行缓冲和回显），每按一个键都会立即被框架处理。你可以直接输入测试命令进行交互。

> **退出方式**：目前测试用例中没有内建 `exit` 命令，直接按 `Ctrl+C` 即可终止程序。程序退出前会自动恢复终端规范模式。

---

## 项目结构与核心机制
