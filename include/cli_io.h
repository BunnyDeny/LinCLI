/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
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

#ifndef _CLI_IO_H
#define _CLI_IO_H

#include "cli_errno.h"
#include "kfifo.h"
#include <stdio.h>

#if defined(_u8)
#undef _u8
typedef volatile uint8_t _u8;
#else
typedef volatile uint8_t _u8;
#endif

#if defined(_int)
#undef _int
typedef volatile int _int;
#else
typedef volatile int _int;
#endif

#define CLI_IO_SIZE 128
#define CLI_PRINTK_BUF_SIZE CLI_IO_SIZE
#define COLOR_TERMINAL_EN 1
#define DEBUG

#if COLOR_TERMINAL_EN
#define COLOR_NONE "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_DIM "\033[2m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RAINBOW_1 "\033[38;5;196m" /* 红 */
#define COLOR_RAINBOW_2 "\033[38;5;208m" /* 橙红 */
#define COLOR_RAINBOW_3 "\033[38;5;214m" /* 橙 */
#define COLOR_RAINBOW_4 "\033[38;5;226m" /* 黄 */
#define COLOR_RAINBOW_5 "\033[38;5;046m" /* 绿 */
#define COLOR_RAINBOW_6 "\033[38;5;051m" /* 青 */
#define COLOR_RAINBOW_7 "\033[38;5;033m" /* 蓝 */
#define COLOR_RAINBOW_8 "\033[38;5;129m" /* 紫 */
#define COLOR_RAINBOW_9 "\033[38;5;201m" /* 洋红 */
#else
#define COLOR_NONE
#define COLOR_RED
#define COLOR_GREEN
#define COLOR_YELLOW
#define COLOR_BLUE
#define COLOR_MAGENTA
#define COLOR_CYAN
#define COLOR_WHITE
#define COLOR_DIM
#define COLOR_BOLD
#define COLOR_RAINBOW_1
#define COLOR_RAINBOW_2
#define COLOR_RAINBOW_3
#define COLOR_RAINBOW_4
#define COLOR_RAINBOW_5
#define COLOR_RAINBOW_6
#define COLOR_RAINBOW_7
#define COLOR_RAINBOW_8
#define COLOR_RAINBOW_9
#endif

#define KERN_EMERG "0"
#define KERN_ALERT "1"
#define KERN_CRIT "2"
#define KERN_ERR "3"
#define KERN_WARNING "4"
#define KERN_NOTICE "5"
#define KERN_INFO "6"
#define KERN_DEBUG "7"
#define KERN_DEFAULT ""

extern char log_level[3];
extern _u8 cli_in_push_lock;

int cli_printk(const char *fmt, ...);
int all_printk(const char *fmt, ...);
int sys_printk(const char *fmt, ...);
void cli_printk_batch_begin(void);
void cli_printk_batch_end(void);

#define pr_emerg(fmt, ...) sys_printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...) sys_printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...) sys_printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) sys_printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) sys_printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...) sys_printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) sys_printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) sys_printk(KERN_DEBUG fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define pr_devel(fmt, ...) sys_printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#endif

struct cli_io {
	kfifo_t in;
	_u8 in_ref;
	char in_buf[CLI_IO_SIZE];

	kfifo_t out;
	_u8 out_ref;
	char out_buf[CLI_IO_SIZE];
};

extern struct cli_io _cli_io;

int cli_in_push(_u8 *data, int size);
int cli_out_push(_u8 *data, int size);
int cli_in_pop(_u8 *data, int size);
int cli_out_pop(_u8 *data, int size);
int cli_get_in_size(void);
int cli_get_out_size(void);

void cli_io_init(void);
int cli_out_sync(void);
int cli_in_clear(void);
void set_cli_in_push_lock(void);
void reset_cli_in_push_lock(void);

#endif
