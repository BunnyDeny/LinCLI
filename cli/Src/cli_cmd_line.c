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

#include "cli_cmd_line.h"
#include "cli_errno.h"
#include "cli_io.h"
#include "cli_mpool.h"
#include "cmd_dispose.h"
#include "init_d.h"
#include "stateM.h"
#include <string.h>

uint8_t rows_to_clear_count = 1;

extern struct tState *const _cli_cmd_line_start[];
extern struct tState *const _cli_cmd_line_end[];

static bool is_valid_char(char c);
void cli_prompt_print(void);

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

static void clear_and_up(int clears, int ups)
{
	for (int i = 0; i < clears; i++) {
		cli_out_push((_u8 *)"\a\r\n", 3);
		cli_out_push((_u8 *)"\033[2K", 4); //清除当前行的所有内容
		cli_out_sync();
	}
	for (int i = 0; i < ups; i++) {
		cli_out_push((_u8 *)"\033[1A", 4); //返回到上一行
		cli_out_sync();
	}
	rows_to_clear_count = 1;
}

static void cmd_line_replace(const char *new_buf, int new_size)
{
	int status;

	status = cli_out_push((_u8 *)"\r\033[K", 4);
	if (status < 0)
		return;
	if (cli_out_sync())
		return;
	cli_printk("\033[K");
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
	cli_printk("\033[K");
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
	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (cmd->name && strcmp(cmd->name, name) == 0)
			return cmd;
	}
	return NULL;
}

static int find_cmd_match(const char *prefix, int prefix_len,
			  const cli_command_t **first_match)
{
	int match_cnt = 0;
	const cli_command_t *cmd;

	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, prefix, prefix_len) == 0) {
			match_cnt++;
			if (match_cnt == 1)
				*first_match = cmd;
		}
	}
	return match_cnt;
}

static void replace_cmdline_token(const char *replacement, int repl_len,
				  int append_space)
{
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	int new_size = tok_start + repl_len;

	if (append_space && new_size < CMD_LINE_BUF_SIZE - 1)
		new_size++;
	if (new_size > CMD_LINE_BUF_SIZE)
		new_size = CMD_LINE_BUF_SIZE;

	char *new_buf = cli_mpool_alloc();
	if (!new_buf) {
		pr_err("out of memory\r\n");
		return;
	}
	memcpy(new_buf, cmd_line.buf, tok_start);
	memcpy(new_buf + tok_start, replacement, repl_len);
	if (append_space && tok_start + repl_len < CMD_LINE_BUF_SIZE - 1)
		new_buf[tok_start + repl_len] = ' ';
	cmd_line_replace(new_buf, new_size);
	cli_mpool_free(new_buf);
}

static void complete_unique_cmd(const cli_command_t *match)
{
	clear_and_up(rows_to_clear_count, rows_to_clear_count);
	replace_cmdline_token(match->name, (int)strlen(match->name), 1);
}

static int compute_cmd_lcp(char *lcp_buf, int lcp_buf_size,
			   const cli_command_t *first_match, const char *prefix,
			   int prefix_len)
{
	int lcp_len = (int)strlen(first_match->name);
	if (lcp_len > lcp_buf_size)
		lcp_len = lcp_buf_size;
	memcpy(lcp_buf, first_match->name, lcp_len);

	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, prefix, prefix_len) != 0)
			continue;
		int cpl = str_common_prefix_len(lcp_buf, cmd->name);
		if (cpl < lcp_len)
			lcp_len = cpl;
	}
	return lcp_len;
}

static void display_candidates(const char *prefix, int prefix_len,
			       int display_max_cows)
{
	const cli_command_t *cmd;
	int max_len = 0, cnt = 0;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, prefix, prefix_len) == 0) {
			max_len = max_len > strlen(cmd->name) ?
					  max_len :
					  strlen(cmd->name);
			cnt++;
		}
	}
	max_len += 3;
	int cows = display_max_cows / max_len, cur_cow = 0;
	rows_to_clear_count = (cnt + cows - 1) / cows;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, prefix, prefix_len) == 0) {
			cli_out_push((_u8 *)cmd->name, strlen(cmd->name));
			int space_count = max_len - strlen(cmd->name);
			while (space_count--) {
				cli_out_push((_u8 *)" ", 1);
			}
			cur_cow++;
			if (cur_cow > cows) {
				cli_out_push((_u8 *)"\r\n", 2);
				cur_cow = 0;
			}
			cli_out_sync();
		}
	}
}

