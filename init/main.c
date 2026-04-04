#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> // 必须包含 pthread 库头文件
#include <unistd.h>

// 线程运行的函数
void *print_message(void *ptr)
{
	char *message = (char *)ptr;
	for (int i = 0; i < 3; i++) {
		printf("线程运行中: %s (步骤 %d)\n", message, i + 1);
		sleep(1); // 模拟耗时操作
	}
	return NULL;
}

int main()
{
	pthread_t thread1, thread2;
	const char *msg1 = "Thread A";
	const char *msg2 = "Thread B";

	// 1. 创建第一个线程
	if (pthread_create(&thread1, NULL, print_message, (void *)msg1)) {
		fprintf(stderr, "创建线程 1 失败\n");
		return 1;
	}

	// 2. 创建第二个线程
	if (pthread_create(&thread2, NULL, print_message, (void *)msg2)) {
		fprintf(stderr, "创建线程 2 失败\n");
		return 1;
	}

	printf("主线程：已启动子线程，正在等待它们完成...\n");

	// 3. 等待线程结束（join），否则主线程结束会导致子线程强制退出
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);

	printf("主线程：所有子线程已结束，程序退出。\n");

	return 0;
}
