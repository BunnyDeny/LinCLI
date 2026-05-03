/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "cli_io.h"
#include "init_d.h"
#include "cmd_dispose.h"
#include "cli_config.h"
#include "cli_env.h"

static void pr_builtin_envs(void)
{
	cli_env_t *env;
	all_printk("Env:\r\n");
	_FOR_EACH_CLI_ENV(_cli_envs_start, _cli_envs_end, env)
	{
		if (!env || !env->name || env->id < 0)
			continue;
		all_printk("%d %s=%s\r\n", env->id, env->name,
			   cli_env_get_value(env));
	}
	all_printk("\r\n");
}

void pr_logo(void *arg)
{
	all_printk("LinCLI v%d.%d.%d\r\n",
		CLI_VERSION_MAJOR, CLI_VERSION_MINOR, CLI_VERSION_PATCH);
	all_printk("Type 'help' to start.\r\n");
}

_EXPORT_INIT_SYMBOL(logo, 21, NULL, pr_logo);