static void list_cmd_candidates(const char *prefix, int prefix_len)
{
	clear_and_up(rows_to_clear_count, rows_to_clear_count - 1);
	display_candidates(prefix, prefix_len, DISPLAY_MAX_COWS);
	for (int i = 0; i < rows_to_clear_count; i++) {
		cli_out_push((_u8 *)"\033[1A", 4); //返回到上一行
		cli_out_sync();
	}
	cmd_line_redraw();
}

static void complete_multi_cmd(const cli_command_t *first_match,
			       const char *prefix, int prefix_len,
			       char *lcp_buf)
{
	int lcp_len = compute_cmd_lcp(lcp_buf, CMD_LINE_BUF_SIZE, first_match,
				      prefix, prefix_len);
	if (lcp_len > prefix_len) {
		replace_cmdline_token(lcp_buf, lcp_len, 0);
	} else {
		list_cmd_candidates(prefix, prefix_len);
	}
}

static void complete_command_name(const char *prefix, int prefix_len)
{
	const cli_command_t *match = NULL;
	int match_cnt = find_cmd_match(prefix, prefix_len, &match);

	if (match_cnt == 1) {
		complete_unique_cmd(match);
	} else if (match_cnt > 1) {
		char *lcp = cli_mpool_alloc();
		if (!lcp) {
			pr_err("out of memory\r\n");
			return;
		}
		complete_multi_cmd(match, prefix, prefix_len, lcp);
		cli_mpool_free(lcp);
	} else {
		clear_and_up(rows_to_clear_count, rows_to_clear_count);
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
		cmd_line_redraw();
	}
}

static void list_all_options(const cli_command_t *cmd)
{
	clear_and_up(rows_to_clear_count, rows_to_clear_count - 1);
	int cows = 0;
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
			cli_out_push((_u8 *)"  \r\n", 4);
			cows++;
		}
		cli_out_sync();
	}
	rows_to_clear_count = cows + 1;
	for (int i = 0; i < rows_to_clear_count; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
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
	cli_out_push((_u8 *)"\a", 1);
	cli_out_sync();
}

static void list_long_option_candidates(const cli_command_t *cmd,
					const char *name_prefix,
					int name_prefix_len)
{
	clear_and_up(rows_to_clear_count, rows_to_clear_count - 1);
	int cows = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, name_prefix, name_prefix_len) == 0) {
			cli_out_push((_u8 *)"--", 2);
			cli_out_push((_u8 *)opt->long_opt,
				     strlen(opt->long_opt));
			cli_out_push((_u8 *)"  \r\n", 4);
			cows++;
		}
	}
	rows_to_clear_count = cows + 1;
	cli_out_sync();
	for (int i = 0; i < rows_to_clear_count; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}

	cmd_line_redraw();
}

static void replace_long_option_only(const char *long_opt, int long_len)
{
	char *new_buf = cli_mpool_alloc();
	if (!new_buf) {
		pr_err("out of memory\r\n");
		return;
	}
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
	cli_mpool_free(new_buf);
}

static void replace_long_option(const char *long_opt, int long_len)
{
	char *new_buf = cli_mpool_alloc();
	if (!new_buf) {
		pr_err("out of memory\r\n");
		return;
	}
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	memcpy(new_buf, cmd_line.buf, tok_start);
	new_buf[tok_start] = '-';
	new_buf[tok_start + 1] = '-';
	memcpy(new_buf + tok_start + 2, long_opt, long_len);
	int new_size = tok_start + 2 + long_len;
	cmd_line_replace(new_buf, new_size);
	cli_mpool_free(new_buf);
}

