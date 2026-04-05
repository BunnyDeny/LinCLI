#ifndef _CLI_IO_H
#define _CLI_IO_H

#include "tVector.h"

#define CLI_IO_SIZE 128
#define COLOR_TERMINAL_EN 1

#if COLOR_TERMINAL_EN
#define COLOR_NONE "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RAINBOW_1 "\033[38;5;196m" /* 红 */
#define COLOR_RAINBOW_2 "\033[38;5;208m" /* 橙红 */
#define COLOR_RAINBOW_3 "\033[38;5;214m" /* 橙 */
#define COLOR_RAINBOW_4 "\033[38;5;226m" /* 黄 */
#define COLOR_RAINBOW_5 "\033[38;5;046m" /* 绿 */
#define COLOR_RAINBOW_6 "\033[38;5;051m" /* 青 */
#define COLOR_RAINBOW_7 "\033[38;5;033m" /* 蓝 */
#define COLOR_RAINBOW_8 "\033[38;5;129m" /* 紫 */
#define COLOR_RAINBOW_9 "\033[38;5;201m" /* 洋红 */
#else
#define COLOR_NONE
#define COLOR_RED
#define COLOR_GREEN
#define COLOR_YELLOW
#define COLOR_BLUE
#define COLOR_MAGENTA
#define COLOR_CYAN
#define COLOR_WHITE
#define COLOR_BOLD
#define COLOR_RAINBOW_1
#define COLOR_RAINBOW_2
#define COLOR_RAINBOW_3
#define COLOR_RAINBOW_4
#define COLOR_RAINBOW_5
#define COLOR_RAINBOW_6
#define COLOR_RAINBOW_7
#define COLOR_RAINBOW_8
#define COLOR_RAINBOW_9
#endif

#define KERN_EMERG "0"
#define KERN_ALERT "1"
#define KERN_CRIT "2"
#define KERN_ERR "3"
#define KERN_WARNING "4"
#define KERN_NOTICE "5"
#define KERN_INFO "6"
#define KERN_DEBUG "7"
#define KERN_DEFAULT ""
#define KERN_CONT "c"

struct cli_io {
	struct vector in;
	_u8 in_push_ref;
	_u8 in_pop_ref;
	char in_buf[CLI_IO_SIZE];

	struct vector out;
	_u8 out_push_ref;
	_u8 out_pop_ref;
	char out_buf[CLI_IO_SIZE];
};

extern struct cli_io _cli_io;

static inline int _cli_io_push(struct vector *v, _u8 *data, int size, _u8 *ref)
{
	bool status;
	if (*ref > 2) {
		return *ref;
	}
	if (*ref == 0) {
		return -2; /*uninited*/
	}
	(*ref)++;
	status = push_back(v, data, size);
	(*ref)--;
	return status ? 0 : -1;
}

static inline int _cli_io_pop(struct vector *v, _u8 *data, int size, _u8 *ref)
{
	if (*ref > 2) {
		return *ref;
	}
	if (*ref == 0) {
		return -2; /*uninited*/
	}
	(*ref)++;
	int remain_to_pop = size;
	while (remain_to_pop) {
		_u8 front;
		if (at(v, 0, &front) == false) {
			(*ref)--;
			return -1;
		}
		int idx = size - remain_to_pop;
		*(data + idx) = front;
		pop_front(v, 1);
		remain_to_pop--;
	}
	(*ref)--;
	return 0;
}

static inline int cli_in_push(_u8 *data, int size)
{
	return _cli_io_push(&_cli_io.in, data, size, &_cli_io.in_push_ref);
}

static inline int cli_out_push(_u8 *data, int size)
{
	return _cli_io_push(&_cli_io.out, data, size, &_cli_io.out_push_ref);
}

static inline int cli_in_pop(_u8 *data, int size)
{
	return _cli_io_pop(&_cli_io.in, data, size, &_cli_io.in_pop_ref);
}

static inline int cli_out_pop(_u8 *data, int size)
{
	return _cli_io_pop(&_cli_io.out, data, size, &_cli_io.out_pop_ref);
}

static inline int cli_get_in_size(void)
{
	return _cli_io.in.size;
}

static inline int cli_get_out_size(void)
{
	return _cli_io.out.size;
}

void cli_io_init(void);
int _cli_io_printf(const char *fmt, ...);

#endif
