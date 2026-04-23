/*
 * LinCLI - Critical section hooks for cross-module synchronization.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CLI_CRITICAL_H_
#define _CLI_CRITICAL_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 临界区保护钩子。
 * 在 MCU 中通常映射为 __disable_irq / __restore_irq；
 * 在 PC 模拟中通常映射为原子自旋锁。
 *
 * 默认实现为空函数（weak），移植层覆盖即可。
 */
void cli_enter_critical(void);
void cli_exit_critical(void);

#ifdef __cplusplus
}
#endif

#endif
