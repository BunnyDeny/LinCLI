/*
 * LinCLI - Memory pool usage monitor command.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "cli_config.h"
#include "cmd_dispose.h"
#include "cli_io.h"
#include "cli_mpool.h"

static int memstat_handler(void *_args)
{
	(void)_args;
	const char *owners[CLI_MPOOL_COUNT];
	int used_count = 0;

	cli_mpool_get_usage(owners, &used_count);

	cli_printk("Memory pool: %d/%d blocks used\r\n", used_count,
		   CLI_MPOOL_COUNT);
	for (int i = 0; i < used_count; i++) {
		cli_printk("  [%d] %s\r\n", i,
			   owners[i] ? owners[i] : "unknown");
	}
	if (used_count == 0)
		cli_printk("  (all free)\r\n");
	return 0;
}

CLI_COMMAND_NO_STRUCT(memstat, "memstat", "Show memory pool usage",
			memstat_handler);
