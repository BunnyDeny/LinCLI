/*
 * LinCLI - Framework-wide configuration macros.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_CONFIG_H_
#define _CLI_CONFIG_H_

#include "cli_kconfig.h"

/* ==========================================================
 * 以下宏与 Kconfig 无关，始终保留。
 * ========================================================== */
#define CLI_VERSION_MAJOR 1
#define CLI_VERSION_MINOR 12
#define CLI_VERSION_PATCH 22

#if CLI_MPOOL_COUNT > 32
#error "CLI_MPOOL_COUNT must not exceed 32"
#endif

#endif /* _CLI_CONFIG_H_ */
