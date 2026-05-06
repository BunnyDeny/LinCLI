#include "unity.h"
#include "stateM.h"
#include "cli_errno.h"

static int entry_count = 0;
static int task_count = 0;
static int exit_count = 0;
static int last_private = 0;

static void test_entry(void *private)
{
	entry_count++;
	(void)private;
}

static int test_task(void *private)
{
	task_count++;
	if (private)
		last_private = *(int *)private;
	return 0;
}

static void test_exit(void *private)
{
	exit_count++;
	(void)private;
}

static struct tState state_a = {
	.state_id = 1,
	.state_entry = test_entry,
	.state_task = test_task,
	.state_exit = test_exit,
};

static struct tState state_b = {
	.state_id = 2,
	.state_entry = test_entry,
	.state_task = test_task,
	.state_exit = test_exit,
};

static struct tState *const test_states[] = { &state_a, &state_b };

void test_statem_init_and_run(void)
{
	struct tStateEngine eng;
	int status;
	int priv = 42;

	entry_count = 0;
	task_count = 0;
	exit_count = 0;

	status = engine_init(&eng, 1, test_states,
			     test_states + sizeof(test_states) / sizeof(void *));
	TEST_ASSERT_EQUAL(0, status);

	/* 第一次运行应触发 state_a 的 entry + task */
	status = stateEngineRun(&eng, &priv);
	TEST_ASSERT_EQUAL(0, status);
	TEST_ASSERT_EQUAL(1, entry_count);
	TEST_ASSERT_EQUAL(1, task_count);
	TEST_ASSERT_EQUAL(42, last_private);

	/* 再次运行仍在 state_a，只触发 task */
	status = stateEngineRun(&eng, &priv);
	TEST_ASSERT_EQUAL(0, status);
	TEST_ASSERT_EQUAL(1, entry_count);
	TEST_ASSERT_EQUAL(2, task_count);
}

void test_statem_switch(void)
{
	struct tStateEngine eng;
	int status;

	entry_count = 0;
	task_count = 0;
	exit_count = 0;

	engine_init(&eng, 1, test_states,
		    test_states + sizeof(test_states) / sizeof(void *));

	stateEngineRun(&eng, NULL);
	TEST_ASSERT_EQUAL(1, entry_count);

	/* 切换到 state_b */
	status = state_switch(&eng, 2);
	TEST_ASSERT_EQUAL(0, status);

	/* 运行时应触发 state_a 的 exit 和 state_b 的 entry + task */
	stateEngineRun(&eng, NULL);
	TEST_ASSERT_EQUAL(1, exit_count);
	TEST_ASSERT_EQUAL(2, entry_count);
	TEST_ASSERT_EQUAL(2, task_count);
}

void test_statem_switch_same(void)
{
	struct tStateEngine eng;
	int status;

	entry_count = 0;
	task_count = 0;

	engine_init(&eng, 1, test_states,
		    test_states + sizeof(test_states) / sizeof(void *));

	stateEngineRun(&eng, NULL);
	TEST_ASSERT_EQUAL(1, task_count);

	/* 切换到同一个状态 */
	status = state_switch(&eng, 1);
	TEST_ASSERT_EQUAL(CLI_ERR_STATEM_SAME, status);

	/* 再次运行不应重复 entry */
	stateEngineRun(&eng, NULL);
	TEST_ASSERT_EQUAL(1, entry_count);
	TEST_ASSERT_EQUAL(2, task_count);
}

void test_statem_invalid_switch(void)
{
	struct tStateEngine eng;
	int status;

	engine_init(&eng, 1, test_states,
		    test_states + sizeof(test_states) / sizeof(void *));

	/* 切换到不存在的状态 */
	status = state_switch(&eng, 99);
	TEST_ASSERT_EQUAL(CLI_ERR_NOTFOUND, status);
}
