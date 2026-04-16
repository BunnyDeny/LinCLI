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

#include "tVector.h"

void vectorInit(struct vector *v, _u8 *buf, int buf_size)
{
	v->buf_size = buf_size;
	v->_buf = buf;
	v->head = 0;
	v->tail = -1;
	v->size = 0;
}

bool at(struct vector *v, int pos, _u8 *data)
{
	if (pos >= v->size || pos < 0) {
		return false;
	}
	*data = v->_buf[(v->head + pos) % v->buf_size];
	return true;
}

bool pop_front(struct vector *v, int n)
{
	if (v->size < n || n <= 0) {
		return false;
	}
	v->head = (v->head + n) % v->buf_size;
	v->size -= n;
	return true;
}

bool pop_back(struct vector *v, int n)
{
	if (v->size < n || n <= 0) {
		return false;
	}
	v->tail = (v->tail - n + v->buf_size) % v->buf_size;
	v->size -= n;
	return true;
}

bool push_font(struct vector *v, _u8 *date, int n)
{
	if (v->size + n > v->buf_size || n <= 0) {
		return false;
	}
	for (int i = n - 1; i >= 0; --i) {
		v->head = (v->head - 1 + v->buf_size) % v->buf_size;
		v->_buf[v->head] = date[i];
	}
	v->size += n;
	return true;
}

bool push_back(struct vector *v, _u8 *date, int n)
{
	if (v->size + n > v->buf_size || n <= 0) {
		return false;
	}
	for (int i = 0; i < n; ++i) {
		v->tail = (v->tail + 1) % v->buf_size;
		v->_buf[v->tail] = date[i];
	}
	v->size += n;
	return true;
}
