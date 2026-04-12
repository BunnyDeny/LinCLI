#include "cli_io.h"
#include "init_d.h"

void pr_logo(void *arg)
{
	cli_printk(
		"         ____                                     _____   _        _____ \n");
	cli_printk(
		"        |  _ \\                                   / ____| | |      |_   _|\n");
	cli_printk(
		"        | |_) |  _   _   _ __    _ __    _   _  | |      | |        | |  \n");
	cli_printk(
		"        |  _ <  | | | | | '_ \\  | '_ \\  | | | | | |      | |        | |  \n");
	cli_printk(
		"        | |_) | | |_| | | | | | | | | | | |_| | | |____  | |____   _| |_ \n");
	cli_printk(
		"        |____/   \\__,_| |_| |_| |_| |_|  \\__, |  \\_____| |______| |_____|\n");
	cli_printk(
		"                                          __/ |                          \n");
	cli_printk(
		"                                         |___/                           \n");
	pr_info("欢迎使用BunnyCLI\n");
}

_EXPORT_INIT_SYMBOL(logo, NULL, pr_logo);