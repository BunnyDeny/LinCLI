/*
 * LinCLI - Command line history management.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

