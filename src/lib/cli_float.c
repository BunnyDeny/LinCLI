/*
 * LinCLI - Lightweight float conversion for embedded targets.
 */

#include "cli_float.h"

float cli_atof(const char *str, char **endptr)
{
	const char *p = str;
	float val = 0.0f;
	float factor = 0.1f;
	int sign = 1;
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
		val = val * 10.0f + (*p - '0');
		p++;
		has_digit = 1;
	}

	if (*p == '.') {
		p++;
		while (*p >= '0' && *p <= '9') {
			val += (*p - '0') * factor;
			factor *= 0.1f;
			p++;
			has_digit = 1;
		}
	}

	if (endptr)
		*endptr = (char *)(has_digit ? p : str);

	return sign * val;
}

int cli_ftoa(float val, char *buf, int buf_size, int prec)
{
	int len = 0;
	long intpart;
	float frac;
	int i;
	char tmp[32];
	int t;

	if (prec < 0)
		prec = 6;
	if (prec > 9)
		prec = 9;

	if (buf_size <= 0)
		return 0;

	if (val < 0.0f) {
		if (len < buf_size - 1)
			buf[len++] = '-';
		val = -val;
	}

	/* 四舍五入 */
	float round = 0.5f;
	for (i = 0; i < prec; i++)
		round *= 0.1f;
	val += round;

	intpart = (long)val;
	frac = val - (float)intpart;

	/* 整数部分 */
	t = 0;
	if (intpart == 0) {
		tmp[t++] = '0';
	} else {
		while (intpart > 0) {
			tmp[t++] = '0' + (intpart % 10);
			intpart /= 10;
		}
	}
	while (t > 0 && len < buf_size - 1)
		buf[len++] = tmp[--t];

	/* 小数部分 */
	if (prec > 0 && len < buf_size - 1)
		buf[len++] = '.';

	for (i = 0; i < prec && len < buf_size - 1; i++) {
		frac *= 10.0f;
		int digit = (int)frac;
		if (digit < 0)
			digit = 0;
		if (digit > 9)
			digit = 9;
		buf[len++] = '0' + digit;
		frac -= digit;
	}

	buf[len] = '\0';
	return len;
}
