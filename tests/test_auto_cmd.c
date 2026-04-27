/**
 * @file test_auto_cmd.c
 * @brief 开机自动执行命令测试用例。
 *
 * 系统启动后会自动顺序执行以下命令：
 *   1. tb -v
 *   2. ti -n 100
 *
 * 第三个命令故意使用 big1（会触发缓冲区不足错误），
 * 用于验证自动执行在遇到错误时会立即停止，不再执行后续命令。
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cli_auto_cmd.h"

#define CLI_AUTO_CMDS_TEST_EN 0

#if CLI_AUTO_CMDS_TEST_EN

const char *const cli_auto_cmds[] = {
	"tb -v",
	"ti -n 100",
	"big1",
	"ts -m should_not_run",
};

const int cli_auto_cmds_count =
	sizeof(cli_auto_cmds) / sizeof(cli_auto_cmds[0]);

#endif

#endif /* CLI_ENABLE_TESTS */
