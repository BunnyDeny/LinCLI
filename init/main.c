#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

void *capture_input(void *arg)
{
	int ch;
	printf("请输入字符（每输入一个字符立即打印）:\n");
	while ((ch = getchar()) != EOF) {
		printf("捕获到字符: '%c' (ASCII: %d)\n", ch, ch);
		if (ch == '\n') {
			printf("请继续输入:\n");
		}
	}
	printf("输入已结束，线程退出。\n");
	return NULL;
}

int main()
{
	pthread_t thread1;
	const char *msg1 = "Thread A";

	if (pthread_create(&thread1, NULL, capture_input, (void *)msg1)) {
		fprintf(stderr, "创建线程 1 失败\n");
		return 1;
	}
	printf("主线程：已启动子线程，正在等待它们完成...\n");

	//等待线程结束（join）
	pthread_join(thread1, NULL);
	printf("主线程：所有子线程已结束，程序退出。\n");

	return 0;
}
