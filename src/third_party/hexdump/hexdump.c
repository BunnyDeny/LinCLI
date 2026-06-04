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
__attribute__((weak)) uint8_t hexdump_default_read(uintptr_t addr)
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

/* 计算字符串可见宽度（跳过 ANSI 转义序列） */
static int hd_visible_len(const char *s)
{
	int len = 0;
	int in_esc = 0;

	while (*s) {
		if (*s == '\033') {
			in_esc = 1;
		} else if (in_esc && *s == 'm') {
			in_esc = 0;
		} else if (!in_esc) {
			len++;
		}
		s++;
	}
	return len;
}

/* 将字符串按可见宽度居中写入缓冲区 */
static void hd_puts_centered(char *buf, int *pos, int size, const char *s,
			     int width)
{
	int vlen = hd_visible_len(s);
	int pad = width - vlen;
	int left_pad, right_pad;
	int i;

	if (pad < 0)
		pad = 0;
	left_pad = pad / 2;
	right_pad = pad - left_pad;

	for (i = 0; i < left_pad && *pos < size - 1; i++)
		buf[(*pos)++] = ' ';
	while (*s && *pos < size - 1)
		buf[(*pos)++] = *s++;
	for (i = 0; i < right_pad && *pos < size - 1; i++)
		buf[(*pos)++] = ' ';
}

/* 填充字符 */
static void hd_fill(char *buf, int *pos, int size, char c, int count)
{
	int i;

	for (i = 0; i < count && *pos < size - 1; i++)
		buf[(*pos)++] = c;
}

/* 打印表头 */
static void hd_print_header(size_t bpl, bool ascii)
{
	char line[128];
	int addr_w = 12; /* "0xXXXXXXXX: " */
	int hex_w = (int)(bpl * 3);
	int ascii_w = ascii ? (int)(bpl + 3) : 0;
	int pos = 0;

	hd_puts_centered(line, &pos, sizeof(line), COLOR_BLUE "addr" COLOR_NONE,
			 addr_w);
	hd_puts_centered(line, &pos, sizeof(line), "value", hex_w);
	if (ascii)
		hd_puts_centered(line, &pos, sizeof(line),
				 COLOR_CYAN "ascii" COLOR_NONE, ascii_w);
	line[pos] = '\0';
	cli_printk("%s\r\n", line);

	pos = 0;
	hd_fill(line, &pos, sizeof(line), '-', addr_w);
	hd_fill(line, &pos, sizeof(line), '-', hex_w);
	if (ascii)
		hd_fill(line, &pos, sizeof(line), '-', ascii_w);
	line[pos] = '\0';
	cli_printk("%s\r\n", line);
}

/* 打印一行 hexdump */
static void hd_print_line(uintptr_t addr, size_t len, size_t offset, size_t bpl,
			  bool ascii)
{
	static const char hex[] = "0123456789ABCDEF";
	uintptr_t line_addr = addr + offset;
	size_t line_len = len - offset;
	char buf[512];
	int pos = 0;
	int max_pos = (int)sizeof(buf) - 1;
	size_t j;
	char addr_buf[9];

	if (line_len > bpl)
		line_len = bpl;

	/* 地址（蓝色） */
	if (sizeof(uintptr_t) == 8 && ((unsigned long long)line_addr >> 32) != 0) {
		char addr_hi[9];
		hd_utox((unsigned int)((unsigned long long)line_addr >> 32), addr_hi, 8);
		hd_utox((unsigned int)line_addr, addr_buf, 8);
		pos += snprintf(buf + pos, sizeof(buf) - pos,
				    COLOR_BLUE "0x%s%s: " COLOR_NONE, addr_hi,
				    addr_buf);
	} else {
		hd_utox((unsigned int)line_addr, addr_buf, 8);
		pos += snprintf(buf + pos, sizeof(buf) - pos,
				    COLOR_BLUE "0x%s: " COLOR_NONE, addr_buf);
	}

	/* 十六进制 */
	for (j = 0; j < bpl && pos < max_pos; j++) {
		if (j < line_len) {
			uint8_t val = hexdump_default_read(line_addr + j);
			buf[pos++] = hex[val >> 4];
			if (pos < max_pos)
				buf[pos++] = hex[val & 0xF];
			if (pos < max_pos)
				buf[pos++] = ' ';
		} else {
			buf[pos++] = ' ';
			if (pos < max_pos)
				buf[pos++] = ' ';
			if (pos < max_pos)
				buf[pos++] = ' ';
		}
	}

	/* ASCII（青色） */
	if (ascii) {
		if (pos < max_pos)
			buf[pos++] = ' ';
		if (pos < max_pos)
			buf[pos++] = '|';
		for (j = 0; j < line_len && pos < max_pos; j++)
			buf[pos++] = hd_to_printable(
				hexdump_default_read(line_addr + j));
		for (; j < bpl && pos < max_pos; j++)
			buf[pos++] = ' ';
		if (pos < max_pos)
			buf[pos++] = '|';
	}

	buf[pos] = '\0';
	cli_printk("%s\r\n", buf);
}

