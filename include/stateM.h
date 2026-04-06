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
 * @brief 导出状态机符号到指定段
 *
 * @param obj      状态机名字，用于state_switch()调用时的标识符（不加引号）
 * @param entry    状态入口函数
 * @param task     状态任务函数
 * @param exit     状态退出函数
 * @param _section 目标段名，需与链接脚本中的段名一致
 *
 * @details
 * 使用例子：
 * 1. 在链接脚本中定义段和起始结束符号：
 *    .scheduler : {
 *        _scheduler_start = .;
 *        *(.scheduler)
 *        _scheduler_end = .;
 *    }
 * 2. 使用本宏导出状态，注意obj不加引号：
 *    _EXPORT_STATE_SYMBOL(cli_idle, cli_idle_entry, cli_idle_task, NULL, ".scheduler");
 * 3. 在初始化代码中extern声明并调用engine_init：
 *    extern struct tState _scheduler_start;
 *    extern struct tState _scheduler_end;
 *    engine_init(&engine, "cli_idle", &_scheduler_start, &_scheduler_end);
 * 4. 在任务循环中调用stateEngineRun：
 *    stateEngineRun(&engine, NULL);
 * 5. 切换状态时调用state_switch传入目的状态通过_EXPORT_STATE_SYMBOL宏定义的时候
 *    的obj参数（注意加引号）：
 *    state_switch(&engine, "state1");
 *
 * @note 具体用法可参照本项目 init/scheduler.c 和 cli_project.ld
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
};

int engine_init(struct tStateEngine *engine, char *startup_state,
		struct tState *sec_start, struct tState *sec_end);
int state_switch(struct tStateEngine *engine, char *name);
int stateEngineRun(struct tStateEngine *engine, void *private);

#endif
