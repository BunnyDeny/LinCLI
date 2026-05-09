/*
 * LinCLI - Environment variable test case.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "cli_config.h"

#if CLI_ENABLE_DEMO_ENV && CLI_ENABLE_ENV
#include "cmd_dispose.h"
#include "cli_io.h"
#include "cli_env.h"

/* ============================================================
 *  测试环境变量注册
 * ============================================================ */

CLI_ENV(GREETING, "Hello LinCLI");
CLI_ENV(PROJECT_NAME, "LinCLI-Framework");
CLI_ENV(EMPTY_VAR, "");

/* 特殊字符测试 */
CLI_ENV(TEST_DASH, "hello -v");
CLI_ENV(TEST_CHAIN, "echo hello && echo world");
CLI_ENV(TEST_MIX, "hello-world");

/* 纯整数名字，应被运行时忽略 */
CLI_ENV(123, "should_be_ignored");

#endif /* CLI_ENABLE_DEMO_ENV && CLI_ENABLE_ENV */
