/*
 * LinCLI - Lightweight string-to-int parser.
 */

#ifndef _CLI_ATOI_H_
#define _CLI_ATOI_H_

#ifdef __cplusplus
extern "C" {
#endif

int cli_atoi(const char *str, int *out, char **endptr);

#ifdef __cplusplus
}
#endif

#endif
