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

#include "init_d.h"
#include "cli_io.h"
#include "cmd_dispose.h"

void pr_license(void *arg)
{
	cli_printk("LinCLI  Copyright (C) 2026  bunnydeny\r\n");
	cli_printk(
		"This program comes with ABSOLUTELY NO WARRANTY; for details type `show -w'.\r\n");
	cli_printk(
		"This is free software, and you are welcome to redistribute it\r\n");
	cli_printk(
		"under certain conditions; type `show -c' for details.\n\r\n");
}
_EXPORT_INIT_SYMBOL(pr_license, 10, NULL, pr_license);

struct show_args {
	bool warranty;
	bool conditions;
};

static int show_handler(void *_args)
{
	struct show_args *args = _args;

	if (args->warranty == false && args->conditions == false) {
		pr_err("please input the option for command show\r\n");
		return -1;
	}

	if (args->warranty) {
		cli_printk(
			"THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY\r\n");
		cli_printk(
			"APPLICABLE LAW. EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT\r\n");
		cli_printk(
			"HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY\r\n");
		cli_printk(
			"OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,\r\n");
		cli_printk(
			"THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\r\n");
		cli_printk(
			"PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM\r\n");
		cli_printk(
			"IS WITH YOU. SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF\r\n");
		cli_printk(
			"ALL NECESSARY SERVICING, REPAIR OR CORRECTION.\r\n");
	}

	if (args->conditions) {
		cli_printk(
			"This program is free software: you can redistribute it and/or modify\r\n");
		cli_printk(
			"it under the terms of the GNU General Public License as published by\r\n");
		cli_printk(
			"the Free Software Foundation, either version 3 of the License, or\r\n");
		cli_printk("(at your option) any later version.\n\r\n");
		cli_printk(
			"Please refer to the LICENSE file in the project root directory for the full text.\r\n");
	}

	return 0;
}

CLI_COMMAND(show, "show", "Show warranty or copying conditions", show_handler,
	    (struct show_args *)0,
	    OPTION('w', "warranty", BOOL, "Show warranty disclaimer",
		   struct show_args, warranty, 0, NULL, NULL, false),
	    OPTION('c', "conditions", BOOL, "Show copying conditions",
		   struct show_args, conditions, 0, NULL, NULL, false),
	    END_OPTIONS);
