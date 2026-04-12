#include "cmd_dispose.h"
#include "stateM.h"
#include "cli_io.h"

struct tStateEngine dispose_mec;
extern struct tState _dispose_start;
extern struct tState _dispose_end;

int dispose_init(void)
{
	int status = engine_init(&dispose_mec, "dispose_start", &_dispose_start,
				 &_dispose_end);
	if (status < 0) {
		return status;
	}
	return 0;
}

int dispose_start_task(void *cmd)
{
	pr_notice("准备解析命令: %s\n", (char *)cmd);
	return dispose_exit;
}
_EXPORT_STATE_SYMBOL(dispose_start, NULL, dispose_start_task, NULL, ".dispose");

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
