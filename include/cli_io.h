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

void cli_io_init(void);

static inline int _cli_io_push(struct vector *v, _u8 *data, int size, _u8 *ref)
{
	if (*ref > 1)
		return *ref;
	(*ref)++;
	return push_back(v, data, size) ? 0 : -1;
}

static inline int _cli_io_pop(struct vector *v, _u8 *data, int size, _u8 *ref)
{
	if (*ref > 1)
		return *ref;
	(*ref)++;
	int remain_to_pop = size;
	while (remain_to_pop) {
		_u8 front;
		if (at(v, 0, &front) == false) {
			return -1;
		}
		int idx = size - remain_to_pop;
		*(data + idx) = front;
		remain_to_pop--;
	}
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
	return _cli_io_pop(&_cli_io.in, data, size, &_cli_io.in_push_ref);
}

static inline int cli_out_pop(_u8 *data, int size)
{
	return _cli_io_pop(&_cli_io.out, data, size, &_cli_io.out_push_ref);
}

#endif
