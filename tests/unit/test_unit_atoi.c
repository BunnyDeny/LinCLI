#include "unity.h"
#include "cli_atoi.h"

void test_cli_atoi_positive(void)
{
	int val;
	char *end;
	TEST_ASSERT_EQUAL(0, cli_atoi("123", &val, &end));
	TEST_ASSERT_EQUAL(123, val);
	TEST_ASSERT_EQUAL_PTR(&"123"[3], end);
}

void test_cli_atoi_negative(void)
{
	int val;
	TEST_ASSERT_EQUAL(0, cli_atoi("-456", &val, NULL));
	TEST_ASSERT_EQUAL(-456, val);
}

void test_cli_atoi_zero(void)
{
	int val;
	TEST_ASSERT_EQUAL(0, cli_atoi("0", &val, NULL));
	TEST_ASSERT_EQUAL(0, val);
}

void test_cli_atoi_no_digits(void)
{
	int val;
	char *end;
	TEST_ASSERT_EQUAL(-1, cli_atoi("abc", &val, &end));
	TEST_ASSERT_EQUAL(0, val);
	TEST_ASSERT_EQUAL_PTR("abc", end);
}

void test_cli_atoi_spaces(void)
{
	int val;
	TEST_ASSERT_EQUAL(0, cli_atoi("  42", &val, NULL));
	TEST_ASSERT_EQUAL(42, val);
}

void test_cli_atoi_mixed(void)
{
	int val;
	char *end;
	TEST_ASSERT_EQUAL(0, cli_atoi("123abc", &val, &end));
	TEST_ASSERT_EQUAL(123, val);
	TEST_ASSERT_EQUAL_PTR("123abc" + 3, end);
}
