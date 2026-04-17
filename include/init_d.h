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

#ifndef _INIT_D__H__
#define _INIT_D__H__

#include <stdint.h>

#define INIT_D_MAGIC 0x494E4931U

struct init_d {
	uint32_t magic;
	char name[32];
	void *_private;
	void (*_init_entry)(void *);
} __attribute__((aligned(sizeof(long))));

#define _EXPORT_INIT_SYMBOL(obj, private, init_entry)                    \
	static struct init_d init_d_##obj __attribute__((                \
		used, section(".my_init_d"), aligned(sizeof(long)))) = { \
		.magic = INIT_D_MAGIC,                                   \
		.name = #obj,                                            \
		._private = private,                                     \
		._init_entry = init_entry,                               \
	}

extern struct init_d _init_d_start;
extern struct init_d _init_d_end;

#define _FOR_EACH_INIT_D(_start, _end, _init_d) \
	for (uintptr_t _addr = (uintptr_t)(_start), _end_addr = (uintptr_t)(_end); \
	     _addr + sizeof(struct init_d) <= _end_addr; \
	     _addr += sizeof(long)) \
		if (((_init_d) = (struct init_d *)(_addr))->magic == INIT_D_MAGIC)

#define CALL_INIT_D                                                        \
	do {                                                               \
		struct init_d *p_init_d;                                   \
		_FOR_EACH_INIT_D(&_init_d_start, &_init_d_end, p_init_d)   \
		{                                                          \
			if (p_init_d && p_init_d->_init_entry) {           \
				p_init_d->_init_entry(p_init_d->_private); \
			}                                                  \
		}                                                          \
	} while (0)

#endif