static void replace_short_option(char c)
{
	char *new_buf = cli_mpool_alloc();
	if (!new_buf) {
		pr_err("out of memory\r\n");
		return;
	}
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	memcpy(new_buf, cmd_line.buf, tok_start);
	new_buf[tok_start] = '-';
	new_buf[tok_start + 1] = c;
	int new_size = tok_start + 2;
	if (new_size < CMD_LINE_BUF_SIZE - 1) {
		new_buf[new_size] = ' ';
		new_size++;
	}
	cmd_line_replace(new_buf, new_size);
	cli_mpool_free(new_buf);
}

static bool is_last_full_token_the_only_option(const cli_command_t *cmd)
{
	if (cmd->option_count != 1)
		return false;

	int end = cmd_line.size - 1;
	while (end >= 0 && cmd_line.buf[end] == ' ')
		end--;
	if (end < 0)
		return false;

	int start = end;
	while (start >= 0 && cmd_line.buf[start] != ' ')
		start--;
	start++;

	int len = end - start + 1;
	const cli_option_t *opt = &cmd->options[0];

	if (opt->long_opt) {
		int llen = (int)strlen(opt->long_opt);
		if (len == llen + 2 && cmd_line.buf[start] == '-' &&
		    cmd_line.buf[start + 1] == '-' &&
		    strncmp(&cmd_line.buf[start + 2], opt->long_opt, llen) == 0)
			return true;
	}
	if (opt->short_opt) {
		if (len == 2 && cmd_line.buf[start] == '-' &&
		    cmd_line.buf[start + 1] == opt->short_opt)
			return true;
	}
	return false;
}

static void complete_long_option(const cli_command_t *cmd,
				 const char *name_prefix, int name_prefix_len)
{
	const cli_option_t *match = NULL;
	int match_cnt = 0;
	char *lcp = NULL;
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
		replace_long_option_only(match->long_opt,
					 (int)strlen(match->long_opt));
	} else if (match_cnt > 1) {
		int lcp_len = (int)strlen(match->long_opt);
		lcp = cli_mpool_alloc();
		if (!lcp) {
			pr_err("out of memory\r\n");
			goto out;
		}
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
out:
	if (lcp)
		cli_mpool_free(lcp);
}

