#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

void *capture_input(void *arg)
{
	char ch;
	// 使用 read() 直接读取单个字符
	while (read(STDIN_FILENO, &ch, 1) > 0) {
		printf("捕获到字符: '%c' (ASCII: %d)\n", ch, ch);
	}
	printf("输入已结束，线程退出。\n");
	return NULL;
}

int main()
{
	// 设置终端为 raw 模式（禁用行缓冲）
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~(ICANON); // 禁用规范模式
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	pthread_t thread1;
	const char *msg1 = "Thread A";

	if (pthread_create(&thread1, NULL, capture_input, (void *)msg1)) {
		fprintf(stderr, "创建线程 1 失败\n");
		return 1;
	}
	printf("主线程：已启动子线程，输入字符立即打印，按 Ctrl+C 退出...\n");

	pthread_join(thread1, NULL);

	// 恢复终端模式
	t.c_lflag |= ICANON;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	printf("主线程：子线程已结束，程序退出。\n");

	return 0;
}