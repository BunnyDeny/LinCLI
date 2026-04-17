/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "cli_io.h"
#include <string.h>
#include "init_d.h"
#include "stateM.h"
#include "cli_cmd_line.h"
#include "cmd_dispose.h"

extern struct tState _cli_cmd_line_start;
extern struct tState _cli_cmd_line_end;

static bool is_valid_char(char c);

struct cmd_line {
	_u8 pos;
	char buf[CMD_LINE_BUF_SIZE];
	_u8 size;
};
struct cmd_line cmd_line = {
	.pos = 0,
	.size = 0,
	.buf = { 0 },
};

struct origin_cmd origin_cmd = {
	.size = 0,
};

struct tStateEngine cmd_line_mec;

/* ============================================================
 * 命令历史记录
 * ============================================================ */

struct history {
	char buf[HISTORY_MAX][CMD_LINE_BUF_SIZE];
	_u8 count;
	_u8 index; /* 0: 当前编辑行；1: 最新历史；2: 第二条；以此类推 */
};

static struct history history = {
	.count = 0,
	.index = 0,
};

static void history_save(const char *cmd, int size)
{
	if (size <= 0)
		return;

	if (history.count > 0 && (int)strlen(history.buf[0]) == size &&
	    memcmp(history.buf[0], cmd, size) == 0) {
		history.index = 0;
		return;
	}

	for (int i = HISTORY_MAX - 1; i > 0; i--) {
		memcpy(history.buf[i], history.buf[i - 1], CMD_LINE_BUF_SIZE);
	}
	memset(history.buf[0], 0, CMD_LINE_BUF_SIZE);
	memcpy(history.buf[0], cmd, size);

	if (history.count < HISTORY_MAX)
		history.count++;
	history.index = 0;
}

static void cmd_line_replace(const char *new_buf, int new_size)
{
	int status;

	status = cli_out_push((_u8 *)"\r\033[K", 4);
	if (status < 0)
		return;
	if (cli_out_sync())
		return;

	void cli_prompt_print(void);
	cli_prompt_print();

	if (new_size > 0) {
		status = cli_out_push((_u8 *)new_buf, new_size);
		if (status < 0)
			return;
		if (cli_out_sync())
			return;
	}

	memset(cmd_line.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(cmd_line.buf, new_buf, new_size);
	cmd_line.size = new_size;
	cmd_line.pos = new_size;
}

static int get_last_token_start(const char *buf, int size)
{
	if (size == 0)
		return 0;
	if (buf[size - 1] == ' ')
		return size;
	int i = size - 1;
	while (i >= 0 && buf[i] != ' ')
		i--;
	return i + 1;
}

static void cmd_line_redraw(void)
{
	int status;
	status = cli_out_push((_u8 *)"\r\033[K", 4);
	if (status < 0)
		return;
	if (cli_out_sync())
		return;
	void cli_prompt_print(void);
	cli_prompt_print();
	if (cmd_line.size > 0) {
		status = cli_out_push((_u8 *)cmd_line.buf, cmd_line.size);
		if (status < 0)
			return;
		if (cli_out_sync())
			return;
	}
	int back = cmd_line.size - cmd_line.pos;
	while (back-- > 0) {
		status = cli_out_push((_u8 *)"\033[D", 4);
		if (status < 0)
			return;
		if (cli_out_sync())
			return;
	}
}

static int str_common_prefix_len(const char *a, const char *b)
{
	int i = 0;
	while (a[i] && b[i] && a[i] == b[i])
		i++;
	return i;
}

static const cli_command_t *find_cmd_by_name(const char *name)
{
	cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(&_cli_commands_start, &_cli_commands_end, cmd)
	{
		if (cmd->name && strcmp(cmd->name, name) == 0)
			return cmd;
	}
	return NULL;
}

static void complete_command_name(const char *prefix, int prefix_len)
{
	const cli_command_t *match = NULL;
	int match_cnt = 0;
	cli_command_t *cmd;

	_FOR_EACH_CLI_COMMAND(&_cli_commands_start, &_cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, prefix, prefix_len) == 0) {
			match_cnt++;
			if (match_cnt == 1)
				match = cmd;
		}
	}

	if (match_cnt == 1) {
		int new_size = strlen(match->name);
		cmd_line_replace(match->name, new_size);
		if (cmd_line.size < CMD_LINE_BUF_SIZE - 1) {
			cmd_line.buf[cmd_line.size] = ' ';
			cmd_line.size++;
			cmd_line.pos++;
			cli_out_push((_u8 *)" ", 1);
			cli_out_sync();
		}
	} else if (match_cnt > 1) {
		int lcp_len = (int)strlen(match->name);
		char lcp[CMD_LINE_BUF_SIZE];
		memcpy(lcp, match->name, lcp_len);

		cli_command_t *cmd2;
		_FOR_EACH_CLI_COMMAND(&_cli_commands_start, &_cli_commands_end,
				      cmd2)
		{
			if (!cmd2->name)
				continue;
			if (strncmp(cmd2->name, prefix, prefix_len) != 0)
				continue;
			int cpl = str_common_prefix_len(lcp, cmd2->name);
			if (cpl < lcp_len)
				lcp_len = cpl;
		}

		if (lcp_len > prefix_len) {
			cmd_line_replace(lcp, lcp_len);
		} else {
			cli_out_push((_u8 *)"\a\n", 2);
			_FOR_EACH_CLI_COMMAND(&_cli_commands_start,
					      &_cli_commands_end, cmd)
			{
				if (!cmd->name)
					continue;
				if (strncmp(cmd->name, prefix, prefix_len) ==
				    0) {
					cli_out_push((_u8 *)cmd->name,
						     strlen(cmd->name));
					cli_out_push((_u8 *)"  ", 2);
				}
			}
			cli_out_push((_u8 *)"\n", 1);
			cli_out_sync();
			cmd_line_redraw();
		}
	} else {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
	}
}

