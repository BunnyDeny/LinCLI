# 🛡️ 用户管理系统

LinCLI 内建一套轻量级用户管理系统 🎯，用于在**产品化阶段**对不同角色进行命令级权限控制 🔐。它并非为多用户并发会话设计，而是为**同一套固件在不同人员手中呈现不同的 CLI 视图** 👀 提供安全收口 ✨。

---

## 🎨 设计哲学：开发期便利与部署期控制的分离

### 🤔 为什么需要用户管理？

起初，在只有一个开发者的嵌入式项目中引入"用户管理"似乎有些小题大做 🤷。但 LinCLI 的设计目标不仅是"让开发者爽" 😎，更是**让开发成果能够安全地走向产品化** 🚀！

想象以下场景 🎬：

| 角色 👤 | 需要的命令 ✅ | 被禁止的命令 ❌ |
|------|-----------|-------------|
| 🔧 产线测试员 | `test_led`、`calibrate`、`read_adc` | `flash_erase`、`partition`、`su` |
| 🏭 现场运维工程师 | `log`、`net --status`、`reboot` | 调试变量、内存池操作 |
| 👨‍💻 固件开发者（你）| `全部` ⭐ | 无 🎉 |

同一套固件烧录到设备后 🔥，**不同人员通过串口连接设备时，应该看到不同的命令集** 📟。这就是用户管理存在的意义 💡！

### 🏗️ 命令注册 vs 用户授权：两个不同阶段的职责

LinCLI 的设计理念是**分层抽象** 🥞，把"命令是否存在"和"谁能用"解耦为两个独立阶段：

```
开发期 🛠️                        部署期 🚢
─────────────────────────────────────────────────
CLI_COMMAND(led, ...)    →   命令"上架"到系统 📦
CLI_USER(op, ...,         →   决定 op 能"购买"哪些 🛒
          USER_CMDS("led", "sensor"))
```

- **`CLI_COMMAND`** 解决的是**"命令存在"**的问题 ✅。开发者用一行宏把功能做出来，框架自动收集、解析、补全 🎯。这是**开发者友好型**设计的核心 💎！
- **`CLI_USER`** 解决的是**"谁能用"**的问题 🔒。部署者（或产品定义者）决定哪些已上架的命令对某个用户角色可见 🎭。这是**产品安全型**设计的收口 🛡️。

这和 Linux 的哲学完全一致 🐧：
- 开发者把二进制装到 `/usr/bin`（上架 📦）
- root 写 `sudoers` 决定谁能执行（授权 🔑）

### 🤷 "普通用户多一步"真的麻烦吗？

表面上看，`CLI_COMMAND` 追求"一行注册即可使用" ✨，而 `CLI_USER` 要求普通用户"显式添加才能使用命令"，似乎与极简理念冲突 ⚡。但实际上：

1. 📝 **编译期完成**：`USER_CMDS("led", "sensor")` 是写在 C 代码里的，编译进固件后就固定了，不需要运行时配置 🎉
2. 🎭 **角色数量极少**：一个产品通常只需要 2~3 个角色（root + 现场人员 + 产线人员），配置一次即可 ⭐
3. 🔍 **显式优于隐式**：如果默认"所有用户都能用所有命令"，那安全就形同虚设 💥。显式声明 `cmds` 虽然多写一行，但权限边界一目了然 🎯，调试时也不会困惑"为什么这个命令突然不见了" 🤔

因此，LinCLI 不是"普通用户麻烦一点来换取开发者便利" 🎭，而是**在正确的阶段做正确的事** 💡：
- 🛠️ 开发阶段追求极简注册（`CLI_COMMAND` 一行搞定 ✨）
- 🚢 部署阶段追求精确控制（`CLI_USER` 一行收口 🔐）

两者都是一行宏 🎉，只是面向的对象不同 👥！

---

## 📋 用户结构体

```c
typedef struct cli_user {
	const char *username;   /* 👤 用户名 */
	const char *password;   /* 🔑 密码 */
	cli_user_role_t role;   /* 🎭 角色 */
	int cmd_count;          /* 📊 命令数量 */
	char **cmds;            /* 📋 持有命令列表 */
} cli_user_t;
```

