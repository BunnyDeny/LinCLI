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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include "cli_io.h"
#include "cli_critical.h"

/* PC模拟层：用原子自旋锁模拟MCU的关中断/开中断 */
static volatile int cli_io_spinlock = 0;

void cli_enter_critical(void)
{
	while (__sync_lock_test_and_set(&cli_io_spinlock, 1)) {
	}
}

void cli_exit_critical(void)
{
	__sync_lock_release(&cli_io_spinlock);
}

void cli_putc(char ch)
{
	write(STDOUT_FILENO, &ch, 1);
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