static void list_all_options(const cli_command_t *cmd)
{
	cli_out_push((_u8 *)"\a\n", 2);
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (opt->short_opt) {
			char buf[4] = { '-', opt->short_opt, ' ', '\0' };
			cli_out_push((_u8 *)buf, 3);
		}
		if (opt->long_opt) {
			cli_out_push((_u8 *)"--", 2);
			cli_out_push((_u8 *)opt->long_opt,
				     strlen(opt->long_opt));
			cli_out_push((_u8 *)"  ", 2);
		}
	}
	cli_out_push((_u8 *)"\n", 1);
	cli_out_sync();
	cmd_line_redraw();
}

static void list_long_options(const cli_command_t *cmd)
{
	cli_out_push((_u8 *)"\a\n", 2);
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt) {
			cli_out_push((_u8 *)"--", 2);
			cli_out_push((_u8 *)opt->long_opt,
				     strlen(opt->long_opt));
			cli_out_push((_u8 *)"  ", 2);
		}
	}
	cli_out_push((_u8 *)"\n", 1);
	cli_out_sync();
	cmd_line_redraw();
}

static void complete_short_option(char c)
{
	cli_out_push((_u8 *)"\a", 1);
	cli_out_sync();
}

static void do_complete_short_option(char c, const cli_command_t *cmd)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].short_opt == c) {
			if (cmd_line.size < CMD_LINE_BUF_SIZE - 1) {
				cmd_line.buf[cmd_line.size] = ' ';
				cmd_line.size++;
				cmd_line.pos++;
				cli_out_push((_u8 *)" ", 1);
				cli_out_sync();
			}
			return;
		}
	}
	complete_short_option(c);
}

static void list_long_option_candidates(const cli_command_t *cmd,
					const char *name_prefix,
					int name_prefix_len)
{
	cli_out_push((_u8 *)"\a\n", 2);
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, name_prefix, name_prefix_len) == 0) {
			cli_out_push((_u8 *)"--", 2);
			cli_out_push((_u8 *)opt->long_opt,
				     strlen(opt->long_opt));
			cli_out_push((_u8 *)"  ", 2);
		}
	}
	cli_out_push((_u8 *)"\n", 1);
	cli_out_sync();
	cmd_line_redraw();
}

static void replace_long_option(const char *long_opt, int long_len)
{
	char new_buf[CMD_LINE_BUF_SIZE];
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	memcpy(new_buf, cmd_line.buf, tok_start);
	new_buf[tok_start] = '-';
	new_buf[tok_start + 1] = '-';
	memcpy(new_buf + tok_start + 2, long_opt, long_len);
	int new_size = tok_start + 2 + long_len;
	if (new_size < CMD_LINE_BUF_SIZE - 1) {
		new_buf[new_size] = ' ';
		new_size++;
	}
	cmd_line_replace(new_buf, new_size);
}

static void complete_long_option(const cli_command_t *cmd,
				 const char *name_prefix, int name_prefix_len)
{
	const cli_option_t *match = NULL;
	int match_cnt = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, name_prefix, name_prefix_len) == 0) {
			match_cnt++;
			if (match_cnt == 1)
				match = opt;
		}
	}

	if (match_cnt == 1) {
		replace_long_option(match->long_opt,
				    (int)strlen(match->long_opt));
	} else if (match_cnt > 1) {
		int lcp_len = (int)strlen(match->long_opt);
		char lcp[CMD_LINE_BUF_SIZE];
		memcpy(lcp, match->long_opt, lcp_len);

		for (size_t i = 0; i < cmd->option_count; i++) {
			const cli_option_t *opt = &cmd->options[i];
			if (!opt->long_opt ||
			    strncmp(opt->long_opt, name_prefix,
				    name_prefix_len) != 0)
				continue;
			int cpl = str_common_prefix_len(lcp, opt->long_opt);
			if (cpl < lcp_len)
				lcp_len = cpl;
		}

		if (lcp_len > name_prefix_len) {
			replace_long_option(lcp, lcp_len);
		} else {
			list_long_option_candidates(cmd, name_prefix,
						    name_prefix_len);
		}
	} else {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
	}
}

