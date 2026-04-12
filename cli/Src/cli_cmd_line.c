#include "cli_io.h"
#include <string.h>
#include "init_d.h"
#include "stateM.h"
#include "cli_cmd_line.h"

static bool is_valid_char(char c);

struct cmd_line cmd_line = {
	.pos = 0,
	.size = 0,
	.buf = { 0 },
};

struct origin_cmd origin_cmd = {
	.size = 0,
};

struct tStateEngine cmd_line_mec;

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
	if (cmd_line.size == CMD_LINE_BUF_SIZE) {
		pr_warn("超出单个命令最长限制\n");
		goto label_cmd_line_exit;
	}
	if (cmd_line.pos == cmd_line.size) {
		status = cli_out_push((_u8 *)pch, 1);
		if (status < 0) {
			return -1;
		}
		cmd_line.buf[cmd_line.pos] = ch;
		goto label_size_pos_inc;
	} else if (cmd_line.pos < cmd_line.size) {
		for (int i = cmd_line.size + 1; i >= cmd_line.pos + 1; i--) {
			cmd_line.buf[i] = cmd_line.buf[i - 1];
		}
		cmd_line.buf[cmd_line.pos] = ch;
		status = cli_out_push((_u8 *)&cmd_line.buf[cmd_line.pos],
				      cmd_line.size - cmd_line.pos + 1);
		if (status < 0) {
			return -1;
		}
		if (cli_out_sync()) {
			return -1;
		}
		int pos_move_cnt = cmd_line.size - cmd_line.pos;
		while (pos_move_cnt--) {
			status = cli_out_push((_u8 *)"\033[D", 4);
			if (status < 0) {
				return -1;
			}
			if (cli_out_sync()) {
				return -1;
			}
		}
		goto label_size_pos_inc;
	}
label_size_pos_inc:
	cmd_line.size++;
	cmd_line.pos++;
label_cmd_line_exit:
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
	if ((int)ch == 27) { // ESC
		status = state_switch(&cmd_line_mec, "ESC_handler");
		if (status < 0) {
			return status;
		}
	} else if ((int)ch == 127) { //backspace
		status = state_switch(&cmd_line_mec, "backspace_handler");
		if (status < 0) {
			return status;
		}
	} else if (ch == '\n') {
		status = state_switch(&cmd_line_mec, "enter");
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
	char esc_params[2], ch;
	while (esc_params_count) {
		if (cli_get_in_size()) {
			status = cli_in_pop((_u8 *)&ch, 1);
			if (status < 0) {
				return status;
			}
			esc_params[2 - esc_params_count] = ch;
			esc_params_count--;
		}
	}
	if (esc_params[1] == 'D' && cmd_line.pos > 0) {
		char *left_move = "\x1b[D";
		status = cli_out_push((_u8 *)left_move, strlen(left_move) + 1);
		if (status < 0) {
			return -1;
		}
		cmd_line.pos--;
	} else if (esc_params[1] == 'C' && cmd_line.pos < cmd_line.size) {
		char *left_move = "\x1b[C";
		status = cli_out_push((_u8 *)left_move, strlen(left_move) + 1);
		if (status < 0) {
			return -1;
		}
		cmd_line.pos++;
	} else if (esc_params[1] == '3') {
		status = cli_in_pop((_u8 *)&ch, 1);
		if (status < 0) {
			return status;
		}
		if (ch == '~') {
			status = state_switch(&cmd_line_mec, "delete");
			if (status < 0) {
				return status;
			}
			return 0;
		}
	}
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(ESC_handler, NULL, ESC_handler, NULL, ".cli_cmd_line");

int delete(void *pch)
{
	int status;
	if (cmd_line.pos == cmd_line.size) {
		goto delete_exit;
	} else if (cmd_line.pos < cmd_line.size) {
		for (int i = cmd_line.pos; i < cmd_line.size; i++) {
			cmd_line.buf[i] = cmd_line.buf[i + 1];
		}
		cmd_line.buf[cmd_line.size - 1] = ' ';
		int writeNums = cmd_line.size - cmd_line.pos;
		status = cli_out_push((_u8 *)&cmd_line.buf[cmd_line.pos],
				      writeNums);
		if (status < 0) {
			return -1;
		}
		if (cli_out_sync()) {
			return -1;
		}
		int pos_move_cnt = cmd_line.size - cmd_line.pos;
		while (pos_move_cnt--) {
			status = cli_out_push((_u8 *)"\033[D", 4);
			if (status < 0) {
				return -1;
			}
			if (cli_out_sync()) {
				return -1;
			}
		}
		cmd_line.size--;
	}
delete_exit:
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(delete, NULL, delete, NULL, ".cli_cmd_line");

int backspace_handler(void *pch)
{
	int status;
	if (cmd_line.pos != 0 && cmd_line.pos == cmd_line.size) {
		status = cli_out_push((_u8 *)"\b \b", 4);
		if (status < 0) {
			return -1;
		}
		goto label_size_pos_dis;
	} else if (cmd_line.pos == 0) {
		goto label_exit2;
	} else if (cmd_line.pos < cmd_line.size) {
		status = cli_out_push((_u8 *)"\b \b", 4);
		if (status < 0) {
			return -1;
		}
		for (int i = cmd_line.pos - 1; i < cmd_line.size - 1; i++) {
			cmd_line.buf[i] = cmd_line.buf[i + 1];
		}
		cmd_line.buf[cmd_line.size - 1] = ' ';
		status = cli_out_push((_u8 *)&cmd_line.buf[cmd_line.pos - 1],
				      cmd_line.size - cmd_line.pos + 1);
		if (status < 0) {
			return -1;
		}
		if (cli_out_sync()) {
			return -1;
		}
		int pos_move_cnt = cmd_line.size - cmd_line.pos + 1;
		while (pos_move_cnt--) {
			status = cli_out_push((_u8 *)"\033[D", 4);
			if (status < 0) {
				return -1;
			}
			if (cli_out_sync()) {
				return -1;
			}
		}
	}
label_size_pos_dis:
	cmd_line.size--;
	cmd_line.pos--;
label_exit2:
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(backspace_handler, NULL, backspace_handler, NULL,
		     ".cli_cmd_line");

void enter_entry(void *pch)
{
	memset(origin_cmd.buf, 0, CMD_LINE_BUF_SIZE);
}
int enter_press(void *pch)
{
	//int status;
	origin_cmd.size = cmd_line.size;
	for (int i = 0; i < origin_cmd.size; i++) {
		origin_cmd.buf[i] = cmd_line.buf[i];
	}
	memset(cmd_line.buf, 0, CMD_LINE_BUF_SIZE);
	cmd_line.size = 0;
	cmd_line.pos = 0;
	set_cli_in_push_lock();
	return cmd_line_enter_press;
}
_EXPORT_STATE_SYMBOL(enter, enter_entry, enter_press, NULL, ".cli_cmd_line");

int cmd_line_exit_handler(void *pch)
{
	int status = state_switch(&cmd_line_mec, "cmd_line_start");
	if (status < 0) {
		return status;
	}
	reset_cli_in_push_lock();
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
		if (status == cmd_line_enter_press) {
			return status;
		}
	}
	return 0;
}
