#include "cmd_dispose.h"
#include "init_d.h"
#include "stateM.h"
#include "cli_io.h"
#include "cli_cmd_line.h"

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