static void complete_option(const cli_command_t *cmd, const char *prefix,
			    int prefix_len)
{
	if (prefix_len == 0) {
		list_all_options(cmd);
		return;
	}
	if (prefix_len == 1 && prefix[0] == '-') {
		list_all_options(cmd);
		return;
	}
	if (prefix_len == 2 && prefix[0] == '-' && prefix[1] == '-') {
		list_long_options(cmd);
		return;
	}
	if (prefix_len == 2 && prefix[0] == '-' && prefix[1] != '-') {
		do_complete_short_option(prefix[1], cmd);
		return;
	}
	if (prefix_len >= 3 && prefix[0] == '-' && prefix[1] == '-') {
		complete_long_option(cmd, prefix + 2, prefix_len - 2);
		return;
	}
	cli_out_push((_u8 *)"\a", 1);
	cli_out_sync();
}

/* ============================================================
 * 状态机函数
 * ============================================================ */

int cli_cmd_line_init(void)
{
	int status = engine_init(&cmd_line_mec, "cmd_line_start",
				 &_cli_cmd_line_start, &_cli_cmd_line_end);
	if (status < 0) {
		return status;
	}
	return 0;
}

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
	char ch = *((char *)pch);
	char *next_state;

	switch ((unsigned char)ch) {
	case 27:
		next_state = "ESC_handler";
		break;
	case 127:
		next_state = "backspace_handler";
		break;
	case '\n':
		next_state = "enter";
		break;
	case '\t':
		next_state = "tab_complete";
		break;
	case 12:
		next_state = "clear";
		break;
	default:
		next_state = "exit_handler";
		break;
	}

	int status = state_switch(&cmd_line_mec, next_state);
	if (status < 0)
		return status;
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
	} else if (esc_params[1] == 'A') {
		status = state_switch(&cmd_line_mec, "history_up");
		if (status < 0) {
			return status;
		}
		return 0;
	} else if (esc_params[1] == 'B') {
		status = state_switch(&cmd_line_mec, "history_down");
		if (status < 0) {
			return status;
		}
		return 0;
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

int history_up_task(void *pch)
{
	int status;
	if (history.index < history.count) {
		history.index++;
		cmd_line_replace(history.buf[history.index - 1],
				 strlen(history.buf[history.index - 1]));
	}
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(history_up, NULL, history_up_task, NULL, ".cli_cmd_line");

int history_down_task(void *pch)
{
	int status;
	if (history.index > 1) {
		history.index--;
		cmd_line_replace(history.buf[history.index - 1],
				 strlen(history.buf[history.index - 1]));
	} else if (history.index == 1) {
		history.index = 0;
		cmd_line_replace("", 0);
	}
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(history_down, NULL, history_down_task, NULL,
		     ".cli_cmd_line");

int tab_complete_task(void *pch)
{
	int status;
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	int prefix_len = cmd_line.size - tok_start;
	const char *prefix = &cmd_line.buf[tok_start];

	int cmd_start = 0;
	while (cmd_start < cmd_line.size && cmd_line.buf[cmd_start] == ' ')
		cmd_start++;
	int first_word_end = cmd_start;
	while (first_word_end < cmd_line.size &&
	       cmd_line.buf[first_word_end] != ' ')
		first_word_end++;

	if (cmd_line.size == 0 ||
	    (tok_start >= cmd_start && tok_start < first_word_end)) {
		complete_command_name(prefix, prefix_len);
	} else {
		char cmd_name[CMD_LINE_BUF_SIZE];
		memcpy(cmd_name, cmd_line.buf + cmd_start,
		       first_word_end - cmd_start);
		cmd_name[first_word_end - cmd_start] = '\0';

		const cli_command_t *cmd = find_cmd_by_name(cmd_name);
		if (!cmd) {
			cli_out_push((_u8 *)"\a", 1);
			cli_out_sync();
		} else {
			complete_option(cmd, prefix, prefix_len);
		}
	}

	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0)
		return status;
	return 0;
}
_EXPORT_STATE_SYMBOL(tab_complete, NULL, tab_complete_task, NULL,
		     ".cli_cmd_line");

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

int clear_handler(void *arg)
{
	int status;
	status = cli_out_push((_u8 *)"\x1b[H\x1b[2J", sizeof("\x1b[H\x1b[2J"));
	if (status < 0) {
		return -1;
	}
	void cli_prompt_print(void);
	cli_prompt_print();
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(clear, NULL, clear_handler, NULL, ".cli_cmd_line");

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
	if (cmd_line.size > 0) {
		history_save(cmd_line.buf, cmd_line.size);
	}
	memset(cmd_line.buf, 0, CMD_LINE_BUF_SIZE);
	cmd_line.size = 0;
	cmd_line.pos = 0;
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
