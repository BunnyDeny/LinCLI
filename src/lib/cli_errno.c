/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
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
	case CLI_ERR_FLOAT_RANGE:
		return "float out of range";
	case CLI_ERR_FLOAT_FMT:
		return "invalid float format";
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
