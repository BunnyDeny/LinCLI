/*
 * LinCLI - Lightweight hexdump utility for embedded/MCU debugging.
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
#include "cli_kconfig.h"

#ifdef HEXDUMP

#include "cmd_dispose.h"
#include "cli_io.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* 默认配置 */
static struct {
	uintptr_t min_addr;
	uintptr_t max_addr;
	size_t bytes_per_line;
	size_t max_len;
} hd_cfg = {
	.min_addr = HEXDUMP_MIN_ADDR,
	.max_addr = HEXDUMP_MAX_ADDR,
	.bytes_per_line = HEXDUMP_BYTES_PER_LINE,
	.max_len = HEXDUMP_MAX_LEN,
};

/* 弱符号：用户可在其他文件中覆盖以实现自定义读取 */
uint8_t hexdump_default_read(uintptr_t addr) __attribute__((weak));

uint8_t hexdump_default_read(uintptr_t addr)
{
	return *(volatile uint8_t *)addr;
}

/* 判断是否为可打印字符 */
static char hd_to_printable(uint8_t c)
{
	return (c >= 32 && c < 127) ? (char)c : '.';
}

/* 将 unsigned int 转为固定 digit 的十六进制字符串 */
static void hd_utox(unsigned int val, char *buf, int digits)
{
	static const char hex[] = "0123456789ABCDEF";
	int i;

	for (i = digits - 1; i >= 0; i--) {
		buf[i] = hex[val & 0xF];
		val >>= 4;
	}
	buf[digits] = '\0';
}

/* 打印地址前缀 */
static void hd_print_addr(uintptr_t addr)
{
	char buf[17];

	if (sizeof(uintptr_t) == 8 && (addr >> 32) != 0) {
		hd_utox((unsigned int)(addr >> 32), buf, 8);
		all_printk("%s", buf);
		hd_utox((unsigned int)addr, buf, 8);
		all_printk("%s: ", buf);
	} else {
		hd_utox((unsigned int)addr, buf, 8);
		all_printk("%s: ", buf);
	}
}

/* 打印单个字节的十六进制 */
static void hd_print_byte_hex(uint8_t val)
{
	static const char hex[] = "0123456789ABCDEF";
	char buf[4];

	buf[0] = hex[val >> 4];
	buf[1] = hex[val & 0xF];
	buf[2] = ' ';
	buf[3] = '\0';
	all_printk("%s", buf);
}

/* 打印一行十六进制值 */
static void hd_print_hex(uintptr_t addr, size_t line_len, size_t bpl)
{
	size_t j;

	for (j = 0; j < bpl; j++) {
		if (j < line_len)
			hd_print_byte_hex(hexdump_default_read(addr + j));
		else
			all_printk("   ");
	}
}

/* 打印一行 ASCII 值 */
static void hd_print_ascii(uintptr_t addr, size_t line_len, size_t bpl)
{
	size_t j;

	all_printk(" |");
	for (j = 0; j < line_len; j++)
		all_printk("%c",
			   hd_to_printable(hexdump_default_read(addr + j)));
	for (; j < bpl; j++)
		all_printk(" ");
	all_printk("|");
}

/* 打印一行 hexdump */
static void hd_print_line(uintptr_t addr, size_t len, size_t offset,
			  size_t bpl, bool ascii)
{
	uintptr_t line_addr = addr + offset;
	size_t line_len = len - offset;

	if (line_len > bpl)
		line_len = bpl;

	hd_print_addr(line_addr);
	hd_print_hex(line_addr, line_len, bpl);
	if (ascii)
		hd_print_ascii(line_addr, line_len, bpl);
	all_printk("\r\n");
}

static void hexdump_print(uintptr_t addr, size_t len, bool ascii)
{
	size_t i;
	size_t bpl = hd_cfg.bytes_per_line;

	if (bpl == 0)
		bpl = 16;

	for (i = 0; i < len; i += bpl)
		hd_print_line(addr, len, i, bpl, ascii);
}

/* 打印当前配置 */
static void hd_print_config(void)
{
	char buf[9];

	all_printk("hexdump config:\r\n");
	hd_utox((unsigned int)hd_cfg.min_addr, buf, 8);
	all_printk("  min_addr       : 0x%s\r\n", buf);
	hd_utox((unsigned int)hd_cfg.max_addr, buf, 8);
	all_printk("  max_addr       : 0x%s\r\n", buf);
	all_printk("  bytes_per_line : %u\r\n",
		   (unsigned int)hd_cfg.bytes_per_line);
	all_printk("  max_len        : %u\r\n",
		   (unsigned int)hd_cfg.max_len);
}

