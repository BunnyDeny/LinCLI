#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include "cli_io.h"
#include "stateM.h"

static pthread_t cli_in_thread;
static pthread_t cli_out_thread;
static pthread_t cli_task_thread;

void test_entry(void *arg)
{
}

int test_task(void *arg)
{
	return 0;
}

void test_exit(void *arg)
{
}
_EXPORT_STATE_SYMBOL(my_test, test_entry, test_task, test_exit,
		     ".my_test_section");

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

void *cli_task_thread_entry(void *arg)
{
	int status, size;
	cli_io_init();
	while (1) {
		size = cli_get_in_size();
		if (size) {
			char ch;
			status = cli_in_pop((_u8 *)&ch, 1);
			if (status) {
				printf("pop in err : %d\n", status);
			}
			status = cli_out_push((_u8 *)&ch, 1);
			if (status) {
				printf("push out err : %d\n", status);
			}
		}
		usleep(10000);
	}
}

int main()
{
	// 设置终端为 raw 模式（禁用行缓冲）
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~(ICANON | ECHO); // 禁用规范模式和回显
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