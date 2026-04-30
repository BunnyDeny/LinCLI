/*
 * LinCLI - Variable export system implementation.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 */

#include "cli_var.h"
#include "cli_io.h"
#include "cli_errno.h"
#include "cli_mpool.h"
#include "cmd_dispose.h"
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
 * 打印变量值
 * ============================================================ */

static void print_int(const cli_var_t *var)
{
	all_printk("%s (INT) = %d\r\n", var->name, *(int *)var->addr);
}

static void print_double(const cli_var_t *var)
{
	all_printk("%s (DOUBLE) = %.6f\r\n", var->name,
		   *(double *)var->addr);
}

static void print_bool(const cli_var_t *var)
{
	all_printk("%s (BOOL) = %s\r\n", var->name,
		   *(bool *)var->addr ? "true" : "false");
}

static void print_string(const cli_var_t *var)
{
	all_printk("%s (STRING) = \"%s\"\r\n", var->name,
		   (char *)var->addr);
}

void cli_var_print(const cli_var_t *var)
{
	if (!var)
		return;
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

static int parse_int_value(const char *str, int *out)
{
	char *endptr;
	errno = 0;
	long val = strtol(str, &endptr, 0);
	if (endptr == str || *endptr != '\0') {
		pr_err("'%s' is not a valid integer\r\n", str);
		return -1;
	}
	if (errno == ERANGE || val > INT_MAX || val < INT_MIN) {
		pr_err("'%s' out of integer range\r\n", str);
		return -1;
	}
	*out = (int)val;
	return 0;
}

static int parse_double_value(const char *str, double *out)
{
	char *endptr;
	errno = 0;
	double val = strtod(str, &endptr);
	if (endptr == str || *endptr != '\0') {
		pr_err("'%s' is not a valid number\r\n", str);
		return -1;
	}
	if (errno == ERANGE) {
		pr_err("'%s' out of floating-point range\r\n", str);
		return -1;
	}
	*out = val;
	return 0;
}

int cli_var_set(const cli_var_t *var, const char *value)
{
	if (!var || !value)
		return CLI_ERR_NULL;

	if (var->readonly) {
		pr_err("'%s' is read-only\r\n", var->name);
		return -1;
	}

	switch (var->type) {
	case CLI_TYPE_INT: {
		int val;
		if (parse_int_value(value, &val) < 0)
			return -1;
		*(int *)var->addr = val;
		break;
	}
	case CLI_TYPE_DOUBLE: {
		double val;
		if (parse_double_value(value, &val) < 0)
			return -1;
		*(double *)var->addr = val;
		break;
	}
	case CLI_TYPE_BOOL: {
		if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
			*(bool *)var->addr = true;
		else if (strcmp(value, "false") == 0 ||
			 strcmp(value, "0") == 0)
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
			pr_warn("'%s' truncated: %zu -> %zu chars\r\n", var->name,
				len, var->size - 1);
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
		all_printk("%s\r\n",
			   *(bool *)var->addr ? "true" : "false");
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
 * set 命令行处理：set <varname> <value...>
 * ============================================================ */

/* ============================================================
 * 内部：set 子命令处理
 * ============================================================ */

static int var_set_cmd(int argc, char **argv)
{
	if (argc < 3) {
		pr_err("usage: var_set <varname> <value>\r\n");
		return -1;
	}

	const cli_var_t *var = cli_var_find(argv[1]);
	if (!var) {
		pr_err("unknown variable: %s\r\n", argv[1]);
		return -1;
	}

	if (argc == 3)
		return cli_var_set(var, argv[2]);

	/* 合并 argv[2..argc-1] 作为值（处理带空格的字符串） */
	size_t total = 0;
	for (int i = 2; i < argc; i++)
		total += strlen(argv[i]) + (i > 2 ? 1 : 0);

	char *buf = cli_mpool_alloc();
	if (!buf) {
		pr_err("out of memory\r\n");
		return -1;
	}

	int pos = 0;
	for (int i = 2; i < argc; i++) {
		if (i > 2)
			buf[pos++] = ' ';
		int len = (int)strlen(argv[i]);
		memcpy(buf + pos, argv[i], len);
		pos += len;
	}
	buf[pos] = '\0';

	int ret = cli_var_set(var, buf);
	cli_mpool_free(buf);
	return ret;
}

/* ============================================================
 * 变量命令统一分派入口
 * ============================================================ */

int cli_var_dispatch(int argc, char **argv)
{
	if (argc < 1)
		return -1;

	if (strcmp(argv[0], "var_set") == 0)
		return var_set_cmd(argc, argv);

	if (strcmp(argv[0], "var_rd") == 0) {
		if (argc < 2) {
			pr_err("usage: var_rd <varname>\r\n");
			return -1;
		}
		const cli_var_t *var = cli_var_find(argv[1]);
		if (!var) {
			pr_err("unknown variable: %s\r\n", argv[1]);
			return -1;
		}
		cli_var_print(var);
		return 0;
	}

	if (strcmp(argv[0], "var_ls") == 0) {
		cli_var_list_all();
		return 0;
	}

	pr_err("unknown var command: %s\r\n", argv[0]);
	return -1;
}

void cli_var_list_all(void)
{
	const cli_var_t *var;
	all_printk("\r\n%-20s %-10s %-24s %s\r\n", "NAME", "TYPE",
		   "VALUE", "ATTR");
	all_printk("---------------------------------------------------------"
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
		default:
			snprintf(value_buf, sizeof(value_buf), "?");
			break;
		}

		const char *type_str =
			(var->type == CLI_TYPE_INT) ?
				"INT" :
			(var->type == CLI_TYPE_DOUBLE) ?
				"DOUBLE" :
			(var->type == CLI_TYPE_BOOL) ?
				"BOOL" :
			(var->type == CLI_TYPE_STRING) ?
				"STRING" :
				"UNKNOWN";

		all_printk("%-20s %-10s %-24s %s\r\n", var->name, type_str,
			   value_buf, attr_buf);
	}
}

/* ============================================================
 * 示例变量（验证用，生产环境可删除或保留作为 demo）
 * ============================================================ */

static int g_loop_count = 0;
CLI_VAR(g_loop_count, "g_loop_count", INT, "Main loop counter");

static char g_device_name[32] = "lincli-dev";
CLI_VAR(g_device_name, "g_device_name", STRING, "Device name");

static bool g_verbose = false;
CLI_VAR(g_verbose, "g_verbose", BOOL, "Verbose output flag");

static double g_kp = 2.5;
CLI_VAR_RO(g_kp, "g_kp", DOUBLE, "PID Kp parameter (read-only)");
