#include "cli_io.h"
#include <string.h>

#define CMD_LINE_BUF_SIZE 256

static bool is_valid_char(char c);

struct cmd_line {
	_u8 pos;
	char buf[CMD_LINE_BUF_SIZE];
	_u8 size;
} cmd_line = {
	.pos = 0,
	.size = 0,
	.buf = { 0 },
};

int cmd_line_input_valid_char(char c)
{
	int status;
	if (!is_valid_char(c)) {
		return -1;
	}
	status = cli_out_push((_u8 *)&c, 1);
	if (status < 0) {
		pr_err("[cmd_line_input_valid_char] cli_out_push异常\n");
		return status;
	} else {
		cmd_line.pos++;
		cmd_line.size++;
	}
	return 0;
}

static bool is_valid_char(char c)
{
	static const bool char_table[256] = {
		['a'] = 1, ['b'] = 1,  ['c'] = 1, ['d'] = 1,  ['e'] = 1,
		['f'] = 1, ['g'] = 1,  ['h'] = 1, ['i'] = 1,  ['j'] = 1,
		['k'] = 1, ['l'] = 1,  ['m'] = 1, ['n'] = 1,  ['o'] = 1,
		['p'] = 1, ['q'] = 1,  ['r'] = 1, ['s'] = 1,  ['t'] = 1,
		['u'] = 1, ['v'] = 1,  ['w'] = 1, ['x'] = 1,  ['y'] = 1,
		['z'] = 1, ['A'] = 1,  ['B'] = 1, ['C'] = 1,  ['D'] = 1,
		['E'] = 1, ['F'] = 1,  ['G'] = 1, ['H'] = 1,  ['I'] = 1,
		['J'] = 1, ['K'] = 1,  ['L'] = 1, ['M'] = 1,  ['N'] = 1,
		['O'] = 1, ['P'] = 1,  ['Q'] = 1, ['R'] = 1,  ['S'] = 1,
		['T'] = 1, ['U'] = 1,  ['V'] = 1, ['W'] = 1,  ['X'] = 1,
		['Y'] = 1, ['Z'] = 1,  ['0'] = 1, ['1'] = 1,  ['2'] = 1,
		['3'] = 1, ['4'] = 1,  ['5'] = 1, ['6'] = 1,  ['7'] = 1,
		['8'] = 1, ['9'] = 1,  [' '] = 1, ['~'] = 1,  ['!'] = 1,
		['@'] = 1, ['#'] = 1,  ['$'] = 1, ['%'] = 1,  ['^'] = 1,
		['&'] = 1, ['*'] = 1,  ['('] = 1, [')'] = 1,  ['-'] = 1,
		['_'] = 1, ['='] = 1,  ['+'] = 1, ['['] = 1,  [']'] = 1,
		['{'] = 1, ['}'] = 1,  ['|'] = 1, ['\\'] = 1, [';'] = 1,
		[':'] = 1, ['\''] = 1, ['"'] = 1, [','] = 1,  ['.'] = 1,
		['<'] = 1, ['>'] = 1,  ['/'] = 1, ['?'] = 1,
	};
	return char_table[(unsigned char)c];
}

int cli_dispose_char(char ch)
{
	int status;
	if (is_valid_char(ch)) {
		status = cmd_line_input_valid_char(ch);
		if (status < 0) {
			return status;
		}
		return 0;
	} else {
		switch (ch) {
		case '\n':
			pr_debug("处理回车以及命令解析的状态转换\n");
			break;
		case '\x0c':
			const char *clear_screen = "\033[H\033[2J";
			status = cli_out_push((_u8 *)clear_screen,
					      strlen(clear_screen) + 1);
			if (status < 0) {
				pr_err("[scheduler_idle_task] cli_out_push异常\n");
				return status;
			}
			break;
		default:
			break;
		}
		return 0;
	}
}