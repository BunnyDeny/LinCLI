#include "unity.h"
#include "tVector.h"

void test_vector_init_and_push_pop(void)
{
	_u8 buf[16];
	struct vector v;
	vectorInit(&v, buf, 16);

	TEST_ASSERT_TRUE(push_back(&v, (_u8 *)"a", 1));
	TEST_ASSERT_TRUE(push_back(&v, (_u8 *)"b", 1));
	TEST_ASSERT_EQUAL(2, v.size);

	_u8 ch;
	TEST_ASSERT_TRUE(at(&v, 0, &ch));
	TEST_ASSERT_EQUAL('a', ch);
	TEST_ASSERT_TRUE(at(&v, 1, &ch));
	TEST_ASSERT_EQUAL('b', ch);

	TEST_ASSERT_TRUE(pop_front(&v, 1));
	TEST_ASSERT_EQUAL(1, v.size);
	TEST_ASSERT_TRUE(at(&v, 0, &ch));
	TEST_ASSERT_EQUAL('b', ch);
}

void test_vector_full(void)
{
	_u8 buf[4];
	struct vector v;
	vectorInit(&v, buf, 4);

	TEST_ASSERT_TRUE(push_back(&v, (_u8 *)"abcd", 4));
	TEST_ASSERT_EQUAL(4, v.size);

	/* 已满，再 push 应失败 */
	TEST_ASSERT_FALSE(push_back(&v, (_u8 *)"e", 1));
}

void test_vector_empty_pop(void)
{
	_u8 buf[4];
	struct vector v;
	vectorInit(&v, buf, 4);

	/* 空队列 pop 应失败 */
	TEST_ASSERT_FALSE(pop_front(&v, 1));
	TEST_ASSERT_FALSE(pop_back(&v, 1));
}

void test_vector_at_out_of_range(void)
{
	_u8 buf[4];
	struct vector v;
	vectorInit(&v, buf, 4);

	_u8 ch;
	TEST_ASSERT_FALSE(at(&v, 0, &ch));

	push_back(&v, (_u8 *)"a", 1);
	TEST_ASSERT_TRUE(at(&v, 0, &ch));
	TEST_ASSERT_FALSE(at(&v, 1, &ch));
}
