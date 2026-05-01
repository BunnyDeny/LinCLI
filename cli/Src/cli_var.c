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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

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
 * 查找自定义类型
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

static void print_int(const cli_var_t *var)
{
	all_printk("%s (INT) = %d\r\n", var->name, *(int *)var->addr);
}

static void print_double(const cli_var_t *var)
{
	all_printk("%s (DOUBLE) = %.6f\r\n", var->name, *(double *)var->addr);
}

static void print_bool(const cli_var_t *var)
{
	all_printk("%s (BOOL) = %s\r\n", var->name,
		   *(bool *)var->addr ? "true" : "false");
}

static void print_string(const cli_var_t *var)
{
	all_printk("%s (STRING) = \"%s\"\r\n", var->name, (char *)var->addr);
}

void cli_var_print(const cli_var_t *var)
{
	if (!var)
		return;

	if (var->type == CLI_TYPE_CUSTOM && var->type_name) {
		const cli_var_type_t *type = cli_var_type_find(var->type_name);
		if (type && type->ops.to_string) {
			char buf[64];
			type->ops.to_string(var->addr, var->size, buf, sizeof(buf));
			all_printk("%s (%s) = %s\r\n", var->name, var->type_name,
				   buf);
			return;
		}
		all_printk("%s (%s) = <unprintable>\r\n", var->name,
			   var->type_name);
		return;
	}

	switch (var->type) {
	case CLI_TYPE_INT:
		print_int(var);
		break;
	case CLI_TYPE_DOUBLE:
		print_double(var);
		break;
	case CLI_TYPE_BOOL:
		print_bool(var);
		break;
	case CLI_TYPE_STRING:
		print_string(var);
		break;
	default:
		all_printk("%s (type=%d) = <unprintable>\r\n", var->name,
			   var->type);
		break;
	}
}

/* ============================================================
 * 解析并写入变量值
 * ============================================================ */

