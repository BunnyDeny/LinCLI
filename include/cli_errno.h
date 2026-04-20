/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_ERRNO_H_
#define _CLI_ERRNO_H_

#include "cli_errno_common.h"
#include "cli_errno_statem.h"
#include "cli_errno_dispose.h"
#include "cli_errno_io.h"

const char *cli_strerror(int err);

#endif
