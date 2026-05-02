/*
 * LinCLI - User management system for embedded CLI.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdbool.h>
#include <string.h>
#include "cli_user.h"
#include "cli_io.h"
#include "init_d.h"
#include "cmd_dispose.h"

/* ============================================================
 *  注册测试用户（验证段收集机制）
 * ============================================================ */

CLI_USER(admin, "admin", "admin123", CLI_USER_ROLE_ROOT, USER_CMDS());
CLI_USER(guest, "guest", "guest", CLI_USER_ROLE_NORMAL,
	 USER_CMDS("help", "version", "env"));
CLI_USER(operator, "op", "op_pass", CLI_USER_ROLE_NORMAL,
	 USER_CMDS("log", "ts"));

/* ============================================================
 *  当前登录用户全局指针（默认 root）
 * ============================================================ */

const cli_user_t *current_user = &_cli_user_def_admin;

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

static void su_print_users(void)
{
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

/* ============================================================
 *  su 异步命令：支持 -l 打印用户，-c 切换用户
 * ============================================================ */

struct su_args {
	bool list;
	const char *change;
};

static const cli_user_t *su_target;
static int su_attempts;
static char su_pwd[32];
static int su_pwd_len;
static bool su_prompted;

static void su_entry(void *_args)
{
	(void)_args;
	reset_cli_in_push_lock();
	su_target = NULL;
	su_attempts = 0;
	su_pwd_len = 0;
	su_pwd[0] = '\0';
	su_prompted = false;
}

static int su_verify_pwd(void)
{
	su_pwd[su_pwd_len] = '\0';
	if (strcmp(su_pwd, su_target->password) == 0) {
		current_user = su_target;
		all_printk("\r\n");
		pr_info("switched to '%s'\r\n", current_user->username);
		return 0;
	}
	su_attempts++;
	if (su_attempts >= 3) {
		all_printk("\r\n");
		pr_err("authentication failed (3 attempts)\r\n");
		return -1;
	}
	all_printk("\r\nincorrect (%d/3), try again: ", su_attempts);
	su_pwd_len = 0;
	return CLI_CONTINUE;
}

static int su_read_input(void)
{
	int size = cli_get_in_size();

	for (int i = 0; i < size; i++) {
		char ch;
		if (cli_in_pop((_u8 *)&ch, 1) < 0)
			break;
		if (ch == '\r' || ch == '\n')
			return su_verify_pwd();
		if (ch == 127 || ch == 8) {
			if (su_pwd_len > 0)
				su_pwd_len--;
			continue;
		}
		if (su_pwd_len < (int)sizeof(su_pwd) - 1)
			su_pwd[su_pwd_len++] = ch;
	}
	return CLI_CONTINUE;
}

static int su_task(void *_args)
{
	struct su_args *args = _args;

	if (args->list) {
		su_print_users();
		return 0;
	}
	if (!args->change) {
		pr_err("usage: su -l | su -c <username>\r\n");
		return -1;
	}
	if (!su_target) {
		const cli_user_t *user;
		FOR_EACH_CLI_USER(user)
		{
			if (user && strcmp(user->username, args->change) == 0) {
				su_target = user;
				break;
			}
		}
		if (!su_target) {
			pr_err("user '%s' not found\r\n", args->change);
			return -1;
		}
	}
	if (!su_prompted) {
		all_printk("Password: ");
		su_prompted = true;
	}
	return su_read_input();
}

static void su_exit(void *_args)
{
	(void)_args;
}

CLI_COMMAND_ASYNC(su_cmd, "su", "Switch user or list users",
		  USAGE("su -l", "su -c <username>"),
		  su_entry, su_task, su_exit,
		  (struct su_args *)0,
		  OPTION('l', "list", BOOL, "List all registered users",
			 struct su_args, list, 0, NULL, "change", false),
		  OPTION('c', "change", STRING, "Switch to specified user",
			 struct su_args, change, 0, NULL, "list", false),
		  END_OPTIONS);

/* ============================================================
 *  init_d 初始化函数（保留，后续扩展使用）
 * ============================================================ */

void cli_user_init(void *arg)
{
	(void)arg;
}

_EXPORT_INIT_SYMBOL(cli_user_init, 13, NULL, cli_user_init);