static void complete_option(const cli_command_t *cmd, const char *prefix,
			    int prefix_len)
{
	if (prefix_len == 0) {
		if (cmd->option_count == 1) {
			const cli_option_t *opt = &cmd->options[0];
			if (is_last_full_token_the_only_option(cmd)) {
				cli_out_push((_u8 *)"\a", 1);
				cli_out_sync();
			} else if (opt->long_opt) {
				replace_long_option_only(
					opt->long_opt,
					(int)strlen(opt->long_opt));
			} else if (opt->short_opt) {
				replace_short_option(opt->short_opt);
			} else {
				cli_out_push((_u8 *)"\a", 1);
				cli_out_sync();
			}
		} else if (cmd->option_count > 0) {
			list_all_options(cmd);
		} else {
			cli_out_push((_u8 *)"\a", 1);
			cli_out_sync();
		}
		return;
	}
	if (prefix_len == 1 && prefix[0] == '-') {
		if (cmd->option_count == 1) {
			const cli_option_t *opt = &cmd->options[0];
			if (opt->long_opt) {
				replace_long_option(opt->long_opt,
						    (int)strlen(opt->long_opt));
			} else if (opt->short_opt) {
				replace_short_option(opt->short_opt);
			} else {
				cli_out_push((_u8 *)"\a", 1);
				cli_out_sync();
			}
		} else {
			list_all_options(cmd);
		}
		return;
	}
	if (prefix_len == 2 && prefix[0] == '-' && prefix[1] == '-') {
		complete_long_option(cmd, prefix + 2, 0);
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

static int cmd_line_start_task(void *pch)
{
	int status;
	char ch = *((char *)pch);
	if (is_valid_char(ch)) {
		status = state_switch(&cmd_line_mec, "valid_char");
		if (status < 0) {
			return status;
		}
	} else {
		status = state_switch(&cmd_line_mec, "invalid_char");
		if (status < 0) {
			return status;
		}
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(cmd_line_start, NULL, cmd_line_start_task, NULL,
		     ".cli_cmd_line");

static int valid_char_task(void *pch)
{
	int status;
	char ch = *((char *)pch);
	if (cmd_line.size == CMD_LINE_BUF_SIZE) {
		pr_warn("command length exceeds the limit. \r\n");
		goto label_cmd_line_exit;
	}
	if (cmd_line.pos == cmd_line.size) {
		status = cli_out_push((_u8 *)pch, 1);
		if (status < 0) {
			return status;
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
			return status;
		}
		if (cli_out_sync()) {
			return CLI_ERR_IO_SYNC;
		}
		int pos_move_cnt = cmd_line.size - cmd_line.pos;
		while (pos_move_cnt--) {
			status = cli_out_push((_u8 *)"\033[D", 4);
			if (status < 0) {
				return status;
			}
			if (cli_out_sync()) {
				return CLI_ERR_IO_SYNC;
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
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(valid_char, NULL, valid_char_task, NULL, ".cli_cmd_line");

static int invalid_char_task(void *pch)
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
	case '\r':
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
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(invalid_char, NULL, invalid_char_task, NULL,
		     ".cli_cmd_line");

static int ESC_handler(void *pch)
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
			return status;
		}
		cmd_line.pos--;
	} else if (esc_params[1] == 'C' && cmd_line.pos < cmd_line.size) {
		char *left_move = "\x1b[C";
		status = cli_out_push((_u8 *)left_move, strlen(left_move) + 1);
		if (status < 0) {
			return status;
		}
		cmd_line.pos++;
	} else if (esc_params[1] == 'A') {
		status = state_switch(&cmd_line_mec, "history_up");
		if (status < 0) {
			return status;
		}
		return CLI_OK;
	} else if (esc_params[1] == 'B') {
		status = state_switch(&cmd_line_mec, "history_down");
		if (status < 0) {
			return status;
		}
		return CLI_OK;
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
			return CLI_OK;
		}
	}
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(ESC_handler, NULL, ESC_handler, NULL, ".cli_cmd_line");

static int history_up_task(void *pch)
{
	int status;
	clear_and_up(rows_to_clear_count, rows_to_clear_count);
	cmd_line_redraw();
	if (history.index < history.count) {
		history.index++;
		cmd_line_replace(history.buf[history.index - 1],
				 strlen(history.buf[history.index - 1]));
	}
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(history_up, NULL, history_up_task, NULL, ".cli_cmd_line");

static int history_down_task(void *pch)
{
	int status;
	clear_and_up(rows_to_clear_count, rows_to_clear_count);
	cmd_line_redraw();
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
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(history_down, NULL, history_down_task, NULL,
		     ".cli_cmd_line");

static int get_current_segment_start(const char *buf, int size)
{
	int start = 0;
	for (int i = 0; i < size - 1; i++) {
		if (buf[i] == '&' && buf[i + 1] == '&') {
			start = i + 2;
			while (start < size && buf[start] == ' ')
				start++;
		}
	}
	return start;
}

static int tab_complete_task(void *pch)
{
	int status;
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	int prefix_len = cmd_line.size - tok_start;
	const char *prefix = &cmd_line.buf[tok_start];

	int cmd_start = get_current_segment_start(cmd_line.buf, cmd_line.size);
	while (cmd_start < cmd_line.size && cmd_line.buf[cmd_start] == ' ')
		cmd_start++;
	int first_word_end = cmd_start;
	while (first_word_end < cmd_line.size &&
	       cmd_line.buf[first_word_end] != ' ')
		first_word_end++;

	if (cmd_line.size == 0 ||
	    (tok_start >= cmd_start && tok_start < first_word_end) ||
	    cmd_start >= cmd_line.size) {
		complete_command_name(prefix, prefix_len);
	} else {
		char *cmd_name = cli_mpool_alloc();
		if (cmd_name == NULL) {
			pr_err("out of memory\r\n");
			return CLI_ERR_NULL;
		}
		memcpy(cmd_name, cmd_line.buf + cmd_start,
		       first_word_end - cmd_start);
		cmd_name[first_word_end - cmd_start] = '\0';

		const cli_command_t *cmd = find_cmd_by_name(cmd_name);
		cli_mpool_free(cmd_name);
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
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(tab_complete, NULL, tab_complete_task, NULL,
		     ".cli_cmd_line");

static int delete(void *pch)
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
			return status;
		}
		if (cli_out_sync()) {
			return CLI_ERR_IO_SYNC;
		}
		int pos_move_cnt = cmd_line.size - cmd_line.pos;
		while (pos_move_cnt--) {
			status = cli_out_push((_u8 *)"\033[D", 4);
			if (status < 0) {
				return status;
			}
			if (cli_out_sync()) {
				return CLI_ERR_IO_SYNC;
			}
		}
		cmd_line.size--;
	}
delete_exit:
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(delete, NULL, delete, NULL, ".cli_cmd_line");

static int backspace_handler(void *pch)
{
	int status;
	if (cmd_line.pos != 0 && cmd_line.pos == cmd_line.size) {
		status = cli_out_push((_u8 *)"\b \b", 4);
		if (status < 0) {
			return status;
		}
		goto label_size_pos_dis;
	} else if (cmd_line.pos == 0) {
		goto label_exit2;
	} else if (cmd_line.pos < cmd_line.size) {
		status = cli_out_push((_u8 *)"\b \b", 4);
		if (status < 0) {
			return status;
		}
		for (int i = cmd_line.pos - 1; i < cmd_line.size - 1; i++) {
			cmd_line.buf[i] = cmd_line.buf[i + 1];
		}
		cmd_line.buf[cmd_line.size - 1] = ' ';
		status = cli_out_push((_u8 *)&cmd_line.buf[cmd_line.pos - 1],
				      cmd_line.size - cmd_line.pos + 1);
		if (status < 0) {
			return status;
		}
		if (cli_out_sync()) {
			return CLI_ERR_IO_SYNC;
		}
		int pos_move_cnt = cmd_line.size - cmd_line.pos + 1;
		while (pos_move_cnt--) {
			status = cli_out_push((_u8 *)"\033[D", 4);
			if (status < 0) {
				return status;
			}
			if (cli_out_sync()) {
				return CLI_ERR_IO_SYNC;
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
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(backspace_handler, NULL, backspace_handler, NULL,
		     ".cli_cmd_line");

static int clear_handler(void *arg)
{
	int status;
	status = cli_out_push((_u8 *)"\x1b[H\x1b[2J", sizeof("\x1b[H\x1b[2J"));
	if (status < 0) {
		return status;
	}
	cli_printk("\033[K");
	cli_prompt_print();
	status = state_switch(&cmd_line_mec, "exit_handler");
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(clear, NULL, clear_handler, NULL, ".cli_cmd_line");

static void enter_entry(void *pch)
{
	memset(origin_cmd.buf, 0, CMD_LINE_BUF_SIZE);
}
static int enter_press(void *pch)
{
	// int status;
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

static int cmd_line_exit_handler(void *pch)
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

int cli_cmd_line_init(void)
{
	int status = engine_init(&cmd_line_mec, "cmd_line_start",
				 _cli_cmd_line_start, _cli_cmd_line_end);
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}

int cli_cmd_line_task(char ch)
{
	int status = 0;
	while (status != cmd_line_exit) {
		status = stateEngineRun(&cmd_line_mec, &ch);
		if (status < 0) {
			pr_err("cli_cmd_line状态机异常，错误码: %d\r\n",
			       status);
			return status;
		}
		if (status == cmd_line_enter_press) {
			clear_and_up(rows_to_clear_count, rows_to_clear_count);
			return status;
		}
	}
	return CLI_OK;
}
