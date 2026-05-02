/*
 * LinCLI - User management system for embedded CLI.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "cli_user.h"
#include "cli_io.h"
#include "init_d.h"

/* ============================================================
 *  注册测试用户（验证段收集机制）
 * ============================================================ */

CLI_USER(admin, "admin", "admin123", CLI_USER_ROLE_ROOT, USER_CMDS());
CLI_USER(guest, "guest", "guest", CLI_USER_ROLE_NORMAL,
	 USER_CMDS("help", "version", "env"));
CLI_USER(operator, "op", "op_pass", CLI_USER_ROLE_NORMAL,
	 USER_CMDS("log", "ts"));

/* ============================================================
 *  辅助函数
 * ============================================================ */

static const char *role_str(cli_user_role_t role)
{
	switch (role) {
	case CLI_USER_ROLE_ROOT:
		return "root";
	case CLI_USER_ROLE_NORMAL:
		return "user";
	default:
		return "unknown";
	}
}

/* ============================================================
 *  init_d 初始化函数：遍历打印所有注册用户
 * ============================================================ */

void cli_user_init(void *arg)
{
	(void)arg;
	const cli_user_t *user;

	all_printk("\r\n[User Manager] registered users:\r\n");
	FOR_EACH_CLI_USER(user)
	{
		if (!user)
			continue;
		all_printk("  user: %-10s  role: %-5s  cmds: ",
			   user->username, role_str(user->role));
		if (user->role == CLI_USER_ROLE_ROOT) {
			all_printk("(all)\r\n");
		} else if (user->cmd_count == 0 || !user->cmds) {
			all_printk("(none)\r\n");
		} else {
			for (int i = 0; i < user->cmd_count; i++) {
				all_printk("%s%s", user->cmds[i],
					   (i < user->cmd_count - 1) ? ", " :
								       "");
			}
			all_printk("\r\n");
		}
	}
}

_EXPORT_INIT_SYMBOL(cli_user_init, 13, NULL, cli_user_init);
