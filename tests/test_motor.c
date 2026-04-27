/*
 * LinCLI - 异步非阻塞命令测试：电机逐步加速示例。
 *
 * 注册命令：motor
 * 命令描述：Ramp motor to target RPM
 *
 * 使用示例：
 *   motor -r 500
 *
 * 预期输出：
 *   Motor starting, target=500 rpm
 *   Motor ramping: 100 rpm
 *   Motor ramping: 200 rpm
 *   ...
 *   Motor reached target!
 *   Motor command done.
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct motor_args {
	int target_rpm;
};

static int current_rpm = 0;

static void motor_entry(void *_args)
{
	struct motor_args *args = _args;
	current_rpm = 0;
	cli_printk("Motor starting, target=%d rpm\r\n", args->target_rpm);
}

static int motor_task(void *_args)
{
	struct motor_args *args = _args;

	if (current_rpm < args->target_rpm) {
		current_rpm += 100;
		cli_printk("Motor ramping: %d rpm\r\n", current_rpm);
		return CLI_CONTINUE; /* 还没到达目标，下次继续 */
	}

	cli_printk("Motor reached target!\r\n");
	return 0; /* 成功完成 */
}

static void motor_exit(void *_args)
{
	(void)_args;
	cli_printk("Motor command done.\r\n");
}

CLI_COMMAND_ASYNC(motor, "motor", "Ramp motor to target RPM", motor_entry,
		  motor_task, motor_exit, (struct motor_args *)0,
		  OPTION('r', "rpm", INT, "Target RPM", struct motor_args,
			 target_rpm, 0, NULL, NULL, true),
		  END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
