#include "unity.h"
#include "cli_mpool.h"

/* cli_mpool.c 中调用的外部函数，在此处提供桩 */
void cli_mpool_dump_usage(void)
{
}

void test_cli_mpool_alloc_and_free(void)
{
	void *p1 = cli_mpool_alloc();
	TEST_ASSERT_NOT_NULL(p1);

	void *p2 = cli_mpool_alloc();
	TEST_ASSERT_NOT_NULL(p2);
	TEST_ASSERT_TRUE(p1 != p2);

	cli_mpool_free(p1);
	cli_mpool_free(p2);
}

void test_cli_mpool_alloc_exhaust(void)
{
	void *ptrs[CLI_MPOOL_COUNT];
	for (int i = 0; i < CLI_MPOOL_COUNT; i++) {
		ptrs[i] = cli_mpool_alloc();
		TEST_ASSERT_NOT_NULL(ptrs[i]);
	}

	/* 内存池已满，再次分配应失败 */
	void *p = cli_mpool_alloc();
	TEST_ASSERT_NULL(p);

	/* 释放一个后应可再次分配 */
	cli_mpool_free(ptrs[0]);
	p = cli_mpool_alloc();
	TEST_ASSERT_NOT_NULL(p);
	cli_mpool_free(p);

	for (int i = 1; i < CLI_MPOOL_COUNT; i++) {
		cli_mpool_free(ptrs[i]);
	}
}

void test_cli_mpool_free_invalid_ptr(void)
{
	/* 释放 NULL 应安全 */
	cli_mpool_free(NULL);

	/* 释放栈地址应安全（静默忽略） */
	int dummy;
	cli_mpool_free(&dummy);

	/* 释放未对齐地址应安全 */
	void *p = cli_mpool_alloc();
	TEST_ASSERT_NOT_NULL(p);
	cli_mpool_free((char *)p + 1); /* 偏移1字节，不是块起始 */
	/* 上面不应释放真正的块，因此先正确释放 p */
	cli_mpool_free(p);
	void *p2 = cli_mpool_alloc();
	TEST_ASSERT_EQUAL_PTR(p, p2); /* 应分配同一个块（因为已释放） */
	cli_mpool_free(p2);
}

void test_cli_mpool_get_usage(void)
{
	const char *owners[CLI_MPOOL_COUNT];
	int used = 0;

	cli_mpool_get_usage(owners, &used);
	int initial_used = used;

	void *p = cli_mpool_alloc();
	cli_mpool_get_usage(owners, &used);
	TEST_ASSERT_EQUAL(initial_used + 1, used);

	cli_mpool_free(p);
	cli_mpool_get_usage(owners, &used);
	TEST_ASSERT_EQUAL(initial_used, used);
}
