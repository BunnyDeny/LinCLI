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

#ifndef _INIT_D__H__
#define _INIT_D__H__

#include <stddef.h>

struct init_d {
	int priority;
	void *_private;
	void (*_init_entry)(void *);
};

#define _EXPORT_INIT_SYMBOL(obj, _priority, private, init_entry) \
	static struct init_d init_d_##obj = {                    \
		.priority = _priority,                           \
		._private = private,                             \
		._init_entry = init_entry,                       \
	};                                                       \
	static struct init_d *const _init_d_ptr_##obj            \
		__attribute__((used, section(".my_init_d.1"))) = &init_d_##obj

extern struct init_d *const _init_d_start[];
extern struct init_d *const _init_d_end[];

#define _FOR_EACH_INIT_D(_init_d)                         \
	for (struct init_d *const *_pp = _init_d_start;   \
	     _pp < (struct init_d *const *)_init_d_end; _pp++) \
		if (((_init_d) = *_pp) != NULL)

extern void call_init_d(void);

#define CALL_INIT_D            \
	do {                   \
		call_init_d(); \
	} while (0)

#endif