/* 打印地址越界错误 */
static void hd_print_addr_err(uintptr_t addr)
{
	char buf[9];

	hd_utox((unsigned int)addr, buf, 8);
	all_printk("Error: address 0x%s out of range [", buf);
	hd_utox((unsigned int)hd_cfg.min_addr, buf, 8);
	all_printk("0x%s, ", buf);
	hd_utox((unsigned int)hd_cfg.max_addr, buf, 8);
	all_printk("0x%s]\r\n", buf);
}

/* hexdump 参数结构体 */
struct hexdump_args {
	int addr;
	int len;
	bool ascii;
	int min_addr;
	int max_addr;
	int bytes_per_line;
	bool show;
};

static int hexdump_handler(void *_args)
{
	struct hexdump_args *args = _args;
	uintptr_t dump_addr;
	size_t dump_len;
	bool did_config;

	/* 先处理配置类选项（冷门用法，允许同一条命令先配后显） */
	if (args->min_addr)
		hd_cfg.min_addr = (unsigned int)args->min_addr;
	if (args->max_addr)
		hd_cfg.max_addr = (unsigned int)args->max_addr;
	if (args->bytes_per_line > 0)
		hd_cfg.bytes_per_line = (size_t)args->bytes_per_line;

	did_config = args->min_addr || args->max_addr ||
		     args->bytes_per_line > 0;

	if (args->show) {
		hd_print_config();
		return 0;
	}

	/* 没有 --addr 且没有做任何配置，才报 usage */
	if (args->addr == 0 && !did_config) {
		all_printk("Usage: hexdump -a <addr> [-l <len>] [-C]\r\n");
		return -1;
	}

	/* 只配置了参数，不 dump */
	if (args->addr == 0)
		return 0;

	dump_addr = (unsigned int)args->addr;

	if (args->len > 0)
		dump_len = (size_t)(unsigned int)args->len;
	else
		dump_len = hd_cfg.max_len;

	/* 地址范围检查 */
	if (dump_addr < hd_cfg.min_addr || dump_addr > hd_cfg.max_addr) {
		hd_print_addr_err(dump_addr);
		return -1;
	}

	/* 长度上限检查 */
	if (dump_len > hd_cfg.max_len) {
		all_printk("Warning: length truncated to %u (max allowed)\r\n",
			   (unsigned int)hd_cfg.max_len);
		dump_len = hd_cfg.max_len;
	}

	/* 结束地址溢出检查 */
	if (dump_addr + dump_len < dump_addr) {
		all_printk("Error: address wrap-around\r\n");
		return -1;
	}

	if (dump_len > 0 && dump_addr + dump_len - 1 > hd_cfg.max_addr) {
		all_printk("Warning: end address exceeds max_addr, "
			   "truncating\r\n");
		dump_len = hd_cfg.max_addr - dump_addr + 1;
	}

	hexdump_print(dump_addr, dump_len, args->ascii);
	return 0;
}

CLI_COMMAND(hexdump, "hexdump", "Memory dump for embedded debugging",
	    USAGE("hexdump -a <addr> [-l <len>] [-C]"),
	    hexdump_handler, (struct hexdump_args *)0,
	    OPTION('a', "addr", INT, "Start address (hex supported)",
		   struct hexdump_args, addr, 0, NULL, NULL, false),
	    OPTION('l', "len", INT, "Length to dump",
		   struct hexdump_args, len, 0, NULL, NULL, false),
	    OPTION('C', "ascii", BOOL, "Show ASCII column",
		   struct hexdump_args, ascii, 0, NULL, NULL, false),
	    OPTION('m', "min-addr", INT, "Set min valid address",
		   struct hexdump_args, min_addr, 0, NULL, NULL, false),
	    OPTION('M', "max-addr", INT, "Set max valid address",
		   struct hexdump_args, max_addr, 0, NULL, NULL, false),
	    OPTION('b', "bytes-per-line", INT,
		   "Set bytes per line (1-64)",
		   struct hexdump_args, bytes_per_line, 0, NULL, NULL, false),
	    OPTION('s', "show", BOOL, "Show current config",
		   struct hexdump_args, show, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* HEXDUMP */
