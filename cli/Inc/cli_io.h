#ifndef _CLI_IO_H
#define _CLI_IO_H

#include "tVector.h"

#define CLI_IO_SIZE 128

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
