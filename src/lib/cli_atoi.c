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
	int base = 10;

	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '-') {
		sign = -1;
		p++;
	} else if (*p == '+') {
		p++;
	}

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		base = 16;
		p += 2;
	}

	while (1) {
		char c = *p;
		int digit = -1;

		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (base == 16 && c >= 'a' && c <= 'f')
			digit = 10 + (c - 'a');
		else if (base == 16 && c >= 'A' && c <= 'F')
			digit = 10 + (c - 'A');

		if (digit < 0 || digit >= base)
			break;

		val = val * base + digit;
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
