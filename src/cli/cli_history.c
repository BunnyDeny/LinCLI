/*
 * LinCLI - Command line history management.
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

#include "cli_history.h"
#include <string.h>

/* 命令历史记录 */

struct history history = {
	.count = 0,
	.index = 0,
};

void history_save(const char *cmd, int size)
{
	if (size <= 0)
		return;

	if (history.count > 0 && (int)strlen(history.buf[0]) == size &&
	    memcmp(history.buf[0], cmd, size) == 0) {
		history.index = 0;
		return;
	}

	for (int i = HISTORY_MAX - 1; i > 0; i--) {
		memcpy(history.buf[i], history.buf[i - 1], CMD_LINE_BUF_SIZE);
	}
	memset(history.buf[0], 0, CMD_LINE_BUF_SIZE);
	memcpy(history.buf[0], cmd, size);

	if (history.count < HISTORY_MAX)
		history.count++;
	history.index = 0;
}