| 字段 📌 | 说明 📝 |
|------|------|
| `username` 👤 | 用户名字符串，用于登录和提示符显示 🏷️ |
| `password` 🔑 | 密码字符串，用于 `su -c` 切换时验证 ✅ |
| `role` 🎭 | `CLI_USER_ROLE_ROOT`（⭐ 持有所有命令）或 `CLI_USER_ROLE_NORMAL`（📋 仅限 `cmds` 列表） |
| `cmd_count` 📊 | `cmds` 数组长度，由 `USER_CMDS` 宏自动推导 🎯 |
| `cmds` 📋 | 该用户有权使用的命令名字符串数组 🔐 |

---

## 📝 注册用户

```c
#include "cli_user.h" /* 🛡️ 用户管理头文件 */

/* 👑 root 用户：持有所有命令 */
CLI_USER(admin, "admin", "admin123", CLI_USER_ROLE_ROOT, USER_CMDS());

/* 👤 普通用户：仅持有 help 和 _echo */
CLI_USER(lin,   "lin",   "lin123",   CLI_USER_ROLE_NORMAL,
         USER_CMDS("help", "_echo"));
```

**参数说明** 📝：
- `name` 🏷️：宏实例名，需唯一，用于生成内部符号
- `username` / `password` 👤🔑：登录凭证
- `role` 🎭：`CLI_USER_ROLE_ROOT` 或 `CLI_USER_ROLE_NORMAL`
- `cmds` 📋：通过 `USER_CMDS(...)` 宏定义的命令名列表
  - 👑 Root 用户传 `USER_CMDS()`（空占位，框架自动视为持有所有命令 ⭐）
  - 👤 Normal 用户必须显式列出可使用的命令名 🔐

> 📝 **命令名必须与 `CLI_COMMAND` 中注册的 `cmd_str` 完全一致** ✅。例如 `_echo` 命令在注册时写的是 `CLI_COMMAND(_echo, "_echo", ...)`，因此 `USER_CMDS` 中也要写 `"_echo"` 🎯。

---

## 🔄 su 命令：切换用户

框架内建 `su` 命令 🔄，支持两个选项：

| 选项 ⚙️ | 功能 💡 |
|------|------|
| `-l` / `--list` 📋 | 列出所有注册用户 👥 |
| `-c` / `--change` 🔄 | 切换到指定用户，提示输入密码 🔑 |

```bash
# 📋 查看所有用户
lin@linCli> su -l

# 🔄 切换到 admin（需要输入密码）
lin@linCli> su -c admin
Password: ***** 🔑
[INFO] ✅ switched to 'admin'
admin@linCli> 👑

# ⚡ 如果已经在目标用户，直接提示
admin@linCli> su -c admin
[INFO] ℹ️ already logged in as 'admin'
```

**密码验证规则** 🔐：
- ✅ 输入正确密码 → 切换成功，提示符同步更新为当前用户名 🎉
- ❌ 输入错误密码 → 最多允许 3 次尝试，超过后报错并退出 💥
- 🔄 `su` 和 `help` 对所有用户开放（避免普通用户切换后被困死 🚪）

---

## 🛡️ 权限控制与 Tab 补全过滤

权限检查在**两个层面**生效 🎯：

### 1️⃣ 命令执行拦截 ❌

当用户输入一个未授权的命令并回车时 ⏎，框架直接拦截 🚫：

```bash
lin@linCli> level --info
[ERR] ❌ permission denied: 'level' is not allowed for user 'lin'
```

### 2️⃣ Tab 补全过滤 🔍

当用户按 `Tab` 键补全命令名时 ⌨️，框架只会列出该用户**有权使用**的命令 ✅：

```bash
# 👑 admin 按 Tab，显示全部命令 ⭐
admin@linCli> <Tab>
env    help    level    su    var

# 👤 lin 按 Tab，仅显示被授权的命令 🔐
lin@linCli> <Tab>
_echo    help    su
```

这意味着**普通用户甚至不知道系统里存在哪些未授权命令** 🙈——攻击面被彻底隐藏 🛡️✨！
