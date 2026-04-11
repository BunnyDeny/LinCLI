#include "cmd_dispose.h"

int dispose_start(void *arg)
{
	int status;
	pr_notice("准备解析命令: %s\n", origin_cmd.buf);
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(dispose_start, NULL, dispose_start, NULL, ".cli_cmd_line");
