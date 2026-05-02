/*
 * LinCLI - User management system for embedded CLI.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_USER_H_
#define _CLI_USER_H_

#include <stddef.h>

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

#define FOR_EACH_CLI_USER(_user)                                       \
	for (const cli_user_t *const *_pp = _cli_users_start;              \
	     _pp < (const cli_user_t *const *)_cli_users_end; _pp++)       \
		if (((_user) = *_pp) != NULL)

/* ============================================================
 *  命令列表辅助宏，复刻 CANDIDATES 实现
 * ============================================================ */

#define USER_CMDS(...) ((char *[]){ __VA_ARGS__ })

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
 *   - Root 用户同样需要通过 USER_CMDS() 传入命令列表占位，
 *     框架在遍历时根据 role 判断，root 自动视为持有所有命令。
 *   - cmd_count 由 sizeof(_cmds) / sizeof(char *) 编译期自动推导，
 *     与 CLI_CANDIDATE 的 argc 计算方式完全一致。
 *
 * 示例：
 *   CLI_USER(admin, "admin", "admin123", CLI_USER_ROLE_ROOT, USER_CMDS());
 *   CLI_USER(guest, "guest", "guest", CLI_USER_ROLE_NORMAL,
 *            USER_CMDS("help", "version"));
 */

#define CLI_USER(name, _username, _password, _role, _cmds)             \
	const cli_user_t _cli_user_def_##name = {                          \
		.username = _username,                                         \
		.password = _password,                                         \
		.role = _role,                                                 \
		.cmd_count = (int)((sizeof(_cmds) / sizeof(char *))),          \
		.cmds = _cmds,                                                 \
	};                                                               \
	static const cli_user_t *const _cli_user_ptr_##name                \
		__attribute__((used, section(".cli_users.1"))) =             \
			&_cli_user_def_##name

/* ============================================================
 *  当前登录用户全局指针
 * ============================================================ */

extern const cli_user_t *current_user;

#endif /* _CLI_USER_H_ */
