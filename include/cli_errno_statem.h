/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_ERRNO_STATEM_H_
#define _CLI_ERRNO_STATEM_H_

#include "cli_errno_common.h"

/* ========== stateM 模块 (-10 ~ -19) ========== */
#define CLI_ERR_STATEM_EMPTY  -10   /* 状态池为空 */
#define CLI_ERR_STATEM_SAME   -11   /* 已在目标状态，无需切换 */

/* 本模块的系统级错误集合 */
#define CLI_ERR_STATEM_SYSTEM(X) \
	X(CLI_ERR_STATEM_EMPTY)    \
	X(CLI_ERR_STATEM_DUP)      \
	X(CLI_ERR_STATEM_SAME)

#endif
