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

struct init_d {
	char name[32];
	void *_private;
	void (*_init_entry)(void *);
};

#define _EXPORT_INIT_SYMBOL(obj, private, init_entry) \
	static struct init_d init_d_##obj = {         \
		.name = #obj,                         \
		._private = private,                  \
		._init_entry = init_entry,            \
	};                                            \
	static struct init_d *const _init_d_ptr_##obj \
		__attribute__((used, section(".my_init_d"))) = &init_d_##obj

extern struct init_d *const _init_d_start[];
extern struct init_d *const _init_d_end[];

#define _FOR_EACH_INIT_D(_start, _end, _init_d)           \
	for (struct init_d *const *_pp = (_start);        \
	     _pp < (struct init_d *const *)(_end); _pp++) \
		if (((_init_d) = *_pp))

#define CALL_INIT_D                                                        \
	do {                                                               \
		struct init_d *p_init_d;                                   \
		_FOR_EACH_INIT_D(_init_d_start, _init_d_end, p_init_d)     \
		{                                                          \
			if (p_init_d && p_init_d->_init_entry) {           \
				p_init_d->_init_entry(p_init_d->_private); \
			}                                                  \
		}                                                          \
	} while (0)

#endif
