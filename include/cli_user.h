/*
 * LinCLI - User management system for embedded CLI.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _CLI_USER_H_
#define _CLI_USER_H_

#include <stddef.h>
#include "cmd_dispose.h"

/* ============================================================
 *  类型定义
 * ============================================================ */

typedef enum {
	CLI_USER_ROLE_ROOT,
	CLI_USER_ROLE_NORMAL,
} cli_user_role_t;

typedef struct cli_user {
	const char *username;
	const char *password;
	cli_user_role_t role;
	int cmd_count;
	char **cmds;
} cli_user_t;

/* ============================================================
 *  链接脚本段收集符号声明
 * ============================================================ */

extern const cli_user_t *const _cli_users_start[];
extern const cli_user_t *const _cli_users_end[];

/* ============================================================
 *  遍历宏
 * ============================================================ */

#define FOR_EACH_CLI_USER(_user)                                     \
	for (const cli_user_t *const *_pp = _cli_users_start;        \
	     _pp < (const cli_user_t *const *)_cli_users_end; _pp++) \
		if (((_user) = *_pp) != NULL)

/* ============================================================
 *  命令列表辅助宏，复刻 CANDIDATES 实现
 * ============================================================ */

#define USER_CMDS(...) ((char *[]){ __VA_ARGS__, NULL })
#define USER_CMDS_NONE ((char *[]){ NULL })

/* ============================================================
 *  注册宏：定义一个 cli_user_t 并将其指针放入 .cli_users 段
 * ============================================================
 *
 * 参数：
 *   name         - 宏实例名（用于生成内部静态变量名，需唯一）
 *   _username    - 用户名字符串
 *   _password    - 密码字符串
 *   _role        - 权限角色（CLI_USER_ROLE_ROOT / CLI_USER_ROLE_NORMAL）
 *   _cmds        - 命令列表指针（通过 USER_CMDS(...) 宏定义）
 *
 * 说明：
 *   - Root 用户同样需要通过 USER_CMDS_NONE 传入空命令列表占位，
 *     框架在遍历时根据 role 判断，root 自动视为持有所有命令。
 *   - cmd_count 由 sizeof(_cmds) / sizeof(char *) - 1 编译期自动推导
 *     （末尾 NULL 哨兵不计入），与 CLI_CANDIDATE 的 argc 计算方式完全一致。
 *
 * 示例：
 *   CLI_USER(admin, "admin", "admin123", CLI_USER_ROLE_ROOT, USER_CMDS_NONE);
 *   CLI_USER(guest, "guest", "guest", CLI_USER_ROLE_NORMAL,
 *            USER_CMDS("help", "version"));
 */

#if CLI_ENABLE_USER
#define CLI_USER(name, _username, _password, _role, _cmds)                \
	const cli_user_t _cli_user_def_##name = {                         \
		.username = _username,                                    \
		.password = _password,                                    \
		.role = _role,                                            \
		.cmd_count = (int)((sizeof(_cmds) / sizeof(char *))) - 1, \
		.cmds = _cmds,                                            \
	};                                                                \
	static const cli_user_t *const _cli_user_ptr_##name               \
		__attribute__((used, section(".cli_users.1"))) =          \
			&_cli_user_def_##name
#else
#define CLI_USER(name, _username, _password, _role, _cmds) /* disabled */
#endif

/* ============================================================
 *  当前登录用户全局指针
 * ============================================================ */

extern const cli_user_t *current_user;

/* ============================================================
 *  自动运行结束后切换用户
 * ============================================================ */

#if CLI_ENABLE_USER
#include "init_d.h"

extern const cli_user_t *_cli_default_user;
extern void cli_user_after_auto_run(void);

/* ============================================================
 *  设置默认用户宏：注册一个 init_d 函数，在运行时把指定用户
 *  暂存到 _cli_default_user；auto_run 结束后 cli_user_after_auto_run()
 *  再将 current_user 切过去。此时所有初始化命令已以 root 执行完毕。
 * ============================================================ */

#define CLI_DEFAULT_USER(name)                                      \
	extern const cli_user_t _cli_user_def_##name;               \
	static void _cli_user_set_default_##name(void *_arg)        \
	{                                                           \
		(void)_arg;                                         \
		extern const cli_user_t *_cli_default_user;         \
		_cli_default_user = &_cli_user_def_##name;          \
	}                                                           \
	_EXPORT_INIT_SYMBOL(_cli_user_set_default_##name, 14, NULL, \
			    _cli_user_set_default_##name)
#else
#define CLI_DEFAULT_USER(name) /* disabled */
#endif

/* ============================================================
 *  权限检查：判断当前用户是否有权访问指定命令
 * ============================================================ */

int cli_user_cmd_permitted(const cli_command_t *cmd);
void cli_user_after_auto_run(void);

#endif /* _CLI_USER_H_ */
