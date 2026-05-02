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
	int cmd_count;	/* -1 = all commands (root), >=0 = cmds[] size */
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

#define _FOR_EACH_CLI_USER(_start, _end, _user)                        \
	for (const cli_user_t *const *_pp = (_start);                      \
	     _pp < (const cli_user_t *const *)(_end); _pp++)               \
		if (((_user) = *_pp) != NULL)

/* ============================================================
 *  命令列表辅助宏，完全复刻 CANDIDATES 实现
 * ============================================================ */

#define USER_CMDS(...) ((char *[]){ __VA_ARGS__ })

/* ============================================================
 *  参数计数器（支持 4 参数或 5 参数重载）
 * ============================================================ */

#define _CLI_USER_COUNT(...) _CLI_USER_COUNT_IMPL(__VA_ARGS__, 5, 4, 3, 2, 1)
#define _CLI_USER_COUNT_IMPL(_1, _2, _3, _4, _5, N, ...) N

#define _CLI_USER_JOIN(a, b) _CLI_USER_JOIN_IMPL(a, b)
#define _CLI_USER_JOIN_IMPL(a, b) a##b
#define _CLI_USER_CHOOSER(...)                                         \
	_CLI_USER_JOIN(_CLI_USER_, _CLI_USER_COUNT(__VA_ARGS__))

/* ============================================================
 *  注册宏底层实现
 * ============================================================ */

#define _CLI_USER_IMPL(name, _username, _password, _role, _cmds,       \
			_cmd_cnt)                                          \
	static const cli_user_t _cli_user_def_##name = {                   \
		.username = _username,                                         \
		.password = _password,                                         \
		.role = _role,                                                 \
		.cmd_count = _cmd_cnt,                                         \
		.cmds = _cmds,                                                 \
	};                                                               \
	static const cli_user_t *const _cli_user_ptr_##name                \
		__attribute__((used, section(".cli_users.1"))) =             \
			&_cli_user_def_##name

/* 4 参数：root 用户，cmd_count = -1, cmds = NULL */
#define _CLI_USER_4(_prefix, name, _username, _password, _role)        \
	_prefix##IMPL(name, _username, _password, _role, NULL, -1)

/*
 * 5 参数：普通用户，cmd_count 由 USER_CMDS 自动推导（复刻 CANDIDATES）
 * sizeof(_cmds) / sizeof(char *) 中 _cmds 为复合字面量数组，编译期得出元素个数
 */
#define _CLI_USER_5(_prefix, name, _username, _password, _role, _cmds) \
	_prefix##IMPL(name, _username, _password, _role, _cmds,            \
		      (int)(sizeof(_cmds) / sizeof(char *)))

/* ============================================================
 *  公共注册宏：根据参数数量自动分发
 *
 *  Root 用户（持有所有命令，cmds 参数可省略）：
 *    CLI_USER(admin, "admin", "admin123", CLI_USER_ROLE_ROOT);
 *
 *  普通用户（持有指定命令列表）：
 *    CLI_USER(guest, "guest", "guest", CLI_USER_ROLE_NORMAL,
 *             USER_CMDS("help", "version"));
 * ============================================================ */

#define CLI_USER(...) _CLI_USER_CHOOSER(__VA_ARGS__)(_CLI_USER_, __VA_ARGS__)

#endif /* _CLI_USER_H_ */
