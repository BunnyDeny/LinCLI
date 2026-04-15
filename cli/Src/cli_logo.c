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