/*
 * LinCLI - Lightweight string-to-int parser (32/64-bit compatible).
 */

#include "cli_atoi.h"

int cli_atoi(const char *str, int *out, char **endptr)
{
	const char *p = str;
	int sign = 1;
	long val = 0;
	int has_digit = 0;

	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '-') {
		sign = -1;
		p++;
	} else if (*p == '+') {
		p++;
	}

	while (*p >= '0' && *p <= '9') {
		val = val * 10 + (*p - '0');
		p++;
		has_digit = 1;
	}

	if (endptr)
		*endptr = (char *)(has_digit ? p : str);

	if (!has_digit) {
		*out = 0;
		return -1;
	}

	*out = (int)(sign * val);
	return 0;
}
