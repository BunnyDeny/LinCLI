#include "cmd_dispose.h"
#include "cli_io.h"

struct array_args {
	bool verbose;
	int *nums;
	size_t nums_count;
};

static int array_handler(void *_args)
{
	struct array_args *args = _args;
	pr_notice("INT_ARRAY test executed!\n");
	if (args->verbose)
		pr_notice("  verbose = true\n");
	if (args->nums && args->nums_count > 0) {
		pr_notice("  nums = ");
		for (size_t i = 0; i < args->nums_count; i++)
			cli_printk(KERN_NOTICE "%d ", args->nums[i]);
		cli_printk(KERN_NOTICE "\n");
	}
	return 0;
}

CLI_COMMAND(ta, "ta", "Test INT_ARRAY option with depends", array_handler,
	    (struct array_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct array_args,
		   verbose),
	    OPTION('n', "nums", INT_ARRAY, "Number list", struct array_args, nums,
		   8, "verbose"),
	    END_OPTIONS);
