/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
bool at(struct vector *v, int pos, _u8 *data);
bool pop_front(struct vector *v, int n);
bool pop_back(struct vector *v, int n);
bool push_font(struct vector *v, _u8 *date, int n);
bool push_back(struct vector *v, _u8 *date, int n);

#endif
