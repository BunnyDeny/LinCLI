#include "unity.h"
#include "cli_float.h"

void test_cli_atof_positive(void)
{
	float val = cli_atof("3.14", NULL);
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14f, val);
}

void test_cli_atof_negative(void)
{
	float val = cli_atof("-2.5", NULL);
	TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.5f, val);
}

void test_cli_atof_integer(void)
{
	float val = cli_atof("42", NULL);
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, val);
}

void test_cli_atof_zero(void)
{
	float val = cli_atof("0", NULL);
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, val);
}

void test_cli_ftoa_positive(void)
{
	char buf[32];
	int len = cli_ftoa(3.14159f, buf, sizeof(buf), 2);
	TEST_ASSERT_EQUAL(4, len);
	TEST_ASSERT_EQUAL_STRING("3.14", buf);
}

void test_cli_ftoa_negative(void)
{
	char buf[32];
	int len = cli_ftoa(-2.5f, buf, sizeof(buf), 1);
	TEST_ASSERT_EQUAL(4, len);
	TEST_ASSERT_EQUAL_STRING("-2.5", buf);
}

void test_cli_ftoa_integer(void)
{
	char buf[32];
	int len = cli_ftoa(42.0f, buf, sizeof(buf), 0);
	TEST_ASSERT_EQUAL(2, len);
	TEST_ASSERT_EQUAL_STRING("42", buf);
}

void test_cli_ftoa_zero(void)
{
	char buf[32];
	cli_ftoa(0.0f, buf, sizeof(buf), 2);
	TEST_ASSERT_EQUAL_STRING("0.00", buf);
}

void test_cli_ftoa_buf_too_small(void)
{
	char buf[4];
	int len = cli_ftoa(123.456f, buf, sizeof(buf), 2);
	TEST_ASSERT_TRUE(len < 4);
	TEST_ASSERT_EQUAL_STRING("123", buf);
}
