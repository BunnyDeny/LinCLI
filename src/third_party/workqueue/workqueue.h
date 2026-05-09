/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _WORKQUEUE_H
#define _WORKQUEUE_H

#include "list.h"
#include <stdbool.h>

/* 工作项状态标志 */
#define WORK_STRUCT_PENDING     0
#define WORK_STRUCT_RUNNING     1

/* 时间比较宏（防回绕） */
#define time_after(a, b)        ((long)((b) - (a)) < 0)
#define time_after_eq(a, b)     ((long)((a) - (b)) >= 0)

/* ───── 工作项 ───── */
struct work_struct {
	struct list_head entry;
	void (*func)(struct work_struct *work);
	unsigned long flags;
};

/* ───── 延迟工作项 ───── */
struct delayed_work {
	struct work_struct work;
	unsigned long tick_expire;
	struct workqueue_struct *wq;
};

/* ───── 工作队列 ───── */
struct workqueue_struct {
	struct list_head work_list;
	struct list_head delayed_list;
	const char *name;
	unsigned int nr_running;
};

/* 全局 jiffies（由外部定时器中断维护） */
extern volatile unsigned long jiffies;

/* 系统默认工作队列 */
extern struct workqueue_struct *system_wq;

/* ───── 平台互斥锁钩子（用户可覆盖） ───── */
unsigned long wq_platform_lock_irqsave(void);
void wq_platform_unlock_irqrestore(unsigned long flags);

/* ───── 初始化 ───── */
void INIT_WORK(struct work_struct *work, void (*func)(struct work_struct *));
void INIT_DELAYED_WORK(struct delayed_work *dwork, void (*func)(struct work_struct *));

/* ───── 调度 / 入队 ───── */
bool queue_work(struct workqueue_struct *wq, struct work_struct *work);
bool queue_delayed_work(struct workqueue_struct *wq,
			struct delayed_work *dwork,
			unsigned long delay_ticks);
bool schedule_work(struct work_struct *work);
bool schedule_delayed_work(struct delayed_work *dwork, unsigned long delay_ticks);

/* ───── 取消 ───── */
bool cancel_work_sync(struct work_struct *work);
bool cancel_delayed_work_sync(struct delayed_work *dwork);

/* ───── 刷新 ───── */
void flush_work(struct work_struct *work);
void flush_delayed_work(struct delayed_work *dwork);
void flush_workqueue(struct workqueue_struct *wq);

/* ───── 执行引擎 ───── */
void workqueue_run_one(struct workqueue_struct *wq);
void workqueue_run_all(struct workqueue_struct *wq);
void workqueue_tick_handler(struct workqueue_struct *wq, unsigned long current_tick);

/* ───── 队列管理 ───── */
struct workqueue_struct *create_workqueue(const char *name);
void destroy_workqueue(struct workqueue_struct *wq);
void workqueue_init(void);

#endif
