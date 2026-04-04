#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include "cli_io.h"

void *cli_in_entry(void *arg)
{
	int ch;
	// 使用 getchar() 直接读取单个字符
	while (1) {
		ch = getchar();
		cli_in_push((_u8 *)&ch, 1);

		cli_out_push((_u8 *)&ch, 1);
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
				write(STDOUT_FILENO, &ch, 1);
			} else {
				printf("cli_out_pop err %d\n", status);
			}
		}
	}
}

int main()
{
	// 设置终端为 raw 模式（禁用行缓冲）
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~(ICANON | ECHO); // 禁用规范模式和回显
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	pthread_t cli_in_thread;
	pthread_t cli_out_thread;
	cli_io_init();
	if (pthread_create(&cli_in_thread, NULL, cli_in_entry, NULL)) {
		fprintf(stderr, "创建线程 cli_in_thread 失败\n");
		return 1;
	}
	if (pthread_create(&cli_out_thread, NULL, cli_out_entry, NULL)) {
		fprintf(stderr, "创建线程 cli_in_thread 失败\n");
		return 1;
	}
	printf("主线程：已启动子线程，输入字符立即打印，按 Ctrl+C 退出...\n");

	pthread_join(cli_in_thread, NULL);
	pthread_join(cli_out_thread, NULL);

	// 恢复终端模式
	t.c_lflag |= ICANON;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	printf("主线程：子线程已结束，程序退出。\n");

	return 0;
}