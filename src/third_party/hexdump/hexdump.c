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

#include "hexdump.h"
#include "cmd_dispose.h"
#include "cli_io.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* 默认配置 */
static struct hexdump_config hd_cfg = {
	.min_addr = HEXDUMP_MIN_ADDR,
	.max_addr = HEXDUMP_MAX_ADDR,
	.bytes_per_line = HEXDUMP_BYTES_PER_LINE,
	.max_len = HEXDUMP_MAX_LEN,
};

/* 默认读取函数：直接解引用 */
static uint8_t hexdump_default_read(uintptr_t addr)
{
	return *(volatile uint8_t *)addr;
}

static hexdump_read_fn_t hd_read_fn = hexdump_default_read;

void hexdump_set_read_fn(hexdump_read_fn_t fn)
{
	hd_read_fn = fn ? fn : hexdump_default_read;
}

hexdump_read_fn_t hexdump_get_read_fn(void)
{
	return hd_read_fn;
}

struct hexdump_config *hexdump_get_config(void)
{
	return &hd_cfg;
}

/* 判断是否为可打印字符 */
static char hd_to_printable(uint8_t c)
{
	return (c >= 32 && c < 127) ? (char)c : '.';
}

/* 解析地址（支持 0x 前缀和十进制） */
static int hd_parse_addr(const char *s, uintptr_t *out)
{
	char *end;
	unsigned long val;

	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		val = strtoul(s, &end, 16);
	else
		val = strtoul(s, &end, 10);

	if (*end != '\0')
		return -1;

	*out = (uintptr_t)val;
	return 0;
}

/* 解析整数 */
static int hd_parse_int(const char *s, int *out)
{
	char *end;
	long val = strtol(s, &end, 0);

	if (*end != '\0')
		return -1;

	*out = (int)val;
	return 0;
}

/* 打印地址前缀 */
static void hd_print_addr(uintptr_t addr)
{
	if (sizeof(uintptr_t) == 8)
		all_printk("%016llX: ", (unsigned long long)addr);
	else
		all_printk("%08X: ", (unsigned int)addr);
}

/* 打印一行十六进制值 */
static void hd_print_hex(uintptr_t addr, size_t line_len, size_t bpl)
{
	size_t j;

	for (j = 0; j < bpl; j++) {
		if (j < line_len)
			all_printk("%02X ", hd_read_fn(addr + j));
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
		all_printk("%c", hd_to_printable(hd_read_fn(addr + j)));
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
	all_printk("hexdump config:\r\n");
	if (sizeof(uintptr_t) == 8) {
		all_printk("  min_addr       : 0x%016llX\r\n",
			   (unsigned long long)hd_cfg.min_addr);
		all_printk("  max_addr       : 0x%016llX\r\n",
			   (unsigned long long)hd_cfg.max_addr);
	} else {
		all_printk("  min_addr       : 0x%08X\r\n",
			   (unsigned int)hd_cfg.min_addr);
		all_printk("  max_addr       : 0x%08X\r\n",
			   (unsigned int)hd_cfg.max_addr);
	}
	all_printk("  bytes_per_line : %u\r\n",
		   (unsigned int)hd_cfg.bytes_per_line);
	all_printk("  max_len        : %u\r\n",
		   (unsigned int)hd_cfg.max_len);
}

/* 打印地址越界错误 */
static void hd_print_addr_err(uintptr_t addr)
{
	if (sizeof(uintptr_t) == 8) {
		all_printk("Error: address 0x%016llX out of range "
			   "[0x%016llX, 0x%016llX]\r\n",
			   (unsigned long long)addr,
			   (unsigned long long)hd_cfg.min_addr,
			   (unsigned long long)hd_cfg.max_addr);
	} else {
		all_printk("Error: address 0x%08X out of range "
			   "[0x%08X, 0x%08X]\r\n",
			   (unsigned int)addr,
			   (unsigned int)hd_cfg.min_addr,
			   (unsigned int)hd_cfg.max_addr);
	}
}

/* hexdump config 处理 */
static int hexdump_config_handler(char **argv, int argc)
{
	int i;

	for (i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "--min-addr") == 0 ||
		     strcmp(argv[i], "-m") == 0) &&
		    i + 1 < argc) {
			uintptr_t val;

			if (hd_parse_addr(argv[++i], &val) != 0) {
				all_printk("Error: invalid min-addr: %s\r\n",
					   argv[i]);
				return -1;
			}
			hd_cfg.min_addr = val;
		} else if ((strcmp(argv[i], "--max-addr") == 0 ||
			    strcmp(argv[i], "-M") == 0) &&
			   i + 1 < argc) {
			uintptr_t val;

			if (hd_parse_addr(argv[++i], &val) != 0) {
				all_printk("Error: invalid max-addr: %s\r\n",
					   argv[i]);
				return -1;
			}
			hd_cfg.max_addr = val;
		} else if ((strcmp(argv[i], "--bytes-per-line") == 0 ||
			    strcmp(argv[i], "-b") == 0) &&
			   i + 1 < argc) {
			int val;

			if (hd_parse_int(argv[++i], &val) != 0 ||
			    val <= 0 || val > 64) {
				all_printk("Error: invalid bytes-per-line: "
					   "%s (1-64)\r\n",
					   argv[i]);
				return -1;
			}
			hd_cfg.bytes_per_line = (size_t)val;
		} else if (strcmp(argv[i], "--show") == 0 ||
			   strcmp(argv[i], "-s") == 0) {
			hd_print_config();
		} else {
			all_printk("Error: unknown option: %s\r\n",
				   argv[i]);
			return -1;
		}
	}

	return 0;
}