int cli_var_set(const cli_var_t *var, const char *value)
{
	if (!var || !value)
		return CLI_ERR_NULL;

	if (var->readonly) {
		pr_err("'%s' is read-only\r\n", var->name);
		return -1;
	}

	if (var->type == CLI_TYPE_CUSTOM && var->type_name) {
		const cli_var_type_t *type = cli_var_type_find(var->type_name);
		if (!type) {
			pr_err("unknown custom type '%s' for variable '%s'\r\n",
				var->type_name, var->name);
			return -1;
		}
		if (!type->ops.from_string) {
			pr_err("type '%s' does not support write\r\n",
				var->type_name);
			return -1;
		}
		if (type->ops.from_string(var->addr, var->size, value) < 0)
			return -1;
		/* 打印确认 */
		all_printk("%s = ", var->name);
		if (type->ops.to_string) {
			char buf[64];
			type->ops.to_string(var->addr, var->size, buf, sizeof(buf));
			all_printk("%s\r\n", buf);
		} else {
			all_printk("<ok>\r\n");
		}
		return 0;
	}

	switch (var->type) {
	case CLI_TYPE_INT: {
		int val;
		if (cli_parse_int(value, &val) < 0)
			return -1;
		*(int *)var->addr = val;
		break;
	}
	case CLI_TYPE_DOUBLE: {
		double val;
		if (cli_parse_double(value, &val) < 0)
			return -1;
		*(double *)var->addr = val;
		break;
	}
	case CLI_TYPE_BOOL: {
		if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
			*(bool *)var->addr = true;
		else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
			*(bool *)var->addr = false;
		else {
			pr_err("bool value must be true/false or 1/0\r\n");
			return -1;
		}
		break;
	}
	case CLI_TYPE_STRING: {
		size_t len = strlen(value);
		if (len >= var->size) {
			pr_warn("'%s' truncated: %zu -> %zu chars\r\n",
				var->name, len, var->size - 1);
			len = var->size - 1;
		}
		memcpy(var->addr, value, len);
		((char *)var->addr)[len] = '\0';
		break;
	}
	default:
		pr_err("type not supported for variable write\r\n");
		return -1;
	}

	/* 打印确认 */
	all_printk("%s = ", var->name);
	switch (var->type) {
	case CLI_TYPE_INT:
		all_printk("%d\r\n", *(int *)var->addr);
		break;
	case CLI_TYPE_DOUBLE:
		all_printk("%.6f\r\n", *(double *)var->addr);
		break;
	case CLI_TYPE_BOOL:
		all_printk("%s\r\n", *(bool *)var->addr ? "true" : "false");
		break;
	case CLI_TYPE_STRING:
		all_printk("\"%s\"\r\n", (char *)var->addr);
		break;
	default:
		all_printk("<ok>\r\n");
		break;
	}
	return 0;
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

static int var_handler(void *_args)
{
	struct var_args *args = _args;

	if (args->list) {
		cli_var_list_all();
		return 0;
	}

	if (args->read) {
		const cli_var_t *var = cli_var_find(args->read);
		if (!var) {
			pr_err("unknown variable: %s\r\n", args->read);
			return -1;
		}
		cli_var_print(var);
		return 0;
	}

	if (args->write) {
		const cli_var_t *var = cli_var_find(args->write);
		if (!var) {
			pr_err("unknown variable: %s\r\n", args->write);
			return -1;
		}
		return cli_var_set(var, args->val);
	}

	pr_err("usage: var -r <name>  or  var -w <name> --val <value>"
	       "  or  var -l\r\n");
	return -1;
}

CLI_COMMAND(var_cmd, "var", "Read/write exported variables",
	    USAGE("var -r <name>", "var -w <name> --val <value>", "var -l"),
	    var_handler, (struct var_args *)0,
	    OPTION('r', "read", STRING, "Read variable name", struct var_args,
		   read, 0, NULL, "write list", false),
	    OPTION('w', "write", STRING, "Write variable name", struct var_args,
		   write, 0, NULL, "read list", false),
	    OPTION('l', "list", BOOL, "List all exported variables",
		   struct var_args, list, 0, NULL, "read write", false),
	    OPTION(0, "val", STRING, "Value to write", struct var_args, val, 0,
		   "write", NULL, false),
	    END_OPTIONS);

void cli_var_list_all(void)
{
	const cli_var_t *var;
	all_printk("\r\n%-20s %-10s %-24s %-4s %s\r\n", "NAME", "TYPE", "VALUE",
		   "ATTR", "DOC");
	all_printk(
		"--------------------------------------------------------------------------"
		"\r\n");

	_FOR_EACH_CLI_VAR(_cli_vars_start, _cli_vars_end, var)
	{
		char value_buf[32] = { 0 };
		char attr_buf[16] = { 0 };

		if (var->readonly)
			snprintf(attr_buf, sizeof(attr_buf), "RO");

		switch (var->type) {
		case CLI_TYPE_INT:
			snprintf(value_buf, sizeof(value_buf), "%d",
				 *(int *)var->addr);
			break;
		case CLI_TYPE_DOUBLE:
			snprintf(value_buf, sizeof(value_buf), "%.4f",
				 *(double *)var->addr);
			break;
		case CLI_TYPE_BOOL:
			snprintf(value_buf, sizeof(value_buf), "%s",
				 *(bool *)var->addr ? "true" : "false");
			break;
		case CLI_TYPE_STRING:
			snprintf(value_buf, sizeof(value_buf), "\"%s\"",
				 (char *)var->addr);
			break;
		case CLI_TYPE_CUSTOM:
			if (var->type_name) {
				const cli_var_type_t *type = cli_var_type_find(var->type_name);
				if (type && type->ops.to_string) {
					type->ops.to_string(var->addr, var->size,
							    value_buf,
							    sizeof(value_buf));
					break;
				}
			}
			snprintf(value_buf, sizeof(value_buf), "?");
			break;
		default:
			snprintf(value_buf, sizeof(value_buf), "?");
			break;
		}

		const char *type_str =
			(var->type == CLI_TYPE_INT)    ? "INT" :
			(var->type == CLI_TYPE_DOUBLE) ? "DOUBLE" :
			(var->type == CLI_TYPE_BOOL)   ? "BOOL" :
			(var->type == CLI_TYPE_STRING) ? "STRING" :
			(var->type == CLI_TYPE_CUSTOM && var->type_name)
							 ? var->type_name :
							 "UNKNOWN";

		all_printk("%-20s %-10s %-24s %-4s %s\r\n", var->name, type_str,
			   value_buf, attr_buf, var->doc ? var->doc : "");
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

static void cli_var_candidate_init(void *arg)
{
	(void)arg;
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
	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd || !cmd->name || strcmp(cmd->name, "var") != 0)
			continue;
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
}
_EXPORT_INIT_SYMBOL(cli_var_candidate_init, 15, NULL, cli_var_candidate_init);
