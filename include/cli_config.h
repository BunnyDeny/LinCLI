/*
 * LinCLI - Framework-wide configuration macros.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_CONFIG_H_
#define _CLI_CONFIG_H_

/*是否启用命令重命名功能*/
#define ALIAS_EN 1

/* 是否编译 tests 文件夹下的测试命令 */
#define CLI_ENABLE_TESTS 1

/*启用尾行模式测试*/
#define INLINE_TEST_EN 1

/*命令历史记录条目数量（嵌入式环境不建议太大）*/
#define HISTORY_MAX 4

/*tab补全显示的候选列表的最大列数
 如果你的终端软件窗口宽度小于这个
 数值，那么tab补全的信息可能会异常
 缺失一部分，此时请调大这个宏到合适
 值
*/
#define DISPLAY_MAX_COWS 50

/* 命令参数全局共享缓冲区大小，如果
  报错显示命令参数缺少字节，增加这个宏 */
#define CLI_CMD_BUF_SIZE 128

/* 内存池配置 */
#define CLI_MPOOL_COUNT 6

#define CLI_MPOOL_SIZE CLI_CMD_BUF_SIZE

#if CLI_MPOOL_COUNT > 32
#error "CLI_MPOOL_COUNT must not exceed 32"
#endif

/*使能logo命令*/
#define COMMAND_LOGO_EN 0

#endif
