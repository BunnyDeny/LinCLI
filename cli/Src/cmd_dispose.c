#include "cmd_dispose.h"
#include "init_d.h"
#include "stateM.h"
#include "cli_io.h"
#include "cli_cmd_line.h"

struct tStateEngine dispose_mec;

void dispose_init(void *arg)
{
	engine_init(&dispose_mec, "dispose_start", &_dispose_start,
		    &_dispose_end);
}
_EXPORT_INIT_SYMBOL(dispose_init, NULL, dispose_init);

void dispose_start_entry(void *cmd)
{
	engine_init(&dispose_mec, "dispose_start", &_dispose_start,
		    &_dispose_end);
}

int dispose_start_task(void *cmd)
{
	int status;
	pr_notice("准备解析命令: %s\n", (char *)cmd);
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(dispose_start, dispose_start_entry, dispose_start_task,
		     NULL, ".dispose");

int dispose_task(char *cmd)
{
	int status = 0;
	while (1) {
		status = stateEngineRun(&dispose_mec, cmd);
		if (status < 0) {
			pr_err("dispose状态机异常\n");
			return -1;
		} else if (status == dispose_exit) {
			return status;
		}
	}
	return 0; /*nerver*/
}
