#include "cmd_dispose.h"
#include "cli_io.h"

struct string_args {
	char *msg;
};

static int string_handler(void *_args)
{
	struct string_args *args = _args;
	pr_notice("STRING test executed!\n");
	if (args->msg)
		pr_notice("  msg = %s\n", args->msg);
	return 0;
}

CLI_COMMAND(ts, "ts", "Test STRING option", string_handler,
	    (struct string_args *)0,
	    OPTION('m', "msg", STRING, "Message text", struct string_args, msg),
	    END_OPTIONS);
