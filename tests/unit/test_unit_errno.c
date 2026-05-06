#include "unity.h"
#include "cli_errno.h"

void test_cli_strerror_returns_string(void)
{
	const char *s;

	s = cli_strerror(CLI_OK);
	TEST_ASSERT_NOT_NULL(s);

	s = cli_strerror(CLI_ERR_INVAL);
	TEST_ASSERT_NOT_NULL(s);

	s = cli_strerror(CLI_ERR_FIFO_FULL);
	TEST_ASSERT_NOT_NULL(s);

	s = cli_strerror(-999);
	TEST_ASSERT_NOT_NULL(s);
}

void test_cli_strerror_consistency(void)
{
	/* 同一个错误码两次调用应返回相同的指针或相同的字符串内容 */
	const char *s1 = cli_strerror(CLI_ERR_NULL);
	const char *s2 = cli_strerror(CLI_ERR_NULL);
	TEST_ASSERT_EQUAL_STRING(s1, s2);
}