/* hexdump dump 处理 */
static int hexdump_dump_handler(char **argv, int argc)
{
	bool ascii = false;
	int arg_idx = 1;
	uintptr_t addr;
	size_t len;

	/* 解析选项 */
	while (arg_idx < argc && argv[arg_idx][0] == '-') {
		if (strcmp(argv[arg_idx], "-C") == 0 ||
		    strcmp(argv[arg_idx], "--ascii") == 0) {
			ascii = true;
			arg_idx++;
		} else {
			all_printk("Error: unknown option: %s\r\n",
				   argv[arg_idx]);
			return -1;
		}
	}

	if (arg_idx >= argc) {
		all_printk("Usage: hexdump [-C|--ascii] <addr> [len]\r\n");
		return -1;
	}

	if (hd_parse_addr(argv[arg_idx], &addr) != 0) {
		all_printk("Error: invalid address: %s\r\n",
			   argv[arg_idx]);
		return -1;
	}
	arg_idx++;

	if (arg_idx < argc) {
		int tmp;

		if (hd_parse_int(argv[arg_idx], &tmp) != 0 || tmp < 0) {
			all_printk("Error: invalid length: %s\r\n",
				   argv[arg_idx]);
			return -1;
		}
		len = (size_t)tmp;
	} else {
		len = hd_cfg.max_len;
	}

	/* 地址范围检查 */
	if (addr < hd_cfg.min_addr || addr > hd_cfg.max_addr) {
		hd_print_addr_err(addr);
		return -1;
	}

	/* 长度上限检查 */
	if (len > hd_cfg.max_len) {
		all_printk("Warning: length truncated to %u (max allowed)\r\n",
			   (unsigned int)hd_cfg.max_len);
		len = hd_cfg.max_len;
	}

	/* 结束地址溢出检查 */
	if (addr + len < addr) {
		all_printk("Error: address wrap-around\r\n");
		return -1;
	}

	if (len > 0 && addr + len - 1 > hd_cfg.max_addr) {
		all_printk("Warning: end address exceeds max_addr, "
			   "truncating\r\n");
		len = hd_cfg.max_addr - addr + 1;
	}

	hexdump_print(addr, len, ascii);
	return 0;
}

static int hexdump_handler(char **argv, int argc)
{
	if (argc >= 2 && strcmp(argv[1], "config") == 0)
		return hexdump_config_handler(argv, argc);

	return hexdump_dump_handler(argv, argc);
}

CLI_RAW_COMMAND(hexdump_cmd, "hexdump",
		"Memory dump for embedded debugging",
		USAGE("hexdump [-C|--ascii] <addr> [len]",
		      "hexdump config [-m <addr>] [-M <addr>] "
		      "[-b <n>] [-s]"),
		hexdump_handler,
		"0x08000000", "0x20000000");

#endif /* HEXDUMP */
