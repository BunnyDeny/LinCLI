#include "unity.h"
#include "cli_vsnprintf.h"

void test_cli_snprintf_basic(void)
{
	char buf[32];
	int len;

	len = cli_snprintf(buf, sizeof(buf), "val=%d", 42);
	TEST_ASSERT_EQUAL_STRING("val=42", buf);
	TEST_ASSERT_EQUAL(6, len);

	len = cli_snprintf(buf, sizeof(buf), "str=%s", "ok");
	TEST_ASSERT_EQUAL_STRING("str=ok", buf);
	TEST_ASSERT_EQUAL(6, len);

	len = cli_snprintf(buf, sizeof(buf), "ch=%c", 'A');
	TEST_ASSERT_EQUAL_STRING("ch=A", buf);
	TEST_ASSERT_EQUAL(4, len);
}

void test_cli_snprintf_unsigned(void)
{
	char buf[32];
	int len = cli_snprintf(buf, sizeof(buf), "u=%u", 123);
	TEST_ASSERT_EQUAL_STRING("u=123", buf);
	TEST_ASSERT_EQUAL(5, len);
}

void test_cli_snprintf_percent(void)
{
	char buf[32];
	int len = cli_snprintf(buf, sizeof(buf), "pct=%%");
	TEST_ASSERT_EQUAL_STRING("pct=%", buf);
	TEST_ASSERT_EQUAL(5, len);
}

void test_cli_snprintf_width(void)
{
	char buf[32];
	int len;

	len = cli_snprintf(buf, sizeof(buf), "[%5d]", 42);
	TEST_ASSERT_EQUAL_STRING("[   42]", buf);
	TEST_ASSERT_EQUAL(7, len);

	len = cli_snprintf(buf, sizeof(buf), "[%-5d]", 42);
	TEST_ASSERT_EQUAL_STRING("[42   ]", buf);
	TEST_ASSERT_EQUAL(7, len);

	len = cli_snprintf(buf, sizeof(buf), "[%5s]", "hi");
	TEST_ASSERT_EQUAL_STRING("[   hi]", buf);
	TEST_ASSERT_EQUAL(7, len);
}

void test_cli_snprintf_truncate(void)
{
	char buf[8];
	int len;

	len = cli_snprintf(buf, sizeof(buf), "hello world");
	TEST_ASSERT_EQUAL_STRING("hello w", buf);
	TEST_ASSERT_TRUE(len < 11);

	len = cli_snprintf(buf, 1, "hello");
	TEST_ASSERT_EQUAL_STRING("", buf);
	TEST_ASSERT_EQUAL(0, len);

	len = cli_snprintf(buf, 0, "hello");
	TEST_ASSERT_EQUAL(0, len);
}

void test_cli_snprintf_null_string(void)
{
	char buf[32];
	int len = cli_snprintf(buf, sizeof(buf), "s=%s", (char *)NULL);
	TEST_ASSERT_EQUAL_STRING("s=(null)", buf);
	TEST_ASSERT_EQUAL(8, len);
}

void test_cli_snprintf_negative(void)
{
	char buf[32];
	int len = cli_snprintf(buf, sizeof(buf), "v=%d", -123);
	TEST_ASSERT_EQUAL_STRING("v=-123", buf);
	TEST_ASSERT_EQUAL(6, len);
}
