/*
 * LinCLI - Common string-to-value parsers.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 */

#include "cli_parse.h"
#include "cli_errno.h"
#include "cli_io.h"
#include "cli_float.h"
#include "cli_atoi.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int cli_parse_int(const char *str, int *out)
{
	char *endptr;
	if (cli_atoi(str, out, &endptr) < 0 || *endptr != '\0') {
		pr_err("'%s' is not a valid integer\r\n", str);
		return CLI_ERR_INT_FMT;
	}
	return CLI_OK;
}

int cli_parse_float(const char *str, float *out)
{
	char *endptr;
	float val = cli_atof(str, &endptr);
	if (endptr == (char *)str || *endptr != '\0') {
		pr_err("'%s' is not a valid floating-point number\r\n", str);
		return CLI_ERR_FLOAT_FMT;
	}
	*out = val;
	return CLI_OK;
}
