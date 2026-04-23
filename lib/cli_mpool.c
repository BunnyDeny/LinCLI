/*
 * LinCLI - Memory pool for framework internal use.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "cli_mpool.h"
#include "cli_critical.h"

static uint8_t pool[CLI_MPOOL_COUNT][CLI_MPOOL_SIZE];
static volatile uint32_t bitmap = 0;

void *cli_mpool_alloc(void)
{
	cli_enter_critical();
	for (int i = 0; i < CLI_MPOOL_COUNT; i++) {
		if (!(bitmap & (1U << i))) {
			bitmap |= (1U << i);
			cli_exit_critical();
			return &pool[i][0];
		}
	}
	cli_exit_critical();
	return NULL;
}

void cli_mpool_free(void *ptr)
{
	if (ptr == NULL)
		return;

	uintptr_t addr = (uintptr_t)ptr;
	uintptr_t base = (uintptr_t)&pool[0][0];
	size_t offset;
	int idx;

	if (addr < base)
		return;

	offset = addr - base;
	if (offset % CLI_MPOOL_SIZE != 0)
		return;

	idx = (int)(offset / CLI_MPOOL_SIZE);
	if (idx < 0 || idx >= CLI_MPOOL_COUNT)
		return;

	cli_enter_critical();
	bitmap &= ~(1U << idx);
	cli_exit_critical();
}
