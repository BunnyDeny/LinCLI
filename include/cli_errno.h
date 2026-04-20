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

#include <stdbool.h>
#include "cli_errno_common.h"
#include "cli_errno_statem.h"
#include "cli_errno_dispose.h"
#include "cli_errno_io.h"

const char *cli_strerror(int err);

/* ========== 错误分类辅助函数 ==========
 *
 * LinCLI 的错误码从架构上分为两类：
 *
 * 1. 系统级错误（System Errors）
 *    含义：框架内部异常、配置错误、资源耗尽或运行环境异常。
 *    处理原则：出现这类错误时，系统可能无法继续正常工作，通常需要
 *    开发者介入修复，或进入安全状态/复位。
 *    典型错误：CLI_ERR_NULL, CLI_ERR_STATEM_EMPTY, CLI_ERR_FIFO_FULL
 *
 * 2. 用户级错误（User Errors）
 *    含义：用户输入不符合命令定义或业务规则。
 *    处理原则：提示用户具体原因，等待下一次输入即可，系统本身无异常。
 *    典型错误：CLI_ERR_UNKNOWN_OPT, CLI_ERR_REQ_OPT, CLI_ERR_CONFLICT
 *
 * 当前错误码分布：
 * - cli_errno_common.h / cli_errno_statem.h / cli_errno_io.h 中的错误
 *   绝大多数属于系统级。
 * - cli_errno_dispose.h 中的错误全部属于用户级。
 */

/* ========== 系统级错误集合（X-Macro）==========
 *
 * 使用 X-Macro 技巧维护系统级错误列表。
 * 新增系统级错误时，只需在下面列表中加一行，
 * cli_err_is_system() 会自动同步，无需手动维护 switch-case。
 */
#define CLI_SYSTEM_ERRORS(X) \
	X(CLI_ERR_NULL)        \
	X(CLI_ERR_NOMEM)       \
	X(CLI_ERR_STATEM_EMPTY)\
	X(CLI_ERR_STATEM_DUP)  \
	X(CLI_ERR_STATEM_SAME) \
	X(CLI_ERR_FIFO_FULL)   \
	X(CLI_ERR_FIFO_EMPTY)  \
	X(CLI_ERR_IO_SYNC)

/**
 * @brief 判断错误码是否为系统级错误。
 *
 * @param err 错误码（负数）。
 * @return true  属于系统级错误，调用方应视严重程度考虑日志级别
 *               和是否需要进入安全状态。
 * @return false 属于用户级错误或未知错误，通常只需提示用户重新输入。
 */
static inline bool cli_err_is_system(int err)
{
	switch (err) {
#define _CLI_ERR_CASE(e) case e: return true;
	CLI_SYSTEM_ERRORS(_CLI_ERR_CASE)
#undef _CLI_ERR_CASE
	default:
		return false;
	}
}

#endif
