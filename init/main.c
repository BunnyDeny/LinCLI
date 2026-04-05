#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include "cli_io.h"
#include "stateM.h"

extern void state_section_test(void);

static pthread_t cli_in_thread;
static pthread_t cli_out_thread;
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

void *cli_out_entry(void *arg)
{
	while (1) {
		while (cli_get_out_size() > 0) {
			char ch;
			int status = cli_out_pop((_u8 *)&ch, 1);
			if (status == 0) {
				// 检测到 Ctrl+D (ASCII 4)，销毁所有线程
				if ((unsigned char)ch == 4) {
					printf("检测到 Ctrl+D，退出程序\n");
					pthread_cancel(cli_in_thread);
					pthread_cancel(cli_out_thread);
					pthread_cancel(cli_task_thread);
					return NULL;
				}
				write(STDOUT_FILENO, &ch, 1);

				/*debug*/
				printf("  <[userinput] acssi : %3d, char %c\n",
				       (int)ch, ch);
			} else {
				printf("cli_out_pop err %d\n", status);
			}
		}
		usleep(10000);
	}
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
	printf("调度器已启动\n");
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

	printf("主线程：已启动子线程，输入字符立即打印，按 Ctrl+D 退出...\n");

	if (pthread_create(&cli_in_thread, NULL, cli_in_entry, NULL)) {
		fprintf(stderr, "创建线程 cli_in_thread 失败\n");
		return 1;
	}
	if (pthread_create(&cli_out_thread, NULL, cli_out_entry, NULL)) {
		fprintf(stderr, "创建线程 cli_out_thread 失败\n");
		return 1;
	}
	if (pthread_create(&cli_task_thread, NULL, cli_task_thread_entry,
			   NULL)) {
		fprintf(stderr, "创建线程 cli_task_thread 失败\n");
		return 1;
	}

	pthread_join(cli_in_thread, NULL);
	pthread_join(cli_out_thread, NULL);
	pthread_join(cli_task_thread, NULL);

	// 恢复终端模式
	t.c_lflag |= ICANON;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	printf("主线程：子线程已结束，程序退出。\n");

	return 0;
}