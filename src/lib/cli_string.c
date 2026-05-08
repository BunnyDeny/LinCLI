/*
 * Minimal string/memory functions to replace newlib nano.
 * Pure byte-wise loops — smaller than newlib's alignment-optimized versions.
 *
 * Functions are defined with cli_ prefix and exposed via alias attributes
 * to avoid direct name clashes with newlib.
 */

#include <stddef.h>

void *cli_memcpy(void *dst, const void *src, size_t n)
{
	char *d = dst;
	const char *s = src;
	while (n--)
		*d++ = *s++;
	return dst;
}

void *cli_memset(void *s, int c, size_t n)
{
	char *p = s;
	while (n--)
		*p++ = (char)c;
	return s;
}

int cli_memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = s1, *p2 = s2;
	while (n--)
		if (*p1++ != *p2++)
			return (int)p1[-1] - (int)p2[-1];
	return 0;
}

size_t cli_strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return (size_t)(p - s);
}

int cli_strcmp(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return (int)(*(unsigned char *)s1) - (int)(*(unsigned char *)s2);
}

char *cli_strcpy(char *dst, const char *src)
{
	char *d = dst;
	while (*src) {
		*d++ = *src++;
	}
	*d = '\0';
	return dst;
}

char *cli_strncpy(char *dst, const char *src, size_t n)
{
	char *d = dst;
	while (n && *src) {
		*d++ = *src++;
		n--;
	}
	if (n)
		*d++ = *src;
	while (n--)
		*d++ = '\0';
	return dst;
}

char *cli_strcat(char *dst, const char *src)
{
	char *d = dst;
	while (*d)
		d++;
	while (*src) {
		*d++ = *src++;
	}
	*d = '\0';
	return dst;
}

int cli_strncmp(const char *s1, const char *s2, size_t n)
{
	while (n && *s1 && *s1 == *s2) {
		s1++;
		s2++;
		n--;
	}
	if (n == 0)
		return 0;
	return (int)(*(unsigned char *)s1) - (int)(*(unsigned char *)s2);
}

char *cli_strchr(const char *s, int c)
{
	char ch = (char)c;
	while (*s) {
		if (*s == ch)
			return (char *)s;
		s++;
	}
	return ch == '\0' ? (char *)s : NULL;
}

void *cli_memmove(void *dst, const void *src, size_t n)
{
	char *d = dst;
	const char *s = src;
	if (d < s) {
		while (n--)
			*d++ = *s++;
	} else if (d > s) {
		d += n;
		s += n;
		while (n--)
			*--d = *--s;
	}
	return dst;
}

/* Expose standard C names as aliases */
__attribute__((alias("cli_memcpy"))) void *memcpy(void *dst, const void *src,
						  size_t n);
__attribute__((alias("cli_memset"))) void *memset(void *s, int c, size_t n);
__attribute__((alias("cli_memcmp"))) int memcmp(const void *s1, const void *s2,
						size_t n);
__attribute__((alias("cli_strlen"))) size_t strlen(const char *s);
__attribute__((alias("cli_strcmp"))) int strcmp(const char *s1, const char *s2);
__attribute__((alias("cli_strcpy"))) char *strcpy(char *dst, const char *src);
__attribute__((alias("cli_strncpy"))) char *strncpy(char *dst, const char *src,
						    size_t n);
__attribute__((alias("cli_strcat"))) char *strcat(char *dst, const char *src);
__attribute__((alias("cli_strncmp"))) int strncmp(const char *s1,
						  const char *s2, size_t n);
__attribute__((alias("cli_strchr"))) char *strchr(const char *s, int c);
__attribute__((alias("cli_memmove"))) void *memmove(void *dst, const void *src,
						    size_t n);
