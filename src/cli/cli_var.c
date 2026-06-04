/*
 * LinCLI - Variable export system implementation.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 */

#include "cli_var.h"
#include "cli_io.h"
#include "cli_errno.h"
#include "cli_mpool.h"
#include "cli_parse.h"
#include "cmd_dispose.h"
#include "init_d.h"
#include "cli_float.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#if CLI_ENABLE_VAR

/* ============================================================
 * 查找变量
 * ============================================================ */

const cli_var_t *cli_var_find(const char *name)
{
	const cli_var_t *var;
	_FOR_EACH_CLI_VAR(_cli_vars_start, _cli_vars_end, var)
	{
		if (var->name && strcmp(var->name, name) == 0)
			return var;
	}
	return NULL;
}

/* ============================================================
 * 查找类型（含内建类型与自定义类型）
 * ============================================================ */

static const cli_var_type_t *cli_var_type_find(const char *name)
{
	const cli_var_type_t *type;
	_FOR_EACH_CLI_VAR_TYPE(_cli_var_types_start, _cli_var_types_end, type)
	{
		if (type->name && strcmp(type->name, name) == 0)
			return type;
	}
	return NULL;
}

/* ============================================================
 * 打印变量值
 * ============================================================ */

void cli_var_print(const cli_var_t *var)
{
	if (!var || !var->type_name)
		return;

	const cli_var_type_t *type = cli_var_type_find(var->type_name);
	if (type && type->ops.to_string) {
		char *buf = cli_mpool_alloc();
		if (!buf) {
			all_printk("%s=%s?<oom>\r\n", var->name,
				   var->type_name);
			return;
		}
		type->ops.to_string(var->addr, var->size, buf, CLI_MPOOL_SIZE);
		all_printk("%s=%s:%s\r\n", var->name, var->type_name, buf);
		cli_mpool_free(buf);
	} else {
		all_printk("%s=%s?<n/a>\r\n", var->name,
			   var->type_name);
	}
}

/* ============================================================
 * 解析并写入变量值
 * ============================================================ */

static const cli_var_type_t *cli_var_set_lookup_type(const cli_var_t *var)
{
	const cli_var_type_t *type = cli_var_type_find(var->type_name);
	if (!type) {
		pr_err("unk type %s:%s\r\n",
			var->type_name, var->name);
		return NULL;
	}
	if (!type->ops.from_string) {
		pr_err("type %s RO\r\n",
			var->type_name);
		return NULL;
	}
	return type;
}

static int cli_var_set_print_confirm(const cli_var_t *var,
				     const cli_var_type_t *type)
{
	all_printk("%s = ", var->name);
	if (type->ops.to_string) {
		char *buf = cli_mpool_alloc();
		if (!buf) {
			all_printk("<oom>\r\n");
			return -1;
		}
		type->ops.to_string(var->addr, var->size, buf, CLI_MPOOL_SIZE);
		all_printk("%s\r\n", buf);
		cli_mpool_free(buf);
	} else {
		all_printk("<ok>\r\n");
	}
	return 0;
}

int cli_var_set(const cli_var_t *var, const char *value)
{
	if (!var || !value)
		return CLI_ERR_NULL;

	if (var->readonly) {
		pr_err("'%s' RO\r\n", var->name);
		return -1;
	}

	if (!var->type_name) {
		pr_err("'%s' no type\r\n", var->name);
		return -1;
	}

	const cli_var_type_t *type = cli_var_set_lookup_type(var);
	if (!type)
		return -1;
	if (type->ops.from_string(var->addr, var->size, value) < 0)
		return -1;

	return cli_var_set_print_confirm(var, type);
}

/* ============================================================
 * var 命令：基于选项的变量读写（符合 LinCLI 框架哲学）
 * ============================================================ */

void cli_var_list_all(void);

struct var_args {
	char *read;
	char *write;
	char *val;
	bool list;
};

static int var_handle_list(void)
{
	cli_var_list_all();
	return 0;
}

static int var_handle_read(const char *name)
{
	const cli_var_t *var = cli_var_find(name);
	if (!var) {
		pr_err("unknown: %s\r\n", name);
		return -1;
	}
	cli_var_print(var);
	return 0;
}

static int var_handle_write(const char *name, const char *val)
{
	const cli_var_t *var = cli_var_find(name);
	if (!var) {
		pr_err("unknown: %s\r\n", name);
		return -1;
	}
	return cli_var_set(var, val);
}

static int var_handler(void *_args)
{
	struct var_args *args = _args;

	if (args->list)
		return var_handle_list();
	if (args->read)
		return var_handle_read(args->read);
	if (args->write)
		return var_handle_write(args->write, args->val);

	pr_err("var -r <n> | var -w <n> --val <v> | var -l\r\n");
	return -1;
}

CLI_COMMAND(var_cmd, "var", "Variables",
	    USAGE("var -r <n>", "var -w <n> --val <v>", "var -l"),
	    var_handler, (struct var_args *)0,
	    OPTION('r', "read", STRING, "", struct var_args,
		   read, 0, NULL, "write list", false),
	    OPTION('w', "write", STRING, "", struct var_args,
		   write, 0, NULL, "read list", false),
	    OPTION('l', "list", BOOL, "",
		   struct var_args, list, 0, NULL, "read write", false),
	    OPTION(0, "val", STRING, "", struct var_args, val, 0,
		   "write", NULL, false),
	    END_OPTIONS);

/* ============================================================
 * 列表辅助函数
 * ============================================================ */

static void cli_var_format_value(const cli_var_t *var, char *buf, size_t size)
{
	if (!var->type_name) {
		snprintf(buf, (int)size, "?");
		return;
	}
	const cli_var_type_t *type = cli_var_type_find(var->type_name);
	if (type && type->ops.to_string) {
		type->ops.to_string(var->addr, var->size, buf, size);
	} else {
		snprintf(buf, (int)size, "?");
	}
}

