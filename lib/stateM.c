/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "stateM.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct tState *state_search(struct rb_root *root, char *name)
{
	struct rb_node *node = root->rb_node;
	while (node) {
		struct tState *data = container_of(node, struct tState, node);
		int result;
		result = strcmp(name, data->name);
		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

static int state_insert(struct rb_root *root, struct tState *state_to_insert)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	/* Figure out where to put new node */
	while (*new) {
		struct tState *this = container_of(*new, struct tState, node);
		int result = strcmp(state_to_insert->name, this->name);
		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			return -1;
	}
	/* Add new node and rebalance tree. */
	rb_link_node(&state_to_insert->node, parent, new);
	rb_insert_color(&state_to_insert->node, root);
	return 0;
}

/**
 * @brief Initializes the state engine with a startup state and state pool.
 *
 * @note This function ensures 'engine->to' is initialized with a non-null
 * 'startup_state', establishing the safety guarantee for the first run.
 *
 * @param engine        Pointer to the state engine.
 * @param startup_state The initial state to enter. Must not be NULL.
 * @param pool          Array of available states.
 * @param pool_size     Number of states in the pool.
 * @return 0 on success, -1 if engine or startup_state is NULL.
 */
int engine_init(struct tStateEngine *engine, char *startup_state,
		struct tState *sec_start, struct tState *sec_end)
{
	if (engine == NULL || startup_state == NULL || sec_start == NULL ||
	    sec_end == NULL)
		return -1;
	if (sec_start >= sec_end)
		return -2;
	engine->from = NULL;
	struct rb_root *rbtree_root = &engine->state_tree_root;
	*rbtree_root = RB_ROOT;
	struct tState *state;
	_FOR_EACH_STATE(sec_start, sec_end, state)
	{
		if (state == NULL) {
			return -3;
		}
		state_insert(rbtree_root, state);
	}

	struct tState *_to = state_search(rbtree_root, startup_state);
	if (_to == NULL) {
		return -4;
	}
	engine->to = _to;
	return 0;
}

/**
 * @brief Executes the state machine's main loop and state transitions.
 *
 * @details
 * 1. Handles state transitions: If 'from' != 'to', it triggers the exit
 *    logic of the old state and entry logic of the new state.
 * 2. **Pointer Safety:** Both 'engine_init' and 'state_switch' guarantee that
 *    'engine->to' is never NULL. Thus, after assignment, 'engine->from' is
 *    guaranteed to be valid for task execution.
 * 3. **Error Propagation:** The return value is directly passed from the
 *    active state's task function. A negative return value triggers an
 *    engine-level exception in the main loop.
 *
 * @param engine  Pointer to the state engine.
 * @param private Context-specific data passed to state functions.
 * @return The status code from the state task.
 */
int stateEngineRun(struct tStateEngine *engine, void *private)
{
	if (engine == NULL)
		return -1;
	if (engine->from != engine->to) {
		STATEM_EXIT(engine->from);
		if (engine->from && engine->from->state_exit) {
			engine->from->state_exit(private);
		}

		/*Both engine_init and state_switch guarantee that the to pointer is never
     * null. Therefore, the subsequent if check on from (after assignment) is
     * redundant.*/
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

/**
 * @brief Switches the target state of the state engine.
 *
 * @note **IMPORTANT:** This function MUST only be called within a state task.
 *
 * @warning The caller must propagate negative return values back to the engine.
 * Any negative value returned by a state task will cause the stateEngineRun()
 * to return that same negative value, triggering an engine exception.
 *
 * @return 0  Success.
 * @return -1 Invalid engine pointer or null state in pool.
 * @return -2 No matching state name found in the pool.
 *
 * @example
 * // 1. Inside a state task:
 * int my_state_task(struct tStateEngine *engine, void *ctx) {
 *     if (state_switch(engine, "state2")) {
 *         printf("switch error!\n");
 *         return -2; // Propagate error to the engine
 *     }
 *     return 0;
 * }
 */
int state_switch(struct tStateEngine *engine, char *name)
{
	if (engine == NULL)
		return -1;
	struct tState *_to = state_search(&engine->state_tree_root, name);
	if (_to == NULL || _to == engine->from) {
		return -2;
	}
	engine->to = _to;
	return 0;
}
