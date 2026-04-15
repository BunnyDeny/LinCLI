#include "cmd_dispose.h"
#include "cli_io.h"

struct double_args {
	double factor;
};

static int double_handler(void *_args)
{
	struct double_args *args = _args;
	pr_notice("DOUBLE test executed!\n");
	pr_notice("  factor = %f\n", args->factor);
	return 0;
}

CLI_COMMAND(td, "td", "Test DOUBLE option", double_handler,
	    (struct double_args *)0,
	    OPTION('f', "factor", DOUBLE, "Float value", struct double_args, factor),
	    END_OPTIONS);
