#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include "cli_io.h"
#include "stateM.h"

static pthread_t cli_in_thread;
static pthread_t cli_task_thread;

void *cli_in_entry(void *arg)
{
	int ch, status;
	// 使用 getchar() 直接读取单个字符
	while (1) {
		ch = getchar();
		status = cli_in_push((_u8 *)&ch, 1);
		if (status) {
			printf("cli_in push err:%d\n", status);
		}
	}
	printf("输入已结束，线程退出。\n");
	return NULL;
}

extern int scheduler_task(void);
extern int scheduler_init(void);

void *cli_task_thread_entry(void *arg)
{
	int status;
	status = scheduler_init();
	if (status != 0) {
		printf("cli_task_thread_entry err code :%d\n", status);
	}
	while (1) {
		status = scheduler_task();
		if (status != 0) {
			printf("cli_task_thread_entry err code :%d\n", status);
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