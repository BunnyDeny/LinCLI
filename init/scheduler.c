/**
 * @file state_test.c
 * @brief Test file to verify .state section linking
 */
#include "stateM.h"
#include <stdio.h>

struct scheduler_arg {
	int running_cnt;
} scheduler_arg = { 0 };

struct tStateEngine scheduler_eng;

/* State functions implementation */
void test_state_entry(void *private)
{
	struct scheduler_arg *arg = (struct scheduler_arg *)private;
	printf("test_state_entry : running_cnt = %3d\n", arg->running_cnt++);
}

int test_state_task(void *private)
{
	int status;
	struct scheduler_arg *arg = (struct scheduler_arg *)private;
	printf("test_state_task : running_cnt = %3d\n", arg->running_cnt++);
	if (arg->running_cnt > 10) {
		status = state_switch(&scheduler_eng, "state2");
		if (status < 0) {
			return status;
		}
	}
	return arg->running_cnt;
}

void test_state_exit(void *private)
{
	struct scheduler_arg *arg = (struct scheduler_arg *)private;
	printf("test_state_exit : running_cnt = %3d\n", arg->running_cnt++);
}
_EXPORT_STATE_SYMBOL(test, test_state_entry, test_state_task, test_state_exit,
		     ".state");

void state2_entry(void *private)
{
	struct scheduler_arg *arg = (struct scheduler_arg *)private;
	printf("state2_entry : running_cnt = %3d\n", arg->running_cnt++);
}

int state2_task(void *private)
{
	int status;
	struct scheduler_arg *arg = (struct scheduler_arg *)private;
	printf("state2_task : running_cnt = %3d\n", arg->running_cnt++);
	if (arg->running_cnt > 20) {
		status = state_switch(&scheduler_eng, "state3");
		if (status < 0) {
			return status;
		}
	}
	return arg->running_cnt;
	return 0;
}

void state2_exit(void *private)
{
	struct scheduler_arg *arg = (struct scheduler_arg *)private;
	printf("state2_exit : running_cnt = %3d\n", arg->running_cnt++);
}
_EXPORT_STATE_SYMBOL(state2, state2_entry, state2_task, state2_exit, ".state");

void state3_entry(void *private)
{
	struct scheduler_arg *arg = (struct scheduler_arg *)private;
	printf("state3_entry : running_cnt = %3d\n", arg->running_cnt++);
}

int state3_task(void *private)
{
	return 1;
}
_EXPORT_STATE_SYMBOL(state3, state3_entry, state3_task, NULL, ".state");

/* Test function */
void state_section_test(void)
{
	int status;
	extern struct tState _state_sec_start;
	extern struct tState _state_sec_end;
	struct tState *state;
	_FOR_EACH_STATE(&_state_sec_start, &_state_sec_end, state)
	{
		printf("state name: %s\n", state->name);
	}
	engine_init(&scheduler_eng, "test", &_state_sec_start, &_state_sec_end);
	printf("===========================\n");
	while (1) {
		status = stateEngineRun(&scheduler_eng, &scheduler_arg);
		if (status < 0) {
			printf("scheduler_eng err code : %d\n", status);
			return;
		} else if (status == 1) {
			printf("scheduler_eng : 正常退出\n");
			return;
		}
	}
}