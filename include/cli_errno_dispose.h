/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_ERRNO_DISPOSE_H_
#define _CLI_ERRNO_DISPOSE_H_

#include "cli_errno_common.h"

/* ========== cmd_dispose / 解析模块 (-20 ~ -39) ========== */
#define CLI_ERR_UNKNOWN_OPT   -20   /* 未知选项 */
#define CLI_ERR_DUP_OPT       -21   /* 重复选项 */
#define CLI_ERR_MISSING_ARG   -22   /* 选项缺少参数 */
#define CLI_ERR_REQ_OPT       -23   /* 缺少必需选项 */
#define CLI_ERR_CONFLICT      -24   /* 互斥选项冲突 */
#define CLI_ERR_DEP_MISSING   -25   /* 依赖选项未提供 */
#define CLI_ERR_ORPHAN_ARG    -26   /* 孤立参数（非选项字符串） */
#define CLI_ERR_INT_RANGE     -27   /* 整数超出范围 */
#define CLI_ERR_INT_FMT       -28   /* 整数格式错误 */
#define CLI_ERR_DOUBLE_RANGE  -29   /* 浮点数超出范围 */
#define CLI_ERR_DOUBLE_FMT    -30   /* 浮点数格式错误 */
#define CLI_ERR_ARRAY_MAX     -31   /* 数组元素超过上限 */
#define CLI_ERR_BUF_INSUFF    -32   /* 缓冲区不足 */

#endif
