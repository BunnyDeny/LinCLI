/*
 * LinCLI - Variable export demo for CLI_ENABLE_TESTS.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cli_var.h"

/* ============================================================
 * 示例变量（验证用，生产环境可删除或保留作为 demo）
 * ============================================================ */

static int g_loop_count = 0;
CLI_VAR(g_loop_count, "g_loop_count", INT, "Main loop counter");

static char g_device_name[32] = "lincli-dev";
CLI_VAR(g_device_name, "g_device_name", STRING, "Device name");

static bool g_verbose = false;
CLI_VAR(g_verbose, "g_verbose", BOOL, "Verbose output flag");

static double g_kp = 2.5;
CLI_VAR_RO(g_kp, "g_kp", DOUBLE, "PID Kp parameter (read-only)");

#endif /* CLI_ENABLE_TESTS */
