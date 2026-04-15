#include "cmd_dispose.h"
#include "cli_io.h"

struct bool_args {
	bool verbose;
};

static int bool_handler(void *_args)
{
	struct bool_args *args = _args;
	pr_notice("BOOL test executed!\n");
	if (args->verbose)
		pr_notice("  verbose = true\n");
	return 0;
}

CLI_COMMAND(tb, "tb", "Test BOOL option", bool_handler,
	    (struct bool_args *)0, OPTION('v', "verbose", BOOL, "Enable verbose",
					  struct bool_args, verbose),
	    END_OPTIONS);
