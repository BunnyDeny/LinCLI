#include "stateM.h"
#include "cli_io.h"

extern struct tState _scheduler_start;
extern struct tState _scheduler_end;

struct tStateEngine scheduler_eng;

/* State functions implementation */
void cli_idle_entry(void *private)
{
	cli_printk("[scheduler]进入空闲状态 cli_idle_entry\n");
	cli_printk(KERN_EMERG "这是EMERG级别 - 最严重的紧急情况\n");
	cli_printk(KERN_ALERT "这是ALERT级别 - 需要立即处理\n");
	cli_printk(KERN_CRIT "这是CRIT级别 - 严重故障\n");
	cli_printk(KERN_ERR "这是ERR级别 - 错误\n");
	cli_printk(KERN_WARNING "这是WARNING级别 - 警告\n");
	cli_printk(KERN_NOTICE "这是NOTICE级别 - 正常但重要\n");
	cli_printk(KERN_INFO "这是INFO级别 - 一般信息\n");
	cli_printk(KERN_DEBUG "这是DEBUG级别 - 调试信息\n");
	cli_printk(KERN_DEFAULT "这是DEFAULT级别 - 无前缀\n");
	cli_printk(KERN_CONT "这是CONT级别 - 继续输出\n");
}

int cli_idle_task(void *private)
{
	// int status, size;
	// size = cli_get_in_size();
	// if (size) {
	// 	char ch;
	// 	status = cli_in_pop((_u8 *)&ch, 1);
	// 	if (status) {
	// 		return status;
	// 	}
	// 	status = cli_out_push((_u8 *)&ch, 1);
	// 	if (status) {
	// 		return status;
	// 	}
	// }
	return 0;
}

_EXPORT_STATE_SYMBOL(cli_idle, cli_idle_entry, cli_idle_task, NULL, ".state");

int scheduler_init(void)
{
	int status;
	cli_io_init();
	status = engine_init(&scheduler_eng, "cli_idle", &_scheduler_start,
			     &_scheduler_end);
	if (status < 0) {
		return status;
	}
	cli_printk("[scheduler]调度器初始化成功\n");
	return 0;
}

/* Test function */
int scheduler_task(void)
{
	int status;
	status = stateEngineRun(&scheduler_eng, NULL);
	if (status < 0) {
		return status;
	} else if (status == 1) {
		return status;
	}
	cli_out_sync();
	return 0;
}