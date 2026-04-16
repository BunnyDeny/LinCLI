#include "init_d.h"
#include "cli_io.h"

void pr_license(void *)
{
	cli_printk("LinCLI  Copyright (C) 2026  bunnydeny\n");
	cli_printk(
		"This program comes with ABSOLUTELY NO WARRANTY; for details type `show w'.\n");
	cli_printk(
		"This is free software, and you are welcome to redistribute it\n");
	cli_printk("under certain conditions; type `show c' for details.\n\n");
}
_EXPORT_INIT_SYMBOL(pr_license, NULL, pr_license);
