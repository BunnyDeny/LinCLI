#ifndef TVECTOR_H
#define TVECTOR_H

#include <stdbool.h>
#include <stdint.h>

#if defined(_u8)
#undef _u8
typedef volatile uint8_t _u8;
#else
typedef volatile uint8_t _u8;
#endif

#if defined(_int)
#undef _int
typedef volatile int _int;
#else
typedef volatile int _int;
#endif

struct vector {
	_int buf_size;
	_int head;
	_int tail;
	_int size;
	_u8 *_buf;
};

void vectorInit(struct vector *v, _u8 *buf, int buf_size);
_u8 at(struct vector *v, int pos);
bool pop_front(struct vector *v, int n);
bool pop_back(struct vector *v, int n);
bool push_font(struct vector *v, _u8 *date, int n);
bool push_back(struct vector *v, _u8 *date, int n);

#endif
