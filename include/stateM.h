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

//在本工程的任意一个位置将你想要的状态通过此宏定义到参数_section表示的段中去，
//然后在链接脚本中收集好你散乱定义在各个c文件定义到同一个_section的变量，并定义
//好起始符号与结束符号格式类似：
/*
.scheduler : {
    _scheduler_start = .;
    *(.scheduler)
    _scheduler_end = .;
}
*/
//然后在某个地方调用engine_init传入在链接脚本中定义好的起始结束指针符号（注意extern声明时的类型是struct tState *）
//初始化你的状态机引擎，最后在某个任务上下文调用stateEngineRun即可运行你的状态机了
//参数obj指定了状态机的名字，主要用于状态切换的标识符，例如obj=state1，那么当你需要
//调用state_switch切换状态的时候，你需要传入"state1"参数，当然这个双引号在这里是必须的
//但是在_EXPORT_STATE_SYMBOL中是不需要的，因为宏里面加了#obj自动生成字符串
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