static void hexdump_print(uintptr_t addr, size_t len, bool ascii)
{
	size_t i;
	size_t bpl = hd_cfg.bytes_per_line;

	if (bpl == 0)
		bpl = 16;

	hd_print_header(bpl, ascii);

	for (i = 0; i < len; i += bpl)
		hd_print_line(addr, len, i, bpl, ascii);
}

/* 打印当前配置 */
static void hd_print_config(void)
{
	char buf[9];

	cli_printk("hexdump config:\r\n");
	hd_utox((unsigned int)hd_cfg.min_addr, buf, 8);
	cli_printk("  min_addr       : 0x%s\r\n", buf);
	hd_utox((unsigned int)hd_cfg.max_addr, buf, 8);
	cli_printk("  max_addr       : 0x%s\r\n", buf);
	cli_printk("  bytes_per_line : %u\r\n",
		   (unsigned int)hd_cfg.bytes_per_line);
	cli_printk("  max_len        : %u\r\n", (unsigned int)hd_cfg.max_len);
}

/* 打印地址越界错误 */
static void hd_print_addr_err(uintptr_t addr)
{
	char buf[9];
	char msg[64];
	int pos = 0;

	hd_utox((unsigned int)addr, buf, 8);
	pos += snprintf(msg + pos, sizeof(msg) - pos,
			    "address 0x%s out of range [", buf);
	hd_utox((unsigned int)hd_cfg.min_addr, buf, 8);
	pos += snprintf(msg + pos, sizeof(msg) - pos, "0x%s, ", buf);
	hd_utox((unsigned int)hd_cfg.max_addr, buf, 8);
	pos += snprintf(msg + pos, sizeof(msg) - pos, "0x%s]", buf);

	pr_err("%s\r\n", msg);
}

/* hexdump 参数结构体 */
struct hexdump_args {
	int addr;
	int len;
	bool ascii;
	int min_addr;
	int max_addr;
	int bytes_per_line;
	int max_len;
	bool show;
	bool config;
};

static int hexdump_handler(void *_args)
{
	struct hexdump_args *args = _args;
	uintptr_t dump_addr;
	size_t dump_len;

	if (args->config) {
		hd_cfg.min_addr = (unsigned int)args->min_addr;
		hd_cfg.max_addr = (unsigned int)args->max_addr;
		if (args->max_len > 0)
			hd_cfg.max_len = (size_t)args->max_len;
		hd_print_config();
		return 0;
	}

	if (args->show) {
		hd_print_config();
		return 0;
	}

	if (args->bytes_per_line > 0)
		hd_cfg.bytes_per_line = (size_t)args->bytes_per_line;

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
		pr_warn("length truncated to %u (max allowed)\r\n",
			(unsigned int)hd_cfg.max_len);
		dump_len = hd_cfg.max_len;
	}

	/* 结束地址溢出检查 */
	if (dump_addr + dump_len < dump_addr) {
		pr_err("address wrap-around\r\n");
		return -1;
	}

	if (dump_len > 0 && dump_addr + dump_len - 1 > hd_cfg.max_addr) {
		pr_warn("end address exceeds max_addr, truncating\r\n");
		dump_len = hd_cfg.max_addr - dump_addr + 1;
	}

	hexdump_print(dump_addr, dump_len, args->ascii);
	return 0;
}

CLI_COMMAND(hexdump, "hexdump", "Memory dump for embedded debugging",
	    USAGE("hexdump -a <addr> [-l <len>] [-C] [-b <bytes>]",
		  "hexdump --config -m <addr> -M <addr> [-L <len>]"),
	    hexdump_handler, (struct hexdump_args *)0,
	    OPTION('a', "addr", INT, "Start address (hex supported)",
		   struct hexdump_args, addr, 0, NULL, NULL, false),
	    OPTION('l', "len", INT, "Length to dump", struct hexdump_args, len,
		   0, NULL, NULL, false),
	    OPTION('C', "ascii", BOOL, "Show ASCII column", struct hexdump_args,
		   ascii, 0, NULL, NULL, false),
	    OPTION('m', "min-addr", INT, "Set min valid address",
		   struct hexdump_args, min_addr, 0, "config M", NULL, false),
	    OPTION('M', "max-addr", INT, "Set max valid address",
		   struct hexdump_args, max_addr, 0, "config m", NULL, false),
	    OPTION('b', "bytes-per-line", INT, "Set bytes per line (1-64)",
		   struct hexdump_args, bytes_per_line, 0, NULL, NULL, false),
	    OPTION('L', "max-len", INT, "Set max bytes per dump (1-4096)",
		   struct hexdump_args, max_len, 0, "config", NULL, false),
	    OPTION('s', "show", BOOL, "Show current config",
		   struct hexdump_args, show, 0, NULL, NULL, false),
	    OPTION(0, "config", BOOL, "enable option m, M, L, b",
		   struct hexdump_args, config, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* HEXDUMP */
