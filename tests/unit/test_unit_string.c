#include "unity.h"
#include "cli_string.h"

void test_cli_strlen_normal(void)
{
	TEST_ASSERT_EQUAL(0, cli_strlen(""));
	TEST_ASSERT_EQUAL(5, cli_strlen("hello"));
	TEST_ASSERT_EQUAL(11, cli_strlen("hello world"));
}

void test_cli_strcpy_normal(void)
{
	char buf[32];
	TEST_ASSERT_EQUAL_STRING("hello", cli_strcpy(buf, "hello"));
	TEST_ASSERT_EQUAL_STRING("hello", buf);
}

void test_cli_strncpy_normal(void)
{
	char buf[32] = { 0 };
	cli_strncpy(buf, "hello world", 5);
	TEST_ASSERT_EQUAL_MEMORY("hello", buf, 5);
}

void test_cli_strcmp_normal(void)
{
	TEST_ASSERT_EQUAL(0, cli_strcmp("abc", "abc"));
	TEST_ASSERT_TRUE(cli_strcmp("abc", "abd") < 0);
	TEST_ASSERT_TRUE(cli_strcmp("abd", "abc") > 0);
}

void test_cli_strncmp_normal(void)
{
	TEST_ASSERT_EQUAL(0, cli_strncmp("abc", "abc", 3));
	TEST_ASSERT_EQUAL(0, cli_strncmp("abc", "abd", 2));
	TEST_ASSERT_TRUE(cli_strncmp("abc", "abd", 3) < 0);
}

void test_cli_strcat_normal(void)
{
	char buf[32] = "hello";
	cli_strcat(buf, " world");
	TEST_ASSERT_EQUAL_STRING("hello world", buf);
}

void test_cli_strchr_normal(void)
{
	TEST_ASSERT_EQUAL_PTR(NULL, cli_strchr("hello", 'z'));
	TEST_ASSERT_EQUAL_PTR(&"hello"[2], cli_strchr("hello", 'l'));
}

void test_cli_memcpy_normal(void)
{
	char src[] = "hello";
	char dst[32] = { 0 };
	cli_memcpy(dst, src, 6);
	TEST_ASSERT_EQUAL_STRING("hello", dst);
}

void test_cli_memset_normal(void)
{
	char buf[32] = { 0 };
	cli_memset(buf, 'A', 10);
	for (int i = 0; i < 10; i++) {
		TEST_ASSERT_EQUAL('A', buf[i]);
	}
	TEST_ASSERT_EQUAL(0, buf[10]);
}

void test_cli_memmove_overlap(void)
{
	char buf[] = "hello world";
	cli_memmove(buf + 6, buf, 5);
	TEST_ASSERT_EQUAL_STRING("hello hello", buf);
}

void test_cli_memcmp_normal(void)
{
	TEST_ASSERT_EQUAL(0, cli_memcmp("abc", "abc", 3));
	TEST_ASSERT_TRUE(cli_memcmp("abc", "abd", 3) < 0);
	TEST_ASSERT_TRUE(cli_memcmp("abd", "abc", 3) > 0);
}
