#include "cmd_dispose.h"
#include "cli_io.h"

struct int_args {
	int num;
};

static int int_handler(void *_args)
{
	struct int_args *args = _args;
	pr_notice("INT test executed!\n");
	pr_notice("  num = %d\n", args->num);
	return 0;
}

CLI_COMMAND(ti, "ti", "Test INT option", int_handler,
	    (struct int_args *)0,
	    OPTION('n', "num", INT, "Integer value", struct int_args, num),
	    END_OPTIONS);
