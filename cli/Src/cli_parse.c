/*
 * LinCLI - Common string-to-value parsers.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 */

#include "cli_parse.h"
#include "cli_errno.h"
#include "cli_io.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int cli_parse_int(const char *str, int *out)
{
	char *endptr;
	errno = 0;
	long val = strtol(str, &endptr, 0);
	if (endptr == str || *endptr != '\0') {
		pr_err("'%s' is not a valid integer\r\n", str);
		return CLI_ERR_INT_FMT;
	}
	if (errno == ERANGE || val > INT_MAX || val < INT_MIN) {
		pr_err("'%s' out of integer range\r\n", str);
		return CLI_ERR_INT_RANGE;
	}
	*out = (int)val;
	return CLI_OK;
}

int cli_parse_double(const char *str, double *out)
{
	char *endptr;
	errno = 0;
	double val = strtod(str, &endptr);
	if (endptr == str || *endptr != '\0') {
		pr_err("'%s' is not a valid floating-point number\r\n", str);
		return CLI_ERR_DOUBLE_FMT;
	}
	if (errno == ERANGE) {
		pr_err("'%s' out of floating-point range\r\n", str);
		return CLI_ERR_DOUBLE_RANGE;
	}
	*out = val;
	return CLI_OK;
}
