/*
 * LinCLI - Lightweight workqueue implementation for embedded/MCU.
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
#include "cli_kconfig.h"

#ifdef WORKQUEUE

#include "workqueue.h"
#include "cli_critical.h"

/* 全局默认队列 */
struct workqueue_struct *system_wq = NULL;

/* 静态池：支持创建少量额外队列 */
#define WQ_MAX_QUEUES   2
static struct workqueue_struct wq_pool[WQ_MAX_QUEUES];
static int wq_pool_idx = 0;

/* ───── 位操作辅助 ───── */
static inline void set_bit(int nr, unsigned long *addr)
{
	*addr |= (1UL << nr);
}

static inline void clear_bit(int nr, unsigned long *addr)
{
	*addr &= ~(1UL << nr);
}

static inline int test_bit(int nr, unsigned long addr)
{
	return (addr >> nr) & 1;
}

/* ───── 平台互斥锁钩子：使用 LinCLI 的 cli_enter_critical / cli_exit_critical ───── */
unsigned long wq_platform_lock_irqsave(void)
{
	cli_enter_critical();
	return 0;
}

void wq_platform_unlock_irqrestore(unsigned long flags)
{
	(void)flags;
	cli_exit_critical();
}

static inline unsigned long wq_lock_irqsave(void)
{
	return wq_platform_lock_irqsave();
}

static inline void wq_unlock_irqrestore(unsigned long flags)
{
	wq_platform_unlock_irqrestore(flags);
}

/* ───── 初始化 ───── */
void INIT_WORK(struct work_struct *work, void (*func)(struct work_struct *))
{
	INIT_LIST_HEAD(&work->entry);
	work->func = func;
	work->flags = 0;
}

void INIT_DELAYED_WORK(struct delayed_work *dwork,
		       void (*func)(struct work_struct *))
{
	INIT_WORK(&dwork->work, func);
	dwork->tick_expire = 0;
	dwork->wq = NULL;
}

/* ───── 调度 ───── */
bool queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	unsigned long flags;

	flags = wq_lock_irqsave();

	if (test_bit(WORK_STRUCT_PENDING, work->flags)) {
		wq_unlock_irqrestore(flags);
		return false;
	}

	set_bit(WORK_STRUCT_PENDING, &work->flags);
	list_add_tail(&work->entry, &wq->work_list);

	wq_unlock_irqrestore(flags);
	return true;
}

bool queue_delayed_work(struct workqueue_struct *wq,
			struct delayed_work *dwork,
			unsigned long delay_ticks)
{
	struct delayed_work *pos;
	unsigned long flags;

	flags = wq_lock_irqsave();

	if (test_bit(WORK_STRUCT_PENDING, dwork->work.flags)) {
		wq_unlock_irqrestore(flags);
		return false;
	}

	set_bit(WORK_STRUCT_PENDING, &dwork->work.flags);
	dwork->tick_expire = jiffies + delay_ticks;
	dwork->wq = wq;

	/* 按 tick_expire 升序插入，相同 tick 排后面（FIFO） */
	list_for_each_entry(pos, &wq->delayed_list, work.entry) {
		if (time_after(pos->tick_expire, dwork->tick_expire))
			break;
	}
	list_add_tail(&dwork->work.entry, &pos->work.entry);

	wq_unlock_irqrestore(flags);
	return true;
}

bool schedule_work(struct work_struct *work)
{
	return queue_work(system_wq, work);
}

bool schedule_delayed_work(struct delayed_work *dwork, unsigned long delay_ticks)
{
	return queue_delayed_work(system_wq, dwork, delay_ticks);
}

/* ───── 内部：执行单个工作 ───── */
static void __workqueue_run_one(struct workqueue_struct *wq)
{
	struct work_struct *work = NULL;
	unsigned long flags;

	flags = wq_lock_irqsave();

	if (!list_empty(&wq->work_list)) {
		work = list_first_entry(&wq->work_list,
					struct work_struct, entry);
		list_del_init(&work->entry);
		clear_bit(WORK_STRUCT_PENDING, &work->flags);
		set_bit(WORK_STRUCT_RUNNING, &work->flags);
		wq->nr_running++;
	}

	wq_unlock_irqrestore(flags);

	if (work) {
		work->func(work);

		flags = wq_lock_irqsave();
		clear_bit(WORK_STRUCT_RUNNING, &work->flags);
		wq->nr_running--;
		wq_unlock_irqrestore(flags);
	}
}