static void cli_var_print_entry(const cli_var_t *var, const char *value_buf)
{
	all_printk("%s %s %s\r\n", var->name,
		   var->type_name ? var->type_name : "?",
		   value_buf);
}

static char *cli_var_alloc_value_buf(void)
{
	char *buf = cli_mpool_alloc();
	if (buf)
		buf[0] = '\0';
	return buf;
}

void cli_var_list_all(void)
{
	const cli_var_t *var;


	_FOR_EACH_CLI_VAR(_cli_vars_start, _cli_vars_end, var)
	{
		char *value_buf = cli_var_alloc_value_buf();
		if (!value_buf)
			continue;
		cli_var_format_value(var, value_buf, CLI_MPOOL_SIZE);
		cli_var_print_entry(var, value_buf);
		cli_mpool_free(value_buf);
	}
}

/* ============================================================
 * 运行时填充 var 命令 -r / -w 选项的候选列表
 * ============================================================ */

#define MAX_CLI_VAR_CANDS 64

static char *var_read_names[MAX_CLI_VAR_CANDS + 1];
static char *var_write_names[MAX_CLI_VAR_CANDS + 1];
static int var_read_count;
static int var_write_count;

static void cli_var_collect_candidates(void)
{
	const cli_var_t *var;
	var_read_count = 0;
	var_write_count = 0;
	_FOR_EACH_CLI_VAR(_cli_vars_start, _cli_vars_end, var)
	{
		if (!var || !var->name)
			continue;
		if (var_read_count < MAX_CLI_VAR_CANDS)
			var_read_names[var_read_count++] = (char *)var->name;
		if (!var->readonly && var_write_count < MAX_CLI_VAR_CANDS)
			var_write_names[var_write_count++] = (char *)var->name;
	}
}

static void cli_var_attach_candidates(const cli_command_t *cmd)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strcmp(opt->long_opt, "read") == 0) {
			opt->candidate_argc = var_read_count;
			opt->candidate_argv = var_read_names;
		} else if (opt->long_opt &&
			   strcmp(opt->long_opt, "write") == 0) {
			opt->candidate_argc = var_write_count;
			opt->candidate_argv = var_write_names;
		}
	}
}

static void cli_var_candidate_init(void *arg)
{
	(void)arg;
	cli_var_collect_candidates();

	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(cmd)
	{
		if (!cmd || !cmd->name || strcmp(cmd->name, "var") != 0)
			continue;
		cli_var_attach_candidates(cmd);
	}
}
_EXPORT_INIT_SYMBOL(cli_var_candidate_init, 15, NULL, cli_var_candidate_init);

/* ============================================================
 * 内建类型回调实现
 * ============================================================
 *
 * INT / FLOAT / BOOL / STRING 统一基于 cli_var_type_ops_t 实现，
 * 通过 CLI_VAR_TYPE 宏注册到 .cli_var_types 段。
 * 对用户完全透明，CLI_VAR() 宏底层走的就是这套机制。
 */

static int builtin_int_from_str(void *addr, size_t size, const char *str)
{
	int val;
	if (cli_parse_int(str, &val) < 0)
		return -1;
	*(int *)addr = val;
	return 0;
}

static int builtin_int_to_str(const void *addr, size_t size, char *buf,
			      size_t buf_size)
{
	snprintf(buf, (int)buf_size, "%d", *(const int *)addr);
	return 0;
}

static int builtin_float_from_str(void *addr, size_t size, const char *str)
{
	float val;
	if (cli_parse_float(str, &val) < 0)
		return -1;
	*(float *)addr = val;
	return 0;
}

static int builtin_float_to_str(const void *addr, size_t size, char *buf,
				 size_t buf_size)
{
	(void)size;
	cli_ftoa(*(const float *)addr, buf, (int)buf_size, 6);
	return 0;
}

static int builtin_bool_from_str(void *addr, size_t size, const char *str)
{
	if (strcmp(str, "true") == 0 || strcmp(str, "1") == 0)
		*(bool *)addr = true;
	else if (strcmp(str, "false") == 0 || strcmp(str, "0") == 0)
		*(bool *)addr = false;
	else {
		pr_err("bool: true/false/1/0\r\n");
		return -1;
	}
	return 0;
}

static int builtin_bool_to_str(const void *addr, size_t size, char *buf,
			       size_t buf_size)
{
	snprintf(buf, (int)buf_size, "%s",
		 *(const bool *)addr ? "true" : "false");
	return 0;
}

static int builtin_string_from_str(void *addr, size_t size, const char *str)
{
	if (size == 0)
		return -1;
	size_t len = strlen(str);
	if (len >= size) {
		pr_warn("str trunc %zu>%zu\r\n", len, size - 1);
		len = size - 1;
	}
	memcpy(addr, str, len);
	((char *)addr)[len] = '\0';
	return 0;
}

static int builtin_string_to_str(const void *addr, size_t size, char *buf,
				 size_t buf_size)
{
	snprintf(buf, (int)buf_size, "\"%s\"", (const char *)addr);
	return 0;
}

/* 注册内建类型到 .cli_var_types 段 */
CLI_VAR_TYPE(INT, builtin_int_from_str, builtin_int_to_str);
CLI_VAR_TYPE(FLOAT, builtin_float_from_str, builtin_float_to_str);
CLI_VAR_TYPE(BOOL, builtin_bool_from_str, builtin_bool_to_str);
CLI_VAR_TYPE(STRING, builtin_string_from_str, builtin_string_to_str);

#endif /* CLI_ENABLE_VAR */
