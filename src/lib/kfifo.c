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

#include "kfifo.h"

void kfifo_init(kfifo_t *fifo, uint8_t *buffer, uint32_t size)
{
	fifo->buffer = buffer;
	fifo->size = size;
	fifo->mask = size - 1;
	fifo->in = 0;
	fifo->out = 0;
}

uint32_t kfifo_put(kfifo_t *fifo, const uint8_t *data, uint32_t len)
{
	uint32_t l;

	len = len < (fifo->size - (fifo->in - fifo->out)) ?
	      len : (fifo->size - (fifo->in - fifo->out));

	smp_mb();

	l = len < (fifo->size - (fifo->in & fifo->mask)) ?
	    len : (fifo->size - (fifo->in & fifo->mask));

	memcpy(fifo->buffer + (fifo->in & fifo->mask), data, l);
	memcpy(fifo->buffer, data + l, len - l);

	smp_wmb();
	fifo->in += len;

	return len;
}

uint32_t kfifo_get(kfifo_t *fifo, uint8_t *data, uint32_t len)
{
	uint32_t l;

	len = len < (fifo->in - fifo->out) ? len : (fifo->in - fifo->out);

	smp_rmb();

	l = len < (fifo->size - (fifo->out & fifo->mask)) ?
	    len : (fifo->size - (fifo->out & fifo->mask));

	memcpy(data, fifo->buffer + (fifo->out & fifo->mask), l);
	memcpy(data + l, fifo->buffer, len - l);

	smp_mb();
	fifo->out += len;

	return len;
}