void workqueue_run_one(struct workqueue_struct *wq)
{
	__workqueue_run_one(wq);
}

void workqueue_run_all(struct workqueue_struct *wq)
{
	while (!list_empty(&wq->work_list))
		__workqueue_run_one(wq);
}

/* ───── tick 中断处理：将到期延迟工作移入执行队列 ───── */
void workqueue_tick_handler(struct workqueue_struct *wq,
			    unsigned long current_tick)
{
	struct delayed_work *pos, *n;
	unsigned long flags;

	flags = wq_lock_irqsave();

	/* 链表按 tick_expire 升序排列，扫到第一个未到期的就停 */
	list_for_each_entry_safe(pos, n, &wq->delayed_list, work.entry) {
		if (!time_after_eq(current_tick, pos->tick_expire))
			break;

		list_del_init(&pos->work.entry);
		clear_bit(WORK_STRUCT_PENDING, &pos->work.flags);
		pos->wq = NULL;
		list_add_tail(&pos->work.entry, &wq->work_list);
		set_bit(WORK_STRUCT_PENDING, &pos->work.flags);
	}

	wq_unlock_irqrestore(flags);
}

/* ───── 取消 ───── */
bool cancel_work_sync(struct work_struct *work)
{
	unsigned long flags;

	flags = wq_lock_irqsave();

	if (test_bit(WORK_STRUCT_PENDING, work->flags)) {
		list_del_init(&work->entry);
		clear_bit(WORK_STRUCT_PENDING, &work->flags);
		wq_unlock_irqrestore(flags);
		return true;
	}

	wq_unlock_irqrestore(flags);

	/* 若正在运行，忙等其完成 */
	while (test_bit(WORK_STRUCT_RUNNING, work->flags))
		; /* MCU 实际场景可插入 wfi/任务切换 */

	return true;
}

bool cancel_delayed_work_sync(struct delayed_work *dwork)
{
	unsigned long flags;

	flags = wq_lock_irqsave();

	if (test_bit(WORK_STRUCT_PENDING, dwork->work.flags)) {
		list_del_init(&dwork->work.entry);
		clear_bit(WORK_STRUCT_PENDING, &dwork->work.flags);
		dwork->wq = NULL;
		wq_unlock_irqrestore(flags);
		return true;
	}

	wq_unlock_irqrestore(flags);

	return cancel_work_sync(&dwork->work);
}

/* ───── 刷新 ───── */
void flush_work(struct work_struct *work)
{
	while (test_bit(WORK_STRUCT_PENDING, work->flags) ||
	       test_bit(WORK_STRUCT_RUNNING, work->flags)) {
		workqueue_run_one(system_wq);
	}
}

void flush_delayed_work(struct delayed_work *dwork)
{
	flush_work(&dwork->work);
}

void flush_workqueue(struct workqueue_struct *wq)
{
	while (!list_empty(&wq->work_list) || !list_empty(&wq->delayed_list))
		workqueue_run_all(wq);
}

/* ───── 队列管理 ───── */
struct workqueue_struct *create_workqueue(const char *name)
{
	struct workqueue_struct *wq;

	if (wq_pool_idx >= WQ_MAX_QUEUES)
		return NULL;

	wq = &wq_pool[wq_pool_idx++];
	INIT_LIST_HEAD(&wq->work_list);
	INIT_LIST_HEAD(&wq->delayed_list);
	wq->name = name;
	wq->nr_running = 0;
	return wq;
}

void destroy_workqueue(struct workqueue_struct *wq)
{
	(void)wq;
	/* 静态池，无需释放 */
}

void workqueue_init(void)
{
	static struct workqueue_struct sys_wq;

	INIT_LIST_HEAD(&sys_wq.work_list);
	INIT_LIST_HEAD(&sys_wq.delayed_list);
	sys_wq.name = "system_wq";
	sys_wq.nr_running = 0;

	system_wq = &sys_wq;
}

#endif /* WORKQUEUE */
