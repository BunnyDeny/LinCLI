/**
 * @file state_test.c
 * @brief Test file to verify .state section linking
 */
#include "stateM.h"
#include <stdio.h>
#include "cli_io.h"
#include <unistd.h>

struct scheduler_arg {
	int running_cnt;
} scheduler_arg = { 0 };

struct tStateEngine scheduler_eng;

/* State functions implementation */
void test_state_entry(void *private)
{
	cli_io_init();
}

int test_state_task(void *private)
{
	int status, size;
	size = cli_get_in_size();
	if (size) {
		char ch;
		status = cli_in_pop((_u8 *)&ch, 1);
		if (status) {
			printf("pop in err : %d\n", status);
		}
		status = cli_out_push((_u8 *)&ch, 1);
		if (status) {
			printf("push out err : %d\n", status);
		}
	}
	return 0;
}

void test_state_exit(void *private)
{
}
_EXPORT_STATE_SYMBOL(test, test_state_entry, test_state_task, test_state_exit,
		     ".state");

int scheduler_init(void)
{
	int status;
	extern struct tState _state_sec_start;
	extern struct tState _state_sec_end;
	struct tState *state;
	_FOR_EACH_STATE(&_state_sec_start, &_state_sec_end, state)
	{
		printf("state name: %s\n", state->name);
	}
	status = engine_init(&scheduler_eng, "test", &_state_sec_start,
			     &_state_sec_end);
	if (status < 0) {
		printf("scheduler_init err code : %d\n", status);
	}
	printf("===========================\n");
	return 0;
}

/* Test function */
int scheduler_task(void)
{
	int status;
	status = stateEngineRun(&scheduler_eng, &scheduler_arg);
	if (status < 0) {
		printf("scheduler_eng err code : %d\n", status);
		return status;
	} else if (status == 1) {
		printf("scheduler_eng : 正常退出\n");
		return status;
	}
	return 0;
}