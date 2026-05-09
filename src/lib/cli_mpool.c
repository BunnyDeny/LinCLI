/*
 * LinCLI - Memory pool for framework internal use.
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

#include "cli_mpool.h"
#include "cli_critical.h"

static uint8_t pool[CLI_MPOOL_COUNT][CLI_MPOOL_SIZE];
static volatile uint32_t bitmap = 0;
static const char *alloc_owner[CLI_MPOOL_COUNT] = { NULL };

void *cli_mpool_alloc_caller(const char *caller)
{
	cli_enter_critical();
	for (int i = 0; i < CLI_MPOOL_COUNT; i++) {
		if (!(bitmap & (1U << i))) {
			bitmap |= (1U << i);
			alloc_owner[i] = caller;
			cli_exit_critical();
			return &pool[i][0];
		}
	}
	cli_exit_critical();
	cli_mpool_dump_usage();
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
	alloc_owner[idx] = NULL;
	cli_exit_critical();
}

void cli_mpool_get_usage(const char **owners, int *used_count)
{
	int count = 0;
	cli_enter_critical();
	for (int i = 0; i < CLI_MPOOL_COUNT; i++) {
		if (bitmap & (1U << i)) {
			if (owners)
				owners[count] = alloc_owner[i];
			count++;
		}
	}
	cli_exit_critical();
	if (used_count)
		*used_count = count;
}

