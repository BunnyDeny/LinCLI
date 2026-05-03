/*
 * LinCLI - Lightweight float conversion for embedded targets.
 */

#ifndef _CLI_FLOAT_H_
#define _CLI_FLOAT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* 字符串转 float，endptr 语义同 strtod */
float cli_atof(const char *str, char **endptr);

/* float 转字符串，prec 为小数位数（0~9），返回写入长度 */
int cli_ftoa(float val, char *buf, int buf_size, int prec);

#ifdef __cplusplus
}
#endif

#endif
