#include "cli_io.h"
#include <string.h>
#include "init_d.h"
#include "stateM.h"

#define CMD_LINE_BUF_SIZE 256
#define cmd_line_exit 1

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

struct tStateEngine cmd_line_mec;

extern struct tState _cli_cmd_line_start;
extern struct tState _cli_cmd_line_end;

void cli_cmd_line_state_mec_init(void *arg)
{
	engine_init(&cmd_line_mec, "cmd_line_start", &_cli_cmd_line_start,
		    &_cli_cmd_line_end);
}
_EXPORT_INIT_SYMBOL(cli_cmd_line, NULL, cli_cmd_line_state_mec_init);

int cmd_line_start_task(void *pch)
{
	int status;
	char ch = *((char *)pch);
	if (is_valid_char(ch)) {
		status = state_switch(&cmd_line_mec, "valid_char");
		if (status < 0) {
			return status;
		}
	} else {
		status = state_switch(&cmd_line_mec, "unvalid_char");
		if (status < 0) {
			return status;
		}
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(cmd_line_start, NULL, cmd_line_start_task, NULL,
		     ".cli_cmd_line");

int valid_char_task(void *pch)
{
	int status;
	char ch = *((char *)pch);
	pr_debug("valid_char_task 处理字符%c\n", ch);
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(valid_char, NULL, valid_char_task, NULL, ".cli_cmd_line");

int unvalid_char_task(void *pch)
{
	int status;
	char ch = *((char *)pch);
	pr_debug("valid_char_task 非法字符, 对应ascci: %d\n", (int)ch);
	if ((int)ch == 27) { // ESC
		status = state_switch(&cmd_line_mec, "ESC_handler");
		if (status < 0) {
			return status;
		}
	} else {
		status = state_switch(&cmd_line_mec, "exit_handler");
		if (status < 0) {
			return status;
		}
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(unvalid_char, NULL, unvalid_char_task, NULL,
		     ".cli_cmd_line");

int ESC_handler(void *pch)
{
	int status, esc_params_count = 2;
	char esc_params[2];
	pr_debug("ESC_handler\n");
	while (esc_params_count) {
		if (cli_get_in_size()) {
			char ch;
			status = cli_in_pop((_u8 *)&ch, 1);
			if (status < 0) {
				return status;
			}
			esc_params[2 - esc_params_count] = ch;
			esc_params_count--;
		}
	}
	for (int i = 0; i < 2; i++) {
		pr_debug("esc参数%d : %c\n", i, esc_params[i]);
	}
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(ESC_handler, NULL, ESC_handler, NULL, ".cli_cmd_line");

int cmd_line_exit_handler(void *pch)
{
	engine_init(&cmd_line_mec, "cmd_line_start", &_cli_cmd_line_start,
		    &_cli_cmd_line_end);
	return cmd_line_exit;
}
_EXPORT_STATE_SYMBOL(exit_handler, NULL, cmd_line_exit_handler, NULL,
		     ".cli_cmd_line");

__attribute__((used)) static bool is_valid_char(char c)
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

int cli_cmd_line_task(char ch)
{
	int status = 0;
	while (status != cmd_line_exit) {
		status = stateEngineRun(&cmd_line_mec, &ch);
		if (status < 0) {
			pr_err("cli_cmd_line状态机异常\n");
			return -1;
		}
	}
	return 0;
}