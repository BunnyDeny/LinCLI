/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

bool push_front(struct vector *v, _u8 *date, int n)
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
