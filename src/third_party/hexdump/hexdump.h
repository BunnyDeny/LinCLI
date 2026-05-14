/*
 * LinCLI - Lightweight hexdump utility for embedded/MCU debugging.
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
#ifndef _HEXDUMP_H_
#define _HEXDUMP_H_

#include <stdint.h>
#include <stddef.h>

typedef uint8_t (*hexdump_read_fn_t)(uintptr_t addr);

struct hexdump_config {
	uintptr_t min_addr;
	uintptr_t max_addr;
	size_t bytes_per_line;
	size_t max_len;
};

void hexdump_set_read_fn(hexdump_read_fn_t fn);
hexdump_read_fn_t hexdump_get_read_fn(void);
struct hexdump_config *hexdump_get_config(void);

#endif
