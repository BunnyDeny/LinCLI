#include "cmd_dispose.h"
#include "cli_io.h"

struct cb_args {
	const char *raw;
};

static int callback_handler(void *_args)
{
	struct cb_args *args = _args;
	pr_notice("CALLBACK test executed!\n");
	pr_notice("  custom callback triggered with: %s\n",
		  args->raw ? args->raw : "(null)");
	return 0;
}

CLI_COMMAND(tc, "tc", "Test CALLBACK option", callback_handler,
	    (struct cb_args *)0,
	    OPTION('c', "cfg", CALLBACK, "Raw config string", struct cb_args, raw),
	    END_OPTIONS);
