/**
 * @file test_init_d.c
 * @brief init_d 优先级排序测试用例。
 *
 * 测试 _EXPORT_INIT_SYMBOL 的优先级排序功能：
 *   - test_init_m1 优先级 -1（最先执行）
 *   - test_init_0  优先级 0
 *   - test_init_1  优先级 1
 *   - test_init_10 优先级 10（最后执行）
 *
 * 用于验证数字越小优先级越高，即按升序执行。
 */

#include "cli_config.h"

#ifdef CLI_ENABLE_TESTS
#include "init_d.h"
#include "cli_io.h"

#define INIT_D_TEST_EN 0

#if INIT_D_TEST_EN

static void test_init_m1(void *arg)
{
	(void)arg;
	pr_info("[init_d test] priority -1 executed\r\n");
}
_EXPORT_INIT_SYMBOL(test_init_m1, -1, NULL, test_init_m1);

static void test_init_0(void *arg)
{
	(void)arg;
	pr_info("[init_d test] priority 0 executed\r\n");
}
_EXPORT_INIT_SYMBOL(test_init_0, 0, NULL, test_init_0);

static void test_init_1(void *arg)
{
	(void)arg;
	pr_info("[init_d test] priority 1 executed\r\n");
}
_EXPORT_INIT_SYMBOL(test_init_1, 1, NULL, test_init_1);

static void test_init_10(void *arg)
{
	(void)arg;
	pr_info("[init_d test] priority 10 executed\r\n");
}
_EXPORT_INIT_SYMBOL(test_init_10, 10, NULL, test_init_10);

#endif

#endif /* CLI_ENABLE_TESTS */
