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

#include "rbtree.h"

#define STATEM_FORMAT_LOG(fmt, ...)

#define STATEM_SWITCH(from, to)                                      \
	{                                                            \
		if (from && to)                                      \
			STATEM_FORMAT_LOG("switch frome %s to %s\n", \
					  (from)->name, (to)->name); \
	}
#define STATEM_EXIT(state)                                             \
	{                                                              \
		if (state)                                             \
			STATEM_FORMAT_LOG("exit %s\n", (state)->name); \
	}
#define STATEM_ENTRY(state)                                             \
	{                                                               \
		if (state)                                              \
			STATEM_FORMAT_LOG("entry %s\n", (state)->name); \
	}

struct tState {
	char name[32];
	struct rb_node node; /*user no need to care*/
	void (*state_entry)(void *);
	int (*state_task)(void *);
	void (*state_exit)(void *);
} __attribute__((aligned(sizeof(long))));

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
 * 1. Define section and start/end symbols in the linker script:
 *    .scheduler : {
 *        _scheduler_start = .;
 *        *(.scheduler)
 *        _scheduler_end = .;
 *    }
 * 2. Use this macro to export the state, note that obj should not have quotes:
 *    _EXPORT_STATE_SYMBOL(scheduler_idle, scheduler_idle_entry, scheduler_idle_task, NULL, ".scheduler");
 * 3. In initialization code, declare with extern and call engine_init:
 *    extern struct tState _scheduler_start;
 *    extern struct tState _scheduler_end;
 *    engine_init(&engine, "scheduler_idle", &_scheduler_start, &_scheduler_end);
 * 4. In the task loop, call stateEngineRun:
 *    stateEngineRun(&engine, NULL);
 * 5. To switch states, call state_switch with the destination state, using the obj parameter
 *    from when the state was defined via _EXPORT_STATE_SYMBOL macro (note: add quotes):
 *    state_switch(&engine, "state1");
 *
 * @note For detailed usage, refer to init/scheduler.c and cli_project.ld in this project
 */
#define _EXPORT_STATE_SYMBOL(obj, entry, task, exit, _section)       \
	static struct tState state_##obj __attribute__((             \
		used, section(_section), aligned(sizeof(long)))) = { \
		.name = #obj,                                        \
		.state_entry = entry,                                \
		.state_task = task,                                  \
		.state_exit = exit,                                  \
	}
#define _FOR_EACH_STATE(_start, _end, _state) \
	for ((_state) = (_start); (_state) < (_end); (_state)++)

struct tStateEngine {
	struct tState *from;
	struct tState *to;
	struct rb_root state_tree_root;
	void *private;
};

int engine_init(struct tStateEngine *engine, char *startup_state,
		struct tState *sec_start, struct tState *sec_end);
int state_switch(struct tStateEngine *engine, char *name);
int stateEngineRun(struct tStateEngine *engine, void *private);

#endif
