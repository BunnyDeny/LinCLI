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
	struct rb_node node; /*user no need to care*/
	char name[32];
	void (*state_entry)(void *);
	int (*state_task)(void *);
	void (*state_exit)(void *);
};

#define _EXPORT_STATE_SYMBOL(name, obj, entry, task, exit, section) \
	static struct tState state_##obj\
__attribute__((used, section(section))) = {                         \
		.name = name,                                       \
		.state_entry = entry,                               \
		.state_task = task,                                 \
		.state_exit = exit,                                 \
	}

struct tStateEngine {
	struct tState *from;
	struct tState *to;
	struct rb_root state_tree_root;
};

int engine_init(struct tStateEngine *engine, struct tState *startup_state,
		struct tState **pool, int pool_size);
int state_switch(struct tStateEngine *engine, char *name);
int stateEngineRun(struct tStateEngine *engine, void *private);

#endif
