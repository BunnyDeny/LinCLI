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

/**
 * @file state_engine.h
 * @brief Lightweight State Machine Engine with strict error propagation.
 *
 * @details
 * ### CRITICAL DESIGN PRINCIPLE: Return Value Propagation
 * This engine is designed such that the return value of `stateEngineRun()` is
 * essentially the return value of the currently active `state_task()`.
 *
 * **Developer Responsibilities:**
 * 1. **Define Meaningful Return Codes:** Developers must carefully define and
 *    consistent return values for all `state_task` functions.
 * 2. **Check state_switch Results:** Functions `state_switch()` MUST be
 *    called within a `state_task`. If `state_switch()` returns a negative value
 *    (indicating a transition failure), the `state_task` should immediately
 *    propagate this failure by returning a negative error code itself.
 *    Note: `CLI_ERR_STATEM_SAME` means the target state is already active,
 *    which is typically harmless and can be ignored or propagated as needed.
 * 3. **Centralized Error Handling:** The caller of `stateEngineRun()`
 * (typically in the `main()` loop or an RTOS task context) must monitor the
 * return value. A negative result indicates an exception within a state task,
 *    allowing the system to implement recovery strategies, such as logging,
 *    safe-stopping, or rebooting.
 *
 * @author https://github.com/BunnyDeny
 * @version 1.0
 */
#ifndef _STATE_M_
#define _STATE_M_

#include "cli_state_ids.h"

#define STATE_POOL_SIZE 64

#define STATEM_FORMAT_LOG(fmt, ...)

#define STATEM_SWITCH(from, to)                                        \
	{                                                              \
		if (from && to)                                        \
			STATEM_FORMAT_LOG("switch %d to %d\r\n",      \
					  (from)->state_id, (to)->state_id);   \
	}
#define STATEM_EXIT(state)                                               \
	{                                                                \
		if (state)                                               \
			STATEM_FORMAT_LOG("exit %d\r\n", (state)->state_id); \
	}
#define STATEM_ENTRY(state)                                               \
	{                                                                 \
		if (state)                                                \
			STATEM_FORMAT_LOG("entry %d\r\n", (state)->state_id); \
	}

struct tState {
	int state_id;
	void (*state_entry)(void *);
	int (*state_task)(void *);
	void (*state_exit)(void *);
};

/**
 * @brief Export state machine symbols to specified section
 *
 * @param obj       State machine name, used as identifier for state_switch() call (without quotes)
 * @param entry     State entry function
 * @param task      State task function
 * @param exit      State exit function
 * @param _section  Target section name, must match the section name in the linker script
 *
 * @details
 * Usage example:
 * 1. Define section and start/end symbols in the linker script
 *    (using the triple-section layout employed by this project):
 *    .scheduler : {
 *        KEEP(*(.scheduler.0.start))
 *        KEEP(*(.scheduler.1))
 *        KEEP(*(.scheduler.1.end))
 *    }
 * 2. Use this macro to export the state, note that obj should not have quotes:
 *    _EXPORT_STATE_SYMBOL(scheduler_idle, scheduler_idle_entry, scheduler_idle_task, NULL, ".scheduler");
 * 3. In initialization code, declare with extern and call engine_init:
 *    extern struct tState * const _scheduler_start[];
 *    extern struct tState * const _scheduler_end[];
 *    engine_init(&engine, "scheduler_idle", _scheduler_start, _scheduler_end);
 * 4. In the task loop, call stateEngineRun:
 *    stateEngineRun(&engine, NULL);
 * 5. To switch states, call state_switch with the destination state, using the obj parameter
 *    from when the state was defined via _EXPORT_STATE_SYMBOL macro (note: add quotes):
 *    state_switch(&engine, "state1");
 *
 * @note For detailed usage, refer to init/scheduler.c and cli_project.ld in this project
 */
#define _EXPORT_STATE_SYMBOL(obj, _state_id, entry, task, exit, _section) \
	static struct tState state_##obj = {                   \
		.state_id = _state_id,                         \
		.state_entry = entry,                          \
		.state_task = task,                            \
		.state_exit = exit,                            \
	};                                                     \
	static struct tState *const _state_ptr_##obj           \
		__attribute__((used, section(_section ".1"))) = &state_##obj
#define _FOR_EACH_STATE(_start, _end, _state)             \
	for (struct tState *const *_pp = (_start);        \
	     _pp < (struct tState *const *)(_end); _pp++) \
		if (((_state) = *_pp) != NULL)

struct tStateEngine {
	struct tState *from;
	struct tState *to;
	struct tState *state_pool[STATE_POOL_SIZE];
};

int engine_init(struct tStateEngine *engine, int startup_state_id,
		struct tState *const *sec_start, struct tState *const *sec_end);
int state_switch(struct tStateEngine *engine, int state_id);
int stateEngineRun(struct tStateEngine *engine, void *private);

#endif
