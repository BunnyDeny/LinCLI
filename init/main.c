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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include "cli_io.h"

/* PC模拟层：用原子自旋锁模拟MCU的关中断/开中断 */
static volatile int cli_io_spinlock = 0;

void cli_io_enter_critical(void)
{
	while (__sync_lock_test_and_set(&cli_io_spinlock, 1)) {
	}
}

void cli_io_exit_critical(void)
{
	__sync_lock_release(&cli_io_spinlock);
}

static pthread_t cli_in_thread;
static pthread_t cli_task_thread;

void *cli_in_entry(void *arg)
{
	int ch, status;
	// 使用 getchar() 直接读取单个字符
	while (1) {
		ch = getchar();
		// printf("接受到字符:%c , 对应的ascci: %3d\n", ch, ch);
		status = cli_in_push((_u8 *)&ch, 1);
		if (status < 0) {
			//printf("cli_in push err:%d\n", status);
		}
	}
}

extern int scheduler_task(void);
extern int scheduler_init(void);

void *cli_task_thread_entry(void *arg)
{
	int status;
	status = scheduler_init();
	if (status < 0) {
		printf("cli_task_thread_entry err code :%d\n", status);
		return NULL;
	}
	while (1) {
		status = scheduler_task();
		if (status < 0) {
			printf("scheduler_task err code :%d\n", status);
		}
		usleep(10000);
	}
}

int main()
{
	// 设置终端为 raw 模式（禁用行缓冲）
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~(ICANON | ECHO); // 禁用规范模式，回显
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	if (pthread_create(&cli_in_thread, NULL, cli_in_entry, NULL)) {
		fprintf(stderr, "创建线程 cli_in_thread 失败\n");
		return 1;
	}
	if (pthread_create(&cli_task_thread, NULL, cli_task_thread_entry,
			   NULL)) {
		fprintf(stderr, "创建线程 cli_task_thread 失败\n");
		return 1;
	}

	pthread_join(cli_in_thread, NULL);
	pthread_join(cli_task_thread, NULL);

	// 恢复终端模式
	t.c_lflag |= ICANON;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	printf("主线程：子线程已结束，程序退出。\n");

	return 0;
}