# OPTION 宏的重载实现原理

本文档解释 `cli/Inc/cmd_dispose.h` 中 `OPTION(...)` 宏背后的**参数计数技巧**（Macro Overloading by Argument Counting）。

---

## 问题背景

C 语言预处理器本身**不支持函数重载**。当我们希望一个 `OPTION` 宏能同时覆盖以下场景时：

- 基础类型（6 个参数）
- 基础类型 + `required`（7 个参数）
- 数组类型 + `max_args` + `depends`（8 个参数）
- 数组类型 + `max_args` + `depends` + `required`（9 个参数）

就必须借助宏技巧，让预处理器根据参数数量自动分发到对应的底层宏（`OPTION_6`、`OPTION_7`、`OPTION_8`、`OPTION_9`）。

---

## 核心代码

```c
#define _GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME

#define OPTION(...)                                           \
	_GET_MACRO(__VA_ARGS__, OPTION_9, OPTION_8, OPTION_7, \
		   OPTION_6)(__VA_ARGS__)
```

虽然只有两行，但它是一个非常经典的 C 预处理器惯用法。

---

## 逐行拆解

### 1. `_GET_MACRO` —— 只取第 10 个参数

```c
#define _GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME
```

这个宏有 **10 个固定形参**，其中 `_1` ~ `_9` 和 `...` 都只是占位符，真正有用的是 `NAME`。展开后，它**只返回第 10 个参数**。

### 2. `OPTION(...)` —— 把参数和候选宏一起塞进去

```c
#define OPTION(...)                                           \
	_GET_MACRO(__VA_ARGS__, OPTION_9, OPTION_8, OPTION_7, \
		   OPTION_6)(__VA_ARGS__)
```

当用户写：

```c
OPTION('v', "verbose", BOOL, "Enable verbose", struct_args, verbose)
```

这里有 **6 个实参**。展开后变成：

```c
_GET_MACRO('v', "verbose", BOOL, "Enable verbose", struct_args, verbose,
	   OPTION_9, OPTION_8, OPTION_7, OPTION_6)('v', "verbose", ...)
```

现在把这 10 个参数依次填入 `_GET_MACRO` 的形参：

| 形参 | 实参 |
|------|------|
| `_1` | `'v'` |
| `_2` | `"verbose"` |
| `_3` | `BOOL` |
| `_4` | `"Enable verbose"` |
| `_5` | `struct_args` |
| `_6` | `verbose` |
| `_7` | `OPTION_9` |
| `_8` | `OPTION_8` |
| `_9` | `OPTION_7` |
| `NAME` | **`OPTION_6`** |

`_GET_MACRO` 只返回 `NAME`，所以最终得到 **`OPTION_6`**。后面的 `(__VA_ARGS__)` 再把原始的 6 个实参传给 `OPTION_6`。

---

## 不同参数数量的映射关系

| 用户传入参数个数 | 前面被 `_1`~`_N` 占用 | 第 10 个位置 `NAME` 是谁 | 最终调用的宏 |
|----------------|----------------------|------------------------|-------------|
| 6 | `_1` ~ `_6` | `OPTION_6` | `OPTION_6(...)` |
| 7 | `_1` ~ `_7` | `OPTION_7` | `OPTION_7(...)` |
| 8 | `_1` ~ `_8` | `OPTION_8` | `OPTION_8(...)` |
| 9 | `_1` ~ `_9` | `OPTION_9` | `OPTION_9(...)` |

这就实现了一个**纯预处理期的参数数量分发器**。

---

## 形象比喻

想象 `_GET_MACRO` 是一条 **10 个座位的传送带**。我们把 `OPTION_9`、`OPTION_8`、`OPTION_7`、`OPTION_6` 按顺序放在最后几格，前面坐的是用户传的实际参数。

不管前面坐了几个人（6~9 个），**第 10 号座位上永远坐着对应数量的 `OPTION_x`**。宏最后把第 10 号座位上的人拿出来，就完成了一次“按人数自动分派”。

---

## 限制与注意事项

1. **参数数量必须严格匹配**：如果某个实参本身包含逗号（比如没加括号的结构体初始化），预处理器会把它当成多个参数，导致计数错误。因此调用 `OPTION` 时，每个实参必须是简单的 token 或字符串。
2. **最大支持到 N 个参数**：`_GET_MACRO` 里写了几对占位符，就最多支持到几参数。如果需要更多重载，只需扩展 `_GET_MACRO` 的形参列表即可。

---

## 一句话总结

> 通过在变参后面追加一组候选宏，再用一个只返回第 N 个参数的固定元数宏做“索引”，即可让 C 预处理器根据实参数量自动选择对应的底层宏。
