/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "cli_errno.h"

#ifndef CLI_DEBUG
const char *cli_strerror(int err)
{
	(void)err;
	return "error";
}
#else
const char *cli_strerror(int err)
{
	switch (err) {
	case CLI_OK:
		return "OK";
	case CLI_ERR_NULL:
		return "null pointer";
	case CLI_ERR_NOTFOUND:
		return "not found";
	case CLI_ERR_INVAL:
		return "invalid argument";
	case CLI_ERR_STATEM_EMPTY:
		return "state pool empty";
	case CLI_ERR_STATEM_SAME:
		return "already in target state";
	case CLI_ERR_UNKNOWN_OPT:
		return "unknown option";
	case CLI_ERR_DUP_OPT:
		return "duplicate option";
	case CLI_ERR_MISSING_ARG:
		return "option missing argument";
	case CLI_ERR_REQ_OPT:
		return "missing required option";
	case CLI_ERR_CONFLICT:
		return "conflicting options";
	case CLI_ERR_DEP_MISSING:
		return "missing dependency option";
	case CLI_ERR_ORPHAN_ARG:
		return "orphan argument";
	case CLI_ERR_INT_RANGE:
		return "integer out of range";
	case CLI_ERR_INT_FMT:
		return "invalid integer format";
	case CLI_ERR_DOUBLE_RANGE:
		return "double out of range";
	case CLI_ERR_DOUBLE_FMT:
		return "invalid double format";
	case CLI_ERR_ARRAY_MAX:
		return "array exceeds max size";
	case CLI_ERR_BUF_INSUFF:
		return "insufficient buffer";
	case CLI_ERR_FIFO_FULL:
		return "FIFO full";
	case CLI_ERR_FIFO_EMPTY:
		return "FIFO empty";
	case CLI_ERR_IO_SYNC:
		return "I/O sync failed";
	default:
		return "unknown error";
	}
}
#endif
