#ifndef _CLI_HISTORY_H_
#define _CLI_HISTORY_H_

#include "cli_cmd_line.h"
#include "cli_config.h"
#include "cli_io.h"

struct history {
	char buf[HISTORY_MAX][CMD_LINE_BUF_SIZE];
	_u8 count;
	_u8 index;
};

extern struct history history;

void history_save(const char *cmd, int size);

#endif
