# 环境搭建与编译运行

本项目目前已在以下平台测试通过：

- **PC 模拟环境**：**Debian / Ubuntu** Linux（GCC + CMake）
- **嵌入式真实环境**：**Keil5 MDK** + **STM32CubeMX**（目标芯片 **STM32F103C8T6**）

> 项目 `example_project/` 目录下提供了完整的 Keil5 示例工程，可直接打开使用。

## 依赖

- **GCC** (`gcc`)
- **Make** (`make`)
- **CMake** (`cmake`，建议 >= 3.10)
- **GNU ld** (`ld`)

## 安装构建工具

```bash
sudo apt update
sudo apt install build-essential cmake
```

安装完成后，验证版本：

```bash
cmake --version
gcc --version
ld --version
```

> **⚠️ 关于链接器版本**
>
> 项目自带的 `default.ld` 是在较高版本链接器（>= 2.38）下导出的默认脚本。如果你系统的 `ld` 版本较低，直接使用该文件可能导致编译失败。此时只需按下文「低版本链接器处理」的步骤，生成你当前系统的默认链接脚本即可。

---

## 低版本链接器处理

如果 `ld --version` 显示版本低于 2.38，请按以下步骤生成适配你当前系统的链接脚本：

### 1. 生成系统默认链接脚本

```bash
ld --verbose > default.ld
```

这会将当前系统链接器的默认链接脚本内容写入 `default.ld` 文件。

### 2. 清理头部和尾部的无关内容

`ld --verbose` 的输出会在**文件开头**附加版本信息和仿真列表，在**文件结尾**附加一行 `==================================================`，这些都不是合法的链接脚本语法，必须删除。

**需要删除的内容示例：**

- **文件开头**（从第一行到第一个 `==================================================`，包含该行）：
  ```
  GNU ld (GNU Binutils for Debian) 2.44
    支持的仿真：
     elf_x86_64
     ...
  使用内部链接脚本：
  ==================================================
  ```

- **文件结尾**（从最后一个 `==================================================` 到文件末尾，包含该行）：
  ```
  }


  ==================================================
  ```

删除后，文件应以 `/* Script for ... */` 之类的注释开头，以 `}` 结尾。

### 3. 引入自定义段收集脚本

在 `SECTIONS { ... }` 块内，找到 `.data` 段结束后的位置（通常是 `_edata = .; PROVIDE (edata = .);` 或 `.data1` 之后），在 `.bss` 段（或 `__bss_start = .`）开始**之前**插入一行：

```ld
INCLUDE cli.ld
```

插入后的效果应类似：

```ld
  .data           :
  {
    *(.data .data.* .gnu.linkonce.d.*)
    SORT(CONSTRUCTORS)
  }
  .data1          : { *(.data1) }
  _edata = .; PROVIDE (edata = .);
  . = .;
  INCLUDE cli.ld
  __bss_start = .;
  .bss            :
  {
    *(.dynbss)
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)
    ...
  }
```

### 4. 替换项目文件

将处理好的 `default.ld` 覆盖到项目根目录：

```bash
cp default.ld /path/to/LinCLI/default.ld
```

完成后即可正常编译。

---

## 编译与运行

项目根目录提供了 `Makefile` 封装，你可以直接用以下命令：

| 命令 | 作用 |
|------|------|
| `make` | 编译项目（自动创建 `build/` 目录、调用 CMake、生成 `build/bin/a.out`） |
| `make run` | 先编译（如需要），然后直接运行 `build/bin/a.out` |
| `make clean` | 删除 `build/` 目录，清理所有编译产物 |
| `make ag` | 等价于 `make clean && make`，完整重建 |

进入项目目录后，最简单的上手方式是：

```bash
cd /path/to/LinCLI
make run
```

程序启动后，终端进入 **raw 模式**（禁用行缓冲和回显），每按一个键都会立即被框架处理。你可以直接输入测试命令进行交互。

> **退出方式**：目前测试用例中没有内建 `exit` 命令，直接按 `Ctrl+C` 即可终止程序。程序退出前会自动恢复终端规范模式。
