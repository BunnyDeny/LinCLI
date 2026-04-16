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

void pr_logo(void *arg)
{
	cli_printk(" _     _        ____ _     ___ \n");
	cli_printk("| |   (_)_ __  / ___| |   |_ _|\n");
	cli_printk("| |   | | '_ \\| |   | |    | | \n");
	cli_printk("| |___| | | | | |___| |___ | | \n");
	cli_printk("|_____|_|_| |_|\\____|_____|___|\n");

	pr_info("欢迎使用LinCLI\n");
}

_EXPORT_INIT_SYMBOL(logo, NULL, pr_logo);