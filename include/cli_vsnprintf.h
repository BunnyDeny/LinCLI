/*
 * LinCLI - Lightweight vsnprintf for embedded targets.
 */

#ifndef _CLI_VSNPRINTF_H_
#define _CLI_VSNPRINTF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

int cli_vsnprintf(char *buf, int buf_size, const char *fmt, va_list args);
int cli_snprintf(char *buf, int buf_size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
