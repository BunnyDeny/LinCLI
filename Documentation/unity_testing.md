# 🧪 Unity 单元测试框架与 LinCLI 测试体系

> 📝 本文档面向**第一次接触 Unity 测试框架**的开发者，从原理到实践，完整介绍 LinCLI 项目中单元测试的写法、运行方式和判定标准。

---

## 📌 目录

1. [Unity 是什么](#unity-是什么)
2. [Unity 的工作原理](#unity-的工作原理)
3. [LinCLI 的两层测试体系](#lincli-的两层测试体系)
4. [如何编写一个 Unity 测试](#如何编写一个-unity-测试)
5. [常用断言宏速查](#常用断言宏速查)
6. [哪些功能需要测试](#哪些功能需要测试)
7. [测试成功的依据是什么](#测试成功的依据是什么)
8. [编译与运行测试](#编译与运行测试)
9. [为项目新增测试模块](#为项目新增测试模块)

---

## 🎯 Unity 是什么

**Unity** 是由 [ThrowTheSwitch.org](https://throwtheswitch.org) 开发的、专为嵌入式 C 语言设计的**轻量级单元测试框架**。它的核心定位是：**小到可以跑在单片机上，简单到只需要 3 个文件就能工作**。

与 Google Test、CppUnit 等大型框架不同，Unity 具有以下特点：

| 特性 | Unity | 传统大型框架 |
|------|-------|-------------|
| 🪶 **体积** | 仅 3 个源文件（~280 KB 源码） | 通常需要链接库或安装包 |
| 🧠 **动态内存** | **零依赖**，不调用 `malloc` | 多数依赖动态分配 |
| 🔧 **可移植性** | 纯 C99，无平台依赖 | 通常依赖 POSIX / C++ STL |
| 🎓 **学习曲线** | 15 分钟即可上手写第一个测试 | 需要了解复杂的 fixtures、参数化测试等概念 |

> 💡 **一句话总结**：Unity 就是 C 语言世界的"极简测试器"——你只管写 `TEST_ASSERT_EQUAL(expected, actual)`，它帮你统计谁通过了、谁失败了。

---

## ⚙️ Unity 的工作原理

Unity 的工作方式可以概括为 **"三段式 + 断言宏"**：

```
┌─────────────────┐
│   UNITY_BEGIN() │  ← 初始化：清零计数器，记录测试套件名
└────────┬────────┘
         │
    ┌────▼────┐
    │ setUp() │  ← 每个测试前的准备工作（可选）
    └────┬────┘
         │
    ┌────▼────┐
    │ 测试函数 │  ← 你写的 test_xxx，内部调用各种 TEST_ASSERT_*
    └────┬────┘
         │
    ┌────▼────┐
    │tearDown()│  ← 每个测试后的清理工作（可选）
    └────┬────┘
         │
    ┌────▼────┐
    │ RUN_TEST │  ← 注册并执行下一个测试
    └────┬────┘
         │
┌────────▼────────┐
│   UNITY_END()   │  ← 收尾：打印报告，返回失败总数
└─────────────────┘
```

### 🔹 1. 断言宏的本质

你在测试里写的：

```c
TEST_ASSERT_EQUAL(42, answer);
```

背后展开后大致是：

```c
if ((42) != (answer)) {
    Unity.CurrentTestFailed = 1;
    UnityTestResultsFailBegin(__LINE__);
    UnityPrintExpectedAndActual(42, answer);
    return;  /* 提前结束当前测试函数，但不会终止程序 */
}
```

**关键点**：
- ✅ 断言**失败时只会提前 return 当前测试函数**，不会让整个程序崩溃
- ✅ 后续的 `RUN_TEST(...)` 会继续执行
- ✅ 失败信息会记录**行号**（`__LINE__`），方便你定位问题

### 🔹 2. 为什么没有 main 也能跑？

Unity 本身**不提供 `main()`**。你需要自己写一个 `main()`，在里面按顺序调用 `UNITY_BEGIN()`、`RUN_TEST(...)`、`UNITY_END()`。这种设计让 Unity 可以无缝嵌入任何嵌入式项目的启动流程中——你甚至可以在 MCU 的 `main()` 里直接跑测试，无需操作系统支持。

### 🔹 3. 不需要动态内存

Unity 内部维护测试状态用的全部是**静态变量**和**函数参数**：

```c
/* unity_internals.h 中的核心状态结构（节选） */
struct UNITY_STORAGE_T {
    const char* TestFile;
    const char* CurrentTestName;
    UNITY_LINE_TYPE CurrentTestLineNumber;
    UNITY_COUNTER_TYPE NumberOfTests;
    UNITY_COUNTER_TYPE TestFailures;
    /* ... */
};
```

这意味着即使你的目标平台**完全没有堆内存**（裸机 MCU），Unity 依然可以正常工作。

---

## 🏗️ LinCLI 的两层测试体系

LinCLI 目前有两套互补的测试机制，分别解决不同层面的验证需求：

| 层级 | 类型 | 位置 | 构建目标 | 验证对象 | 运行方式 |
|------|------|------|----------|----------|----------|
| 1️⃣ | **单元测试**（Unity） | `tests/unit/*.c` | `unit_tests` | `lib/` 下的纯工具函数 | 自动、无交互 |
| 2️⃣ | **集成测试/示例** | `tests/*.c` | `a.out` | 完整 CLI 交互功能 | 人工终端输入 |

### 💡 两者的区别

**单元测试（Unity）**——验证"函数对不对"：
- 输入 `cli_atoi("123", ...)`，断言返回值是 `0`，输出值是 `123`
- 输入 `cli_strlen("hello")`，断言返回值是 `5`
- 不需要启动 CLI 状态机、不需要键盘输入、不需要终端
- **全部由代码自动完成，CI 可直接运行**

**集成测试（原有 tests/）**——验证"命令能不能用"：
- 在终端输入 `tb -v`，看是否打印 `BOOL test executed!`
- 测试 Tab 补全、历史记录、环境变量替换等**系统级行为**
- 需要人工在终端里敲命令、用眼睛看输出
- **适合作为功能演示和手工回归验证，但无法自动判定通过/失败**

> 📝 **设计哲学**：单元测试负责守住"底层函数不出错"的底线；集成测试负责展示"上层功能怎么用"。两者缺一不可。

---

## ✏️ 如何编写一个 Unity 测试

下面以 LinCLI 项目中的 `tests/unit/test_unit_atoi.c` 为例，逐步拆解一个完整测试文件的构成。

### 第 1 步：包含头文件

```c
#include "unity.h"      /* Unity 框架的核心头文件 */
#include "cli_atoi.h"   /* 被测函数的声明 */
```

> 📝 `unity.h` 已经通过 `tests/CMakeLists.txt` 的 `target_include_directories` 配置好了搜索路径，你不需要写相对路径。

### 第 2 步：提供 setUp / tearDown（可选）

```c
void setUp(void)
{
    /* 每个测试函数执行前，Unity 会自动调用这里 */
    /* 例如：重置全局状态、初始化内存池等 */
}

void tearDown(void)
{
    /* 每个测试函数执行后，Unity 会自动调用这里 */
    /* 例如：释放内存、恢复 mock 对象等 */
}
```

**注意**：
- 这两个函数是**全局唯一**的。如果多个 `.c` 文件链接到同一个可执行文件，只能在一处定义它们。
- 在 LinCLI 项目中，我们统一在 `test_runners.c` 中定义空的 `setUp()` / `tearDown()`，各测试文件不再重复定义。
- 如果你的测试组确实需要特殊的准备/清理逻辑，可以在测试函数内部手动调用辅助函数，而不依赖全局 setUp/tearDown。

### 第 3 步：编写测试函数

```c
void test_cli_atoi_positive(void)
{
    int val;
    char *end;
    /* 调用被测函数 */
    TEST_ASSERT_EQUAL(0, cli_atoi("123", &val, &end));
    /* 验证输出参数 */
    TEST_ASSERT_EQUAL(123, val);
    /* 验证 endptr 指向的位置 */
    TEST_ASSERT_EQUAL_PTR("123" + 3, end);
}
```

**规则**：
- 函数名以 `test_` 开头（约定，非强制）
- 返回类型必须是 `void`
- 不接受任何参数
- 内部使用 `TEST_ASSERT_xxx` 宏来判定结果

### 第 4 步：在 runner 中注册

测试函数写好后，还需要在 `test_runners.c` 中**显式注册**，否则 Unity 不会执行它：

```c
/* 1. 声明外部测试函数 */
extern void test_cli_atoi_positive(void);

/* 2. 在 main() 或分组函数中用 RUN_TEST 注册 */
static void run_atoi_tests(void)
{
    RUN_TEST(test_cli_atoi_positive);
    RUN_TEST(test_cli_atoi_negative);
    /* ... */
}

int main(void)
{
    UNITY_BEGIN();
    run_atoi_tests();
    /* run_string_tests(); ... */
    return UNITY_END();
}
```

> 📝 `RUN_TEST(func)` 的本质是：`UnityDefaultTestRun(func, #func, __LINE__)`。它会先调用 `setUp()`，再执行 `func()`，再调用 `tearDown()`，最后根据 `func()` 内部是否有断言失败来记录结果。

---

## 📋 常用断言宏速查

Unity 提供了丰富的断言宏，覆盖整数、浮点、字符串、指针、内存等多种类型。

### 基础断言

| 宏 | 用途 | 示例 |
|----|------|------|
| `TEST_ASSERT_TRUE(cond)` | 断言条件为真 | `TEST_ASSERT_TRUE(len > 0)` |
| `TEST_ASSERT_FALSE(cond)` | 断言条件为假 | `TEST_ASSERT_FALSE(ptr == NULL)` |
| `TEST_ASSERT_NULL(ptr)` | 断言指针为 NULL | `TEST_ASSERT_NULL(p)` |
| `TEST_ASSERT_NOT_NULL(ptr)` | 断言指针非 NULL | `TEST_ASSERT_NOT_NULL(p)` |

### 整数比较

| 宏 | 用途 | 示例 |
|----|------|------|
| `TEST_ASSERT_EQUAL(expected, actual)` | 通用相等（自动推断类型） | `TEST_ASSERT_EQUAL(42, val)` |
| `TEST_ASSERT_EQUAL_INT(e, a)` | int 类型相等 | `TEST_ASSERT_EQUAL_INT(0, status)` |
| `TEST_ASSERT_EQUAL_HEX8(e, a)` | 8 位十六进制相等 | `TEST_ASSERT_EQUAL_HEX8(0xFF, reg)` |

### 字符串与内存

| 宏 | 用途 | 示例 |
|----|------|------|
| `TEST_ASSERT_EQUAL_STRING(e, a)` | 字符串相等（含 `\0`） | `TEST_ASSERT_EQUAL_STRING("ok", buf)` |
| `TEST_ASSERT_EQUAL_MEMORY(e, a, len)` | 内存块逐字节相等 | `TEST_ASSERT_EQUAL_MEMORY("abc", buf, 3)` |

### 浮点数

| 宏 | 用途 | 示例 |
|----|------|------|
| `TEST_ASSERT_FLOAT_WITHIN(d, e, a)` | 浮点值在误差范围内 | `TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14f, val)` |

### 指针

| 宏 | 用途 | 示例 |
|----|------|------|
| `TEST_ASSERT_EQUAL_PTR(e, a)` | 指针地址相等 | `TEST_ASSERT_EQUAL_PTR(p1, p2)` |

> ⚠️ **常见陷阱**：`TEST_ASSERT_EQUAL_PTR("abc"[3], end)` 是**错误**的！ `"abc"[3]` 是 `char` 值（`0x00`），会被当作整数指针 `0x0`。正确写法是 `TEST_ASSERT_EQUAL_PTR("abc" + 3, end)`。

---

## 🎯 哪些功能需要测试

单元测试的价值在于**验证不可再分的最小逻辑单元**。在 LinCLI 项目中，以下类型的函数最适合用 Unity 覆盖：

### ✅ 强烈推荐测试

| 模块 | 测试重点 | 已有测试文件 |
|------|----------|-------------|
| `lib/cli_string.c` | 字符串/内存操作：边界、空串、重叠拷贝 | `test_unit_string.c` |
| `lib/cli_atoi.c` | 正数、负数、零、无前导数字、空格前缀 | `test_unit_atoi.c` |
| `lib/cli_float.c` | 浮点解析与格式化：正负值、小数、精度截断 | `test_unit_float.c` |
| `lib/cli_mpool.c` | 分配/释放配对、池耗尽、非法指针释放安全 | `test_unit_mpool.c` |
| `lib/tVector.c` | FIFO 的 push/pop、空/满边界、越界访问 | `test_unit_vector.c` |

### ✅ 已补充测试

| 模块 | 测试重点 | 测试文件 |
|------|----------|----------|
| `lib/cli_vsnprintf.c` | `%d`/`%s`/`%c`/`%u`/`%%` 格式化、宽度对齐、缓冲区截断、NULL 字符串 | `test_unit_vsnprintf.c` |
| `lib/stateM.c` | 状态机初始化、状态切换、同状态切换、非法状态 ID | `test_unit_statem.c` |
| `lib/cli_errno.c` | 错误码映射非空、返回值一致性 | `test_unit_errno.c` |

### ❌ 不适合单元测试（适合集成测试）

| 功能 | 原因 |
|------|------|
| 命令解析与选项校验 | 依赖链接脚本段收集、宏展开，无法独立运行 |
| Tab 补全 | 依赖终端输入状态和候选列表的完整上下文 |
| 异步命令调度 | 依赖状态机和全局调度器，行为是时序相关的 |
| 环境变量替换 | 依赖字符串解析和命令链拆分，属于系统级行为 |

> 💡 **判断标准**：如果一个函数**不依赖任何全局状态、不需要终端输入、可以独立传入参数并得到返回值**，它就适合写 Unity 单元测试。

---

## ✅ 测试成功的依据是什么

Unity 的测试报告遵循非常直观的**红绿灯模型**：

```
/path/to/test_runners.c:57:test_cli_strlen_normal:PASS
/path/to/test_runners.c:58:test_cli_strcpy_normal:PASS
/path/to/test_runners.c:59:test_cli_atoi_positive:PASS
/path/to/test_runners.c:60:test_cli_atoi_mixed:FAIL: Expected ... Was ...
...
-----------------------
34 Tests 1 Failures 0 Ignored
FAIL
```

### 判定规则

| 结果 | 含义 | 程序退出码 |
|------|------|-----------|
| **PASS** | 该测试函数内所有断言均通过 | — |
| **FAIL** | 至少有一个断言不满足，具体差异打印在行尾 | 非 0 |
| **Ignored** | 测试被显式跳过（`TEST_IGNORE()`）| — |

### 什么是"测试通过"

一个测试文件或一个测试套件只有在以下情况下才算**完全通过**：

1. 所有 `RUN_TEST(...)` 都输出 `PASS`
2. `UNITY_END()` 最终打印的 **Failures = 0**
3. 程序退出码为 **0**

> 📝 CTest（CMake 的测试驱动工具）正是通过检查程序退出码来判断测试是否通过的。如果 `unit_tests` 返回非 0，CTest 会标记为 `Failed`。

---

## 🚀 编译与运行测试

### 一步编译所有目标

```bash
cd /path/to/LinCLI
make
```

项目根目录的 `Makefile` 会自动完成：
1. `cmake -S . -B build` — 生成构建系统
2. `make -C build` — 编译框架库 `lincli`、演示程序 `a.out`、单元测试 `unit_tests`
3. `ctest --test-dir build --output-on-failure` — 运行 47 个 Unity 单元测试

编译完成后生成的产物：
- `build/bin/a.out` — 交互式 CLI 模拟器（原有功能）
- `build/bin/unit_tests` — Unity 单元测试程序

### 直接运行单元测试

```bash
./bin/unit_tests
```

### 使用 CTest 运行（推荐）

```bash
ctest --output-on-failure
```

输出示例：

```
Test project /path/to/LinCLI/build
    Start 1: unit_tests
1/1 Test #1: unit_tests .......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 1
```

> 💡 `ctest` 会自动收集 `CMakeLists.txt` 中通过 `add_test()` 注册的所有测试，按顺序执行，并汇总通过/失败统计。这是 CI/CD 流水线中运行测试的标准方式。

---

## 🆕 为项目新增测试模块

假设你为 `lib/cli_vsnprintf.c` 新增了一组函数，想补充对应的 Unity 测试，步骤如下：

### 第 1 步：创建测试文件

```bash
touch tests/unit/test_unit_vsnprintf.c
```

内容模板：

```c
#include "unity.h"
#include "cli_vsnprintf.h"

void test_cli_snprintf_basic(void)
{
    char buf[32];
    int len = cli_snprintf(buf, sizeof(buf), "val=%d", 42);
    TEST_ASSERT_EQUAL_STRING("val=42", buf);
    TEST_ASSERT_EQUAL(6, len);
}

void test_cli_snprintf_truncate(void)
{
    char buf[4];
    int len = cli_snprintf(buf, sizeof(buf), "hello");
    TEST_ASSERT_EQUAL_STRING("hel", buf);  /* 被截断，末尾留 \0 */
}
```

### 第 2 步：在 runner 中注册

编辑 `tests/unit/test_runners.c`：

```c
/* 1. 在顶部声明外部测试函数 */
extern void test_cli_snprintf_basic(void);
extern void test_cli_snprintf_truncate(void);

/* 2. 新增一个分组函数（或加入已有分组） */
static void run_vsnprintf_tests(void)
{
    RUN_TEST(test_cli_snprintf_basic);
    RUN_TEST(test_cli_snprintf_truncate);
}

/* 3. 在 main() 中调用 */
int main(void)
{
    UNITY_BEGIN();
    /* ... 其他分组 ... */
    run_vsnprintf_tests();
    return UNITY_END();
}
```

### 第 3 步：编译验证

```bash
cd build && make -j$(nproc) && ctest --output-on-failure
```

如果新增测试编译通过且 `ctest` 显示 `100% tests passed`，任务完成。

---

## 📚 延伸阅读

- Unity 官方仓库：https://github.com/ThrowTheSwitch/Unity
- ThrowTheSwitch 组织（还开发了 CMock 和 Ceedling）：https://throwtheswitch.org
- 本文档配套的已有测试文件：`tests/unit/*.c`
- 项目原有的交互式测试说明：`Documentation/tests.md`

---

> 🎯 **最后一句**：单元测试不是为了证明代码没有 bug，而是为了在下次改代码时，**第一时间告诉你哪里被你弄坏了**。Unity 让这件事变得简单、快速、无负担。
