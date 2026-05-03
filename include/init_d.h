/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
