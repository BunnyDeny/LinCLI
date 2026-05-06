#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* ==== test_unit_string.c ==== */
extern void test_cli_strlen_normal(void);
extern void test_cli_strcpy_normal(void);
extern void test_cli_strncpy_normal(void);
extern void test_cli_strcmp_normal(void);
extern void test_cli_strncmp_normal(void);
extern void test_cli_strcat_normal(void);
extern void test_cli_strchr_normal(void);
extern void test_cli_memcpy_normal(void);
extern void test_cli_memset_normal(void);
extern void test_cli_memmove_overlap(void);
extern void test_cli_memcmp_normal(void);

/* ==== test_unit_atoi.c ==== */
extern void test_cli_atoi_positive(void);
extern void test_cli_atoi_negative(void);
extern void test_cli_atoi_zero(void);
extern void test_cli_atoi_no_digits(void);
extern void test_cli_atoi_spaces(void);
extern void test_cli_atoi_mixed(void);

/* ==== test_unit_float.c ==== */
extern void test_cli_atof_positive(void);
extern void test_cli_atof_negative(void);
extern void test_cli_atof_integer(void);
extern void test_cli_atof_zero(void);
extern void test_cli_ftoa_positive(void);
extern void test_cli_ftoa_negative(void);
extern void test_cli_ftoa_integer(void);
extern void test_cli_ftoa_zero(void);
extern void test_cli_ftoa_buf_too_small(void);

/* ==== test_unit_mpool.c ==== */
extern void test_cli_mpool_alloc_and_free(void);
extern void test_cli_mpool_alloc_exhaust(void);
extern void test_cli_mpool_free_invalid_ptr(void);
extern void test_cli_mpool_get_usage(void);

/* ==== test_unit_vector.c ==== */
extern void test_vector_init_and_push_pop(void);
extern void test_vector_full(void);
extern void test_vector_empty_pop(void);
extern void test_vector_at_out_of_range(void);

static void run_string_tests(void)
{
	RUN_TEST(test_cli_strlen_normal);
	RUN_TEST(test_cli_strcpy_normal);
	RUN_TEST(test_cli_strncpy_normal);
	RUN_TEST(test_cli_strcmp_normal);
	RUN_TEST(test_cli_strncmp_normal);
	RUN_TEST(test_cli_strcat_normal);
	RUN_TEST(test_cli_strchr_normal);
	RUN_TEST(test_cli_memcpy_normal);
	RUN_TEST(test_cli_memset_normal);
	RUN_TEST(test_cli_memmove_overlap);
	RUN_TEST(test_cli_memcmp_normal);
}

static void run_atoi_tests(void)
{
	RUN_TEST(test_cli_atoi_positive);
	RUN_TEST(test_cli_atoi_negative);
	RUN_TEST(test_cli_atoi_zero);
	RUN_TEST(test_cli_atoi_no_digits);
	RUN_TEST(test_cli_atoi_spaces);
	RUN_TEST(test_cli_atoi_mixed);
}

static void run_float_tests(void)
{
	RUN_TEST(test_cli_atof_positive);
	RUN_TEST(test_cli_atof_negative);
	RUN_TEST(test_cli_atof_integer);
	RUN_TEST(test_cli_atof_zero);
	RUN_TEST(test_cli_ftoa_positive);
	RUN_TEST(test_cli_ftoa_negative);
	RUN_TEST(test_cli_ftoa_integer);
	RUN_TEST(test_cli_ftoa_zero);
	RUN_TEST(test_cli_ftoa_buf_too_small);
}

static void run_mpool_tests(void)
{
	RUN_TEST(test_cli_mpool_alloc_and_free);
	RUN_TEST(test_cli_mpool_alloc_exhaust);
	RUN_TEST(test_cli_mpool_free_invalid_ptr);
	RUN_TEST(test_cli_mpool_get_usage);
}

static void run_vector_tests(void)
{
	RUN_TEST(test_vector_init_and_push_pop);
	RUN_TEST(test_vector_full);
	RUN_TEST(test_vector_empty_pop);
	RUN_TEST(test_vector_at_out_of_range);
}

int main(void)
{
	UNITY_BEGIN();
	run_string_tests();
	run_atoi_tests();
	run_float_tests();
	run_mpool_tests();
	run_vector_tests();
	return UNITY_END();
}
