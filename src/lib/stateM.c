/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "stateM.h"
#include "cli_errno.h"
#include <string.h>

static struct tState *state_pool_search(struct tStateEngine *engine, int state_id)
{
	if (state_id >= 0 && state_id < STATE_POOL_SIZE)
		return engine->state_pool[state_id];
	return NULL;
}

static void state_pool_insert(struct tStateEngine *engine, struct tState *state)
{
	if (state->state_id >= 0 && state->state_id < STATE_POOL_SIZE)
		engine->state_pool[state->state_id] = state;
}

int engine_init(struct tStateEngine *engine, int startup_state_id,
		struct tState *const *sec_start, struct tState *const *sec_end)
{
	if (engine == NULL || sec_start == NULL || sec_end == NULL)
		return CLI_ERR_NULL;
	if (sec_start >= sec_end)
		return CLI_ERR_STATEM_EMPTY;
	engine->from = NULL;
	memset(engine->state_pool, 0, sizeof(engine->state_pool));
	struct tState *state;
	_FOR_EACH_STATE(sec_start, sec_end, state)
	{
		state_pool_insert(engine, state);
	}

	struct tState *_to = state_pool_search(engine, startup_state_id);
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
	struct tState *_to = state_pool_search(engine, state_id);
	if (_to == NULL)
		return CLI_ERR_NOTFOUND;
	if (_to == engine->from)
		return CLI_ERR_STATEM_SAME;
	engine->to = _to;
	return CLI_OK;
}
