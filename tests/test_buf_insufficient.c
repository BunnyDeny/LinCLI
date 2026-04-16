/**
 * @file test_buf_insufficient.c
 * @brief 测试 CLI_COMMAND 与 CLI_COMMAND_WITH_BUF 在缓冲区不足时的行为。
 *
 * 本文件包含两个独立的命令：
 *
 * 1. big1 —— 使用默认共享缓冲区 g_cli_cmd_buf（大小为 CLI_CMD_BUF_SIZE）
 *    结构体仅包含一个 char[CLI_CMD_BUF_SIZE] 数组，因此结构体大小恰好等于
 *    CLI_CMD_BUF_SIZE，剩余空间为 0。注册了 1 个 BOOL 选项后，opt_seen
 *    需要 1 字节，0 < 1，会触发 "命令 big1 的参数缓冲区不足以容纳解析状态"。
 *
 * 2. big2 —— 使用 CLI_COMMAND_WITH_BUF 指定独立缓冲区 big2_buf（1 B）
 *    结构体仅包含一个 char[1]，大小为 1，剩余空间为 0。注册了 1 个选项后，
 *    opt_seen 需要 1 字节，0 < 1，同样会触发缓冲区不足错误。
 *
 * 预期终端输出：
 *   lin@linCli> big1
 *   [ERR] 命令 big1 的参数缓冲区不足以容纳解析状态
 *   [ERR] 命令解析失败: big1
 *   ...
 *
 *   lin@linCli> big2
 *   [ERR] 命令 big2 的参数缓冲区不足以容纳解析状态
 *   [ERR] 命令解析失败: big2
 *   ...
 */

#include "cmd_dispose.h"
#include "cli_io.h"

/* ============================================================
 * big1：使用 CLI_COMMAND（共享 g_cli_cmd_buf）
 * ============================================================ */

struct big1_args {
	char padding[CLI_CMD_BUF_SIZE];
};

static int big1_handler(void *_args)
{
	(void)_args;
	cli_printk("big1 should never be executed!\n");
	return 0;
}

CLI_COMMAND(big1, "big1", "Test insufficient shared buffer", big1_handler,
	    (struct big1_args *)0,
	    OPTION('a', "a", BOOL, "A", struct big1_args, padding),
	    END_OPTIONS);

/* ============================================================
 * big2：使用 CLI_COMMAND_WITH_BUF（独立缓冲区 1 B）
 * ============================================================ */

struct big2_args {
	char padding[1];
};

static char big2_buf[1];

static int big2_handler(void *_args)
{
	(void)_args;
	cli_printk("big2 should never be executed!\n");
	return 0;
}

CLI_COMMAND_WITH_BUF(big2, "big2",
		     "Test insufficient private buffer", big2_handler,
		     (struct big2_args *)0, big2_buf, sizeof(big2_buf),
		     OPTION('x', "x", BOOL, "X", struct big2_args, padding),
		     END_OPTIONS);
