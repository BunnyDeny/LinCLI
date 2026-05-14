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

#include <stdbool.h>
#include <string.h>
#include "cli_user.h"
#include "cli_io.h"
#include "init_d.h"
#include "cmd_dispose.h"

#if CLI_ENABLE_USER

/* ============================================================
 *  注册测试用户（验证段收集机制）
 * ============================================================ */

CLI_USER(admin, "admin", "admin123", CLI_USER_ROLE_ROOT, USER_CMDS_NONE);
CLI_USER(lin, "lin", "lin123", CLI_USER_ROLE_NORMAL, USER_CMDS("_echo"));

/* ============================================================
 *  当前登录用户全局指针（运行时由 cli_user_init 初始化）
 * ============================================================ */

const cli_user_t *current_user;

/* ============================================================
 *  CLI_DEFAULT_USER 暂存变量：auto_run 结束后切换的目标用户
 * ============================================================ */

const cli_user_t *_cli_default_user = NULL;

/* ============================================================
 *  auto_run 结束后切换用户（scheduler 调用）
 * ============================================================ */

void cli_user_after_auto_run(void)
{
	if (_cli_default_user)
		current_user = _cli_default_user;
}

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

	all_printk("\r\nU:\r\n");
	FOR_EACH_CLI_USER(user)
	{
		if (!user)
			continue;
		all_printk("%-10s %-5s ", user->username, role_str(user->role));
		if (user->role == CLI_USER_ROLE_ROOT) {
			all_printk("*\r\n");
		} else if (user->cmd_count == 0 || !user->cmds) {
			all_printk("-\r\n");
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
		pr_info("switched to %s\r\n", current_user->username);
		return 0;
	}
	su_attempts++;
	if (su_attempts >= 3) {
		all_printk("\r\n");
		pr_err("auth failed\r\n");
		return -1;
	}
	all_printk("\r\nbad (%d/3): ", su_attempts);
	su_pwd_len = 0;
	return CLI_CONTINUE;
}

static int su_read_input(void)
{
	int size = cli_get_in_size();

	for (int i = 0; i < size; i++) {
		char ch;
		if (cli_in_pop((_u8 *)&ch, 1) <= 0)
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

static int su_find_target(const char *username)
{
	const cli_user_t *user;
	FOR_EACH_CLI_USER(user)
	{
		if (user && strcmp(user->username, username) == 0) {
			su_target = user;
			return 0;
		}
	}
	pr_err("no user: %s\r\n", username);
	return -1;
}

static int su_prompt_password(void)
{
	if (!su_prompted) {
		all_printk("PW: ");
		su_prompted = true;
	}
	return su_read_input();
}

static int su_task(void *_args)
{
	struct su_args *args = _args;

	if (args->list) {
		su_print_users();
		return 0;
	}
	if (!args->change) {
		pr_err("su -l | su -c <user>\r\n");
		return -1;
	}
	if (!su_target && su_find_target(args->change) < 0)
		return -1;
	if (su_target == current_user) {
		pr_info("already %s\r\n", current_user->username);
		return 0;
	}
	return su_prompt_password();
}

static void su_exit(void *_args)
{
	(void)_args;
}

CLI_COMMAND_ASYNC(su_cmd, "su", "Switch usr", USAGE("su -l", "su -c <user>"),
		  su_entry, su_task, su_exit, (struct su_args *)0,
		  OPTION('l', "list", BOOL, "", struct su_args, list, 0, NULL,
			 "change", false),
		  OPTION('c', "change", STRING, "", struct su_args, change, 0,
			 NULL, "list", false),
		  END_OPTIONS);

/* ============================================================
 *  权限检查
 * ============================================================ */

int cli_user_cmd_permitted(const cli_command_t *cmd)
{
	if (!cmd || !cmd->name)
		return 0;
	if (current_user->role == CLI_USER_ROLE_ROOT)
		return 1;
	if (strcmp(cmd->name, "su") == 0)
		return 1;
	for (int i = 0; i < current_user->cmd_count; i++) {
		if (current_user->cmds[i] &&
		    strcmp(current_user->cmds[i], cmd->name) == 0)
			return 1;
	}
	return 0;
}

/* ============================================================
 *  init_d 初始化函数：为 su -c 选项附加用户名候选列表
 * ============================================================ */

#define MAX_CLI_USER_CANDS 32

static char *user_names[MAX_CLI_USER_CANDS + 1];
static int user_name_count;

static void cli_user_collect_candidates(void)
{
	const cli_user_t *user;
	user_name_count = 0;
	FOR_EACH_CLI_USER(user)
	{
		if (!user || !user->username)
			continue;
		if (user_name_count < MAX_CLI_USER_CANDS)
			user_names[user_name_count++] = (char *)user->username;
	}
}

static void cli_user_attach_candidates(const cli_command_t *cmd)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		if (!opt->long_opt)
			continue;
		if (strcmp(opt->long_opt, "change") == 0) {
			opt->candidate_argc = user_name_count;
			opt->candidate_argv = user_names;
		}
	}
}

void cli_user_init(void *arg)
{
	(void)arg;
	current_user = &_cli_user_def_admin;
	cli_user_collect_candidates();

	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(cmd)
	{
		if (!cmd || !cmd->name || strcmp(cmd->name, "su") != 0)
			continue;
		cli_user_attach_candidates(cmd);
	}
}

_EXPORT_INIT_SYMBOL(cli_user_init, 13, NULL, cli_user_init);

#else /* !CLI_ENABLE_USER */

/* ============================================================
 *  存根：默认 admin 用户，所有命令允许
 * ============================================================ */

static const cli_user_t _cli_user_def_admin = {
	.username = "admin",
	.role = CLI_USER_ROLE_ROOT,
	.cmd_count = 0,
	.cmds = NULL,
};

const cli_user_t *current_user = &_cli_user_def_admin;

int cli_user_cmd_permitted(const cli_command_t *cmd)
{
	(void)cmd;
	return 1;
}

void cli_user_after_auto_run(void)
{
}

#endif /* CLI_ENABLE_USER */
