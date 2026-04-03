#include "stateM.h"
#include <stdio.h>

struct tStateEngine engine;

struct ctx {
	int running_count;
} ctx;

//////////////////////////////////////state1////////////////////////////////
void state1_entry(void *private)
{
	struct ctx *ctx = (struct ctx *)private;
	printf("[state1_entry] running count : %d\n", ctx->running_count++);
}

int state1_task(void *private)
{
	struct ctx *ctx = (struct ctx *)private;
	printf("[state1_task] running count : %d\n", ctx->running_count++);
	if (ctx->running_count > 10) {
		// if (state_switch(&engine, "state4")) {/* switch error */
		if (state_switch(&engine, "state2")) {
			printf("switch error!\n");
			return -2;
		}
	}
	return ctx->running_count;
}

void state1_exit(void *private)
{
	struct ctx *ctx = (struct ctx *)private;
	printf("[state1_exit] running count : %d\n", ctx->running_count++);
}

struct tState state1 = {
	.name = "state1",
	.state_entry = state1_entry,
	.state_task = state1_task,
	.state_exit = state1_exit,
};

//////////////////////////////////////state2////////////////////////////////
void state2_entry(void *private)
{
	struct ctx *ctx = (struct ctx *)private;
	printf("[state2_entry] running count : %d\n", ctx->running_count++);
}

int state2_task(void *private)
{
	struct ctx *ctx = (struct ctx *)private;
	printf("[state2_task] running count : %d\n", ctx->running_count++);
	if (ctx->running_count > 20) {
		if (state_switch(&engine, "state3")) {
			printf("switch error!\n");
			return -2;
		}
	}
	return ctx->running_count;
}

void state2_exit(void *private)
{
	struct ctx *ctx = (struct ctx *)private;
	printf("[state2_exit] running count : %d\n", ctx->running_count++);
}

struct tState state2 = {
	.name = "state2",
	.state_entry = state2_entry,
	.state_task = state2_task,
	.state_exit = state2_exit,
};

//////////////////////////////////////state3////////////////////////////////
void state3_entry(void *private)
{
	struct ctx *ctx = (struct ctx *)private;
	printf("[state3_entry] running count : %d\n", ctx->running_count++);
}

int state3_task(void *private)
{
	struct ctx *ctx = (struct ctx *)private;
	printf("[state3_task] running count : %d\n", ctx->running_count++);
	if (ctx->running_count == 30) {
		return 30;
	}
	return ctx->running_count;
}

struct tState state3 = {
	.name = "state3",
	.state_entry = state3_entry,
	.state_task = state3_task,
	.state_exit = NULL,
};

//////////////////////////////////pool///////////////////////////////
struct tState *pool[3] = { &state1, &state2, &state3 };

/*test engine_init error*/
// struct tState *pool[3] = {&state1, &state2, NULL};

int main(void)
{
	int status = engine_init(&engine, &state1, pool, 3);
	if (status != 0) {
		printf("engine_init error : %d", status);
		return status;
	}
	while (1) {
		int runing_state = stateEngineRun(&engine, &ctx);
		if (runing_state == 30)
			return 0;
		else if (runing_state < 0) {
			printf("running task ocurrs error!\n");
			return -1;
		}
	}
}
