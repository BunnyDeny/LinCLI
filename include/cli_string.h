/*
 * LinCLI - Minimal string/memory functions (newlib nano replacement).
 */

#ifndef _CLI_STRING_H_
#define _CLI_STRING_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *cli_memcpy(void *dst, const void *src, size_t n);
void *cli_memset(void *s, int c, size_t n);
int cli_memcmp(const void *s1, const void *s2, size_t n);
size_t cli_strlen(const char *s);
int cli_strcmp(const char *s1, const char *s2);
char *cli_strcpy(char *dst, const char *src);
char *cli_strncpy(char *dst, const char *src, size_t n);
char *cli_strcat(char *dst, const char *src);
int cli_strncmp(const char *s1, const char *s2, size_t n);
char *cli_strchr(const char *s, int c);
void *cli_memmove(void *dst, const void *src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
