/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "stateM.h"
#include "cli_errno.h"
#include <string.h>

#define STATE_POOL_SIZE 64

static struct tState *state_pool[STATE_POOL_SIZE];

static struct tState *state_pool_search(int state_id)
{
	if (state_id >= 0 && state_id < STATE_POOL_SIZE)
		return state_pool[state_id];
	return NULL;
}

static void state_pool_insert(struct tState *state)
{
	if (state->state_id >= 0 && state->state_id < STATE_POOL_SIZE)
		state_pool[state->state_id] = state;
}

int engine_init(struct tStateEngine *engine, int startup_state_id,
		struct tState *const *sec_start, struct tState *const *sec_end)
{
	if (engine == NULL || sec_start == NULL || sec_end == NULL)
		return CLI_ERR_NULL;
	if (sec_start >= sec_end)
		return CLI_ERR_STATEM_EMPTY;
	engine->from = NULL;
	memset(state_pool, 0, sizeof(state_pool));
	struct tState *state;
	_FOR_EACH_STATE(sec_start, sec_end, state)
	{
		state_pool_insert(state);
	}

	struct tState *_to = state_pool_search(startup_state_id);
	if (_to == NULL) {
		return CLI_ERR_NOTFOUND;
	}
	engine->to = _to;
	return CLI_OK;
}

int stateEngineRun(struct tStateEngine *engine, void *private)
{
	if (engine == NULL)
		return CLI_ERR_NULL;
	if (engine->from != engine->to) {
		STATEM_EXIT(engine->from);
		if (engine->from && engine->from->state_exit) {
			engine->from->state_exit(private);
		}

		STATEM_SWITCH(engine->from, engine->to);

		engine->from = engine->to;
		STATEM_ENTRY(engine->from);
		if (engine->from->state_entry) {
			engine->from->state_entry(private);
		}
	}
	if (engine->from->state_task) {
		return engine->from->state_task(private);
	} else {
		return 0;
	}
}

int state_switch(struct tStateEngine *engine, int state_id)
{
	if (engine == NULL)
		return CLI_ERR_NULL;
	struct tState *_to = state_pool_search(state_id);
	if (_to == NULL)
		return CLI_ERR_NOTFOUND;
	if (_to == engine->from)
		return CLI_ERR_STATEM_SAME;
	engine->to = _to;
	return CLI_OK;
}
