#include "cli_io.h"
#include <string.h>
#include "init_d.h"
#include "stateM.h"

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

struct tStateEngine cmd_line_mec;

extern struct tState _cli_cmd_line_start;
extern struct tState _cli_cmd_line_end;

void cmd_line_start_entry(void *pch)
{
	pr_notice("cmd_line_entry\n");
}
int cmd_line_start_task(void *pch)
{
	int status;
	pr_notice("cmd_line_task\n");
	char ch = *((char *)pch);
	if (is_valid_char(ch)) {
		status = state_switch(&cmd_line_mec, "valid_char");
		if (status < 0) {
			return status;
		}
	}
	return 0;
}
void cmd_line_start_exit(void *pch)
{
	pr_notice("cmd_line_exit\n");
}
_EXPORT_STATE_SYMBOL(cmd_line_start, cmd_line_start_entry, cmd_line_start_task,
		     cmd_line_start_exit, ".cli_cmd_line");

void valid_char_entry(void *pch)
{
	pr_notice("接收到合法字符%c\n", *((char *)pch));
}
int valid_char_task(void *pch)
{
	int status;
	char ch = *((char *)pch);
	pr_notice("valid_char_task 处理字符%c\n", ch);
	status = state_switch(&cmd_line_mec, "cmd_line_start");
	if (status < 0) {
		return status;
	}
	return 0;
}
void valid_char_exit(void *pch)
{
	pr_notice("valid_char_exit\n");
}
_EXPORT_STATE_SYMBOL(valid_char, valid_char_entry, valid_char_task,
		     valid_char_exit, ".cli_cmd_line");

void cli_cmd_line_state_mec_init(void *arg)
{
	engine_init(&cmd_line_mec, "cmd_line_start", &_cli_cmd_line_start,
		    &_cli_cmd_line_end);
}
_EXPORT_INIT_SYMBOL(cli_cmd_line, NULL, cli_cmd_line_state_mec_init);

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
	int status;
	status = stateEngineRun(&cmd_line_mec, &ch);
	if (status < 0) {
		pr_err("cli_cmd_line状态机异常\n");
		return -1;
	}
	return 0;
}