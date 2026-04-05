/**
 * @file state_test.c
 * @brief Test file to verify .state section linking
 */
#include "stateM.h"
#include <stdio.h>

/* Forward declarations */
void test_state_entry(void *private);
int test_state_task(void *private);
void test_state_exit(void *private);

void state2_entry(void *private);
int state2_task(void *private);
void state2_exit(void *private);

void state3_entry(void *private);
int state3_task(void *private);
void state3_exit(void *private);

/* Define states using the macro - these will go into .state section */
_EXPORT_STATE_SYMBOL(test, test_state_entry, test_state_task, test_state_exit,
		     ".state");
_EXPORT_STATE_SYMBOL(state2, state2_entry, state2_task, state2_exit, ".state");
_EXPORT_STATE_SYMBOL(state3, state3_entry, state3_task, state3_exit, ".state");

/* State functions implementation */
void test_state_entry(void *private)
{
}

int test_state_task(void *private)
{
	return 0;
}

void test_state_exit(void *private)
{
}

void state2_entry(void *private)
{
}

int state2_task(void *private)
{
	return 0;
}

void state2_exit(void *private)
{
}

void state3_entry(void *private)
{
}

int state3_task(void *private)
{
	return 0;
}

void state3_exit(void *private)
{
}

/* Test function */
void state_section_test(void)
{
	extern struct tState _state_sec_start;
	extern struct tState _state_sec_end;
	struct tState *state;
	_FOR_EACH_STATE(&_state_sec_start, &_state_sec_end, state)
	{
		printf("state name: %s\n", state->name);
	}
	printf("===========================\n");
}