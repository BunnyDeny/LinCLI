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
#include <stdarg.h>
#include <string.h>

extern struct tState *const _cli_cmd_line_start[];
extern struct tState *const _cli_cmd_line_end[];

static bool is_valid_char(char c);
void cli_prompt_print(void);
static int get_current_segment_start(const char *buf, int size);

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
}

/* ============================================================
 *  候选列表状态管理（供 cli_printk 重绘使用）
 * ============================================================ */

struct candidate_ctx {
	int active; /* 0=none, 1=cmd, 2=all_opts, 3=long_opts */
	char prefix[CMD_LINE_BUF_SIZE];
	int prefix_len;
	const cli_command_t *cmd;
	int highlight_index;
	int cycling; /* 0=none, 1=cmd cycling, 2=opt cycling */
	int rows;
	int cols;
	int repl_start; /* 选项高亮替换的起始位置 */
};
static struct candidate_ctx candidate_ctx = { 0 };

static void candidate_ctx_save(int active, const char *prefix, int prefix_len,
			       const cli_command_t *cmd)
{
	if (candidate_ctx.active != active)
		candidate_ctx.repl_start = -1;
	candidate_ctx.active = active;
	if (prefix && prefix_len > 0) {
		int n = prefix_len < CMD_LINE_BUF_SIZE ? prefix_len :
							 CMD_LINE_BUF_SIZE - 1;
		memcpy(candidate_ctx.prefix, prefix, n);
		candidate_ctx.prefix[n] = '\0';
		candidate_ctx.prefix_len = n;
	} else {
		candidate_ctx.prefix[0] = '\0';
		candidate_ctx.prefix_len = 0;
	}
	candidate_ctx.cmd = cmd;
	candidate_ctx.highlight_index = 0;
	candidate_ctx.cycling = 0;
	candidate_ctx.rows = 0;
	candidate_ctx.cols = 0;
}

static void candidate_ctx_clear(void)
{
	candidate_ctx.active = 0;
	candidate_ctx.cycling = 0;
	candidate_ctx.rows = 0;
	candidate_ctx.cols = 0;
	candidate_ctx.repl_start = -1;
}

static void cmd_line_replace(const char *new_buf, int new_size)
{
	if (candidate_ctx.rows > 1)
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
	candidate_ctx_clear();
	int status;

	status = cli_out_push((_u8 *)"\r\033[K", 4);
	if (status < 0)
		return;
	if (cli_out_sync())
		return;
	all_printk("\033[K");
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
	all_printk("\033[K");
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

/* ============================================================
 *  cli_printk 辅助函数（从 cli_io.c 迁移而来）
 * ============================================================ */

static char buffer[CLI_PRINTK_BUF_SIZE];

extern const char *pre_EMERG_gen(void);
extern const char *pre_ALERT_gen(void);
extern const char *pre_CRIT_gen(void);
extern const char *pre_ERR_gen(void);
extern const char *pre_WARNING_gen(void);
extern const char *pre_NOTICE_gen(void);
extern const char *pre_INFO_gen(void);
extern const char *pre_DEBUG_gen(void);
extern const char *pre_DEFAULT_gen(void);
extern const char *pre_CONT_gen(void);

static const char *prefix_gen(const char *level)
{
	char lv = level[0];
	const char *prefix;
	switch (lv) {
	case '0':
		prefix = pre_EMERG_gen();
		break;
	case '1':
		prefix = pre_ALERT_gen();
		break;
	case '2':
		prefix = pre_CRIT_gen();
		break;
	case '3':
		prefix = pre_ERR_gen();
		break;
	case '4':
		prefix = pre_WARNING_gen();
		break;
	case '5':
		prefix = pre_NOTICE_gen();
		break;
	case '6':
		prefix = pre_INFO_gen();
		break;
	case '7':
		prefix = pre_DEBUG_gen();
		break;
	case 'c':
		prefix = pre_CONT_gen();
		break;
	default:
		prefix = pre_DEFAULT_gen();
		break;
	}
	return prefix;
}

static inline int is_kern_level(char c)
{
	return (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' ||
		c == '5' || c == '6' || c == '7' || c == 'c');
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
	clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
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

static void compute_candidate_layout(const char *prefix, int prefix_len,
				     int display_max_cows, int *max_len,
				     int *cnt)
{
	const cli_command_t *cmd;
	*max_len = 0;
	*cnt = 0;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, prefix, prefix_len) == 0) {
			*max_len = *max_len > strlen(cmd->name) ?
					   *max_len :
					   strlen(cmd->name);
			(*cnt)++;
		}
	}
	*max_len += 3;
	int cows = display_max_cows / *max_len;
	candidate_ctx.rows = (*cnt + cows - 1) / cows;
	candidate_ctx.cols = cows;
}

static void display_candidates(const char *prefix, int prefix_len,
			       int display_max_cows, int highlight_idx)
{
	const cli_command_t *cmd;
	int max_len, cnt;
	compute_candidate_layout(prefix, prefix_len, display_max_cows,
				 &max_len, &cnt);

	int cur_cow = 0, cur_idx = 0;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, prefix, prefix_len) == 0) {
			if (cur_cow == 0)
				cli_out_push((_u8 *)"\r\n", 2);
			if (cur_idx == highlight_idx) {
				cli_out_push((_u8 *)"\033[7m", 4);
				cli_out_push((_u8 *)cmd->name, strlen(cmd->name));
				cli_out_push((_u8 *)"\033[0m", 4);
			} else {
				cli_out_push((_u8 *)cmd->name, strlen(cmd->name));
			}
			int space_count = max_len - strlen(cmd->name);
			while (space_count--)
				cli_out_push((_u8 *)" ", 1);
			cur_cow++;
			if (cur_cow >= candidate_ctx.cols)
				cur_cow = 0;
			cli_out_sync();
			cur_idx++;
		}
	}
}

static void list_cmd_candidates(const char *prefix, int prefix_len)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(1, prefix, prefix_len, NULL);
	display_candidates(prefix, prefix_len, DISPLAY_MAX_COWS, -1);
	for (int i = 0; i < candidate_ctx.rows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4); //返回到上一行
		cli_out_sync();
	}
	cmd_line_redraw();
}


static void normalize_highlight_index(int total)
{
	while (candidate_ctx.highlight_index < 0)
		candidate_ctx.highlight_index = total + candidate_ctx.highlight_index;
	while (candidate_ctx.highlight_index >= total)
		candidate_ctx.highlight_index = candidate_ctx.highlight_index % total;
}

static void candidate_list_redraw(int rows)
{
	for (int i = 0; i < rows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
}

static void replace_token_at(int seg_start, const char *token, int tok_len,
			     int append_space)
{
	int new_size = seg_start + tok_len;
	if (append_space && new_size < CMD_LINE_BUF_SIZE - 1)
		new_size++;
	if (new_size > CMD_LINE_BUF_SIZE)
		new_size = CMD_LINE_BUF_SIZE;

	char *new_buf = cli_mpool_alloc();
	if (!new_buf) {
		pr_err("out of memory\r\n");
		return;
	}
	memcpy(new_buf, cmd_line.buf, seg_start);
	memcpy(new_buf + seg_start, token, tok_len);
	if (append_space && seg_start + tok_len < CMD_LINE_BUF_SIZE - 1)
		new_buf[seg_start + tok_len] = ' ';

	memset(cmd_line.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(cmd_line.buf, new_buf, new_size);
	cmd_line.size = new_size;
	cmd_line.pos = new_size;
	cli_mpool_free(new_buf);
}

static int cmd_match_total(void)
{
	int total = 0;
	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, candidate_ctx.prefix,
			    candidate_ctx.prefix_len) == 0)
			total++;
	}
	return total;
}

static const cli_command_t *cmd_find_match_by_index(int idx)
{
	int cur = 0;
	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, candidate_ctx.prefix,
			    candidate_ctx.prefix_len) == 0) {
			if (cur == idx)
				return cmd;
			cur++;
		}
	}
	return NULL;
}

static int long_opt_match_total(const cli_command_t *cmd)
{
	int total = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, candidate_ctx.prefix,
			    candidate_ctx.prefix_len) == 0)
			total++;
	}
	return total;
}

static const cli_option_t *long_opt_find_match_by_index(const cli_command_t *cmd,
							int idx)
{
	int cur = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, candidate_ctx.prefix,
			    candidate_ctx.prefix_len) == 0) {
			if (cur == idx)
				return opt;
			cur++;
		}
	}
	return NULL;
}

static int build_option_token(const cli_option_t *opt, char *buf)
{
	int pos = 0;
	if (opt->long_opt && candidate_ctx.prefix_len >= 2 &&
	    candidate_ctx.prefix[0] == '-' &&
	    candidate_ctx.prefix[1] == '-') {
		buf[pos++] = '-';
		buf[pos++] = '-';
		int len = (int)strlen(opt->long_opt);
		memcpy(buf + pos, opt->long_opt, len);
		pos += len;
	} else if (opt->short_opt) {
		buf[pos++] = '-';
		buf[pos++] = opt->short_opt;
	} else if (opt->long_opt) {
		buf[pos++] = '-';
		buf[pos++] = '-';
		int len = (int)strlen(opt->long_opt);
		memcpy(buf + pos, opt->long_opt, len);
		pos += len;
	}
	return pos;
}

static void replace_long_opt_at(int tok_start, const char *long_opt,
				int long_len)
{
	char *new_buf = cli_mpool_alloc();
	if (!new_buf) {
		pr_err("out of memory\r\n");
		return;
	}
	memcpy(new_buf, cmd_line.buf, tok_start);
	new_buf[tok_start] = '-';
	new_buf[tok_start + 1] = '-';
	memcpy(new_buf + tok_start + 2, long_opt, long_len);
	int new_size = tok_start + 2 + long_len;
	if (new_size < CMD_LINE_BUF_SIZE - 1)
		new_buf[new_size++] = ' ';

	memset(cmd_line.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(cmd_line.buf, new_buf, new_size);
	cmd_line.size = new_size;
	cmd_line.pos = new_size;
	cli_mpool_free(new_buf);
}

static void cycle_cmd_candidate_highlight(void)
{
	clear_and_up(candidate_ctx.rows, candidate_ctx.rows);

	int total = cmd_match_total();
	normalize_highlight_index(total);

	const cli_command_t *target =
		cmd_find_match_by_index(candidate_ctx.highlight_index);
	if (!target)
		return;

	int tok_start = get_current_segment_start(cmd_line.buf, cmd_line.size);
	replace_token_at(tok_start, target->name, (int)strlen(target->name), 1);

	display_candidates(candidate_ctx.prefix, candidate_ctx.prefix_len,
			   DISPLAY_MAX_COWS, candidate_ctx.highlight_index);
	candidate_list_redraw(candidate_ctx.rows);
	candidate_ctx.active = 1;
	candidate_ctx.cycling = 1;
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
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
		char *lcp = cli_mpool_alloc();
		if (!lcp) {
			pr_err("out of memory\r\n");
			return;
		}
		complete_multi_cmd(match, prefix, prefix_len, lcp);
		cli_mpool_free(lcp);
	} else {
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
		candidate_ctx_clear();
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
		cmd_line_redraw();
	}
}

static void list_all_options(const cli_command_t *cmd, const char *prefix,
			     int prefix_len, int highlight_idx)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(2, prefix, prefix_len, cmd);
	int cows = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		cli_out_push((_u8 *)"\r\n", 2);
		cows++;
		if ((int)i == highlight_idx) {
			cli_out_push((_u8 *)"\033[7m", 4);
		}
		if (opt->short_opt) {
			char buf[4] = { '-', opt->short_opt, ' ', '\0' };
			cli_out_push((_u8 *)buf, 3);
		}
		if (opt->long_opt) {
			cli_out_push((_u8 *)"--", 2);
			cli_out_push((_u8 *)opt->long_opt,
				     strlen(opt->long_opt));
		}
		if ((int)i == highlight_idx) {
			cli_out_push((_u8 *)"\033[0m", 4);
		}
		cli_out_sync();
	}
	candidate_ctx.rows = cows;
	candidate_ctx.cols = 1;
	for (int i = 0; i < candidate_ctx.rows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
}

static void do_complete_short_option(char c, const cli_command_t *cmd)
{
	candidate_ctx_clear();
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
					int name_prefix_len,
					int highlight_idx)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows - 1);
	candidate_ctx_save(3, name_prefix, name_prefix_len, cmd);
	int cows = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, name_prefix, name_prefix_len) == 0) {
			cli_out_push((_u8 *)"--", 2);
			if ((int)cows == highlight_idx) {
				cli_out_push((_u8 *)"\033[7m", 4);
			}
			cli_out_push((_u8 *)opt->long_opt,
				     strlen(opt->long_opt));
			if ((int)cows == highlight_idx) {
				cli_out_push((_u8 *)"\033[0m", 4);
			}
			cli_out_push((_u8 *)"\r\n", 2);
			cows++;
		}
	}
	candidate_ctx.rows = cows + 1;
	candidate_ctx.cols = 1;
	cli_out_sync();
	for (int i = 0; i < candidate_ctx.rows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}

	cmd_line_redraw();
}

static void cycle_all_option_highlight(void)
{
	const cli_command_t *cmd = candidate_ctx.cmd;
	if (!cmd)
		return;

	normalize_highlight_index((int)cmd->option_count);
	const cli_option_t *target = &cmd->options[candidate_ctx.highlight_index];

	char *new_buf = cli_mpool_alloc();
	if (!new_buf) {
		pr_err("out of memory\r\n");
		return;
	}

	int tok_start = candidate_ctx.repl_start;
	if (tok_start < 0 || tok_start > cmd_line.size)
		tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);

	memcpy(new_buf, cmd_line.buf, tok_start);
	int repl_len = build_option_token(target, new_buf + tok_start);
	int new_size = tok_start + repl_len;
	if (new_size < CMD_LINE_BUF_SIZE - 1)
		new_buf[new_size++] = ' ';

	memset(cmd_line.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(cmd_line.buf, new_buf, new_size);
	cmd_line.size = new_size;
	cmd_line.pos = new_size;
	cli_mpool_free(new_buf);

	int saved_repl_start = candidate_ctx.repl_start;
	int saved_highlight = candidate_ctx.highlight_index;
	list_all_options(cmd, candidate_ctx.prefix, candidate_ctx.prefix_len,
			 candidate_ctx.highlight_index);
	candidate_ctx.active = 2;
	candidate_ctx.cycling = 2;
	candidate_ctx.repl_start = saved_repl_start;
	candidate_ctx.highlight_index = saved_highlight;
}

static void cycle_long_option_highlight(void)
{
	const cli_command_t *cmd = candidate_ctx.cmd;
	if (!cmd)
		return;

	int total = long_opt_match_total(cmd);
	if (total == 0)
		return;

	normalize_highlight_index(total);

	const cli_option_t *target =
		long_opt_find_match_by_index(cmd, candidate_ctx.highlight_index);
	if (!target)
		return;

	int tok_start = candidate_ctx.repl_start;
	if (tok_start < 0 || tok_start > cmd_line.size)
		tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	replace_long_opt_at(tok_start, target->long_opt,
			    (int)strlen(target->long_opt));

	int saved_repl_start = candidate_ctx.repl_start;
	int saved_highlight = candidate_ctx.highlight_index;
	list_long_option_candidates(cmd, candidate_ctx.prefix,
				    candidate_ctx.prefix_len,
				    candidate_ctx.highlight_index);
	candidate_ctx.active = 3;
	candidate_ctx.cycling = 2;
	candidate_ctx.repl_start = saved_repl_start;
	candidate_ctx.highlight_index = saved_highlight;
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

static int long_opt_compute_lcp(const cli_command_t *cmd,
				const char *name_prefix, int name_prefix_len,
				const cli_option_t *first_match)
{
	int lcp_len = (int)strlen(first_match->long_opt);
	char *lcp = cli_mpool_alloc();
	if (!lcp) {
		pr_err("out of memory\r\n");
		return -1;
	}
	memcpy(lcp, first_match->long_opt, lcp_len);

	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		if (!opt->long_opt ||
		    strncmp(opt->long_opt, name_prefix, name_prefix_len) != 0)
			continue;
		int cpl = str_common_prefix_len(lcp, opt->long_opt);
		if (cpl < lcp_len)
			lcp_len = cpl;
	}
	cli_mpool_free(lcp);
	return lcp_len;
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
		replace_long_option_only(match->long_opt,
					 (int)strlen(match->long_opt));
	} else if (match_cnt > 1) {
		int lcp_len = long_opt_compute_lcp(cmd, name_prefix,
						   name_prefix_len, match);
		if (lcp_len < 0)
			return;
		if (lcp_len > name_prefix_len) {
			replace_long_option(match->long_opt, lcp_len);
		} else {
			list_long_option_candidates(cmd, name_prefix,
						    name_prefix_len, -1);
			candidate_ctx.repl_start =
				get_last_token_start(cmd_line.buf,
					     cmd_line.size);
		}
	} else {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
	}
}

static void complete_option_empty_prefix(const cli_command_t *cmd)
{
	if (cmd->option_count == 1) {
		const cli_option_t *opt = &cmd->options[0];
		if (is_last_full_token_the_only_option(cmd)) {
			cli_out_push((_u8 *)"\a", 1);
			cli_out_sync();
		} else if (opt->long_opt) {
			replace_long_option_only(opt->long_opt,
						 (int)strlen(opt->long_opt));
		} else if (opt->short_opt) {
			replace_short_option(opt->short_opt);
		} else {
			cli_out_push((_u8 *)"\a", 1);
			cli_out_sync();
		}
	} else if (cmd->option_count > 0) {
		list_all_options(cmd, "", 0, -1);
		candidate_ctx.repl_start =
			get_last_token_start(cmd_line.buf, cmd_line.size);
	} else {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
	}
}

static void complete_option_dash_prefix(const cli_command_t *cmd,
					const char *prefix, int prefix_len)
{
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
		list_all_options(cmd, prefix, prefix_len, -1);
		candidate_ctx.repl_start =
			get_last_token_start(cmd_line.buf, cmd_line.size);
	}
}

static void complete_option(const cli_command_t *cmd, const char *prefix,
			    int prefix_len)
{
	if (prefix_len == 0) {
		complete_option_empty_prefix(cmd);
		return;
	}
	if (prefix_len == 1 && prefix[0] == '-') {
		complete_option_dash_prefix(cmd, prefix, prefix_len);
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

/* ============================================================
 *  候选列表重绘辅助函数（cli_printk / clear_handler 共用）
 * ============================================================ */

static void candidate_redraw(void)
{
	if (candidate_ctx.active == 1) {
		if (candidate_ctx.cycling == 1)
			cycle_cmd_candidate_highlight();
		else
			list_cmd_candidates(candidate_ctx.prefix,
					    candidate_ctx.prefix_len);
	} else if (candidate_ctx.active == 2 && candidate_ctx.cmd) {
		if (candidate_ctx.cycling == 2)
			cycle_all_option_highlight();
		else
			list_all_options(candidate_ctx.cmd, candidate_ctx.prefix,
					 candidate_ctx.prefix_len, -1);
	} else if (candidate_ctx.active == 3 && candidate_ctx.cmd) {
		if (candidate_ctx.cycling == 2)
			cycle_long_option_highlight();
		else
			list_long_option_candidates(candidate_ctx.cmd,
						    candidate_ctx.prefix,
						    candidate_ctx.prefix_len,
						    -1);
	}
}

/* ============================================================
 *  状态机任务函数
 * ============================================================ */

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

static int valid_char_append(char ch)
{
	int status = cli_out_push((_u8 *)&ch, 1);
	if (status < 0)
		return status;
	cmd_line.buf[cmd_line.pos] = ch;
	cmd_line.size++;
	cmd_line.pos++;
	return CLI_OK;
}

static int valid_char_insert(char ch)
{
	int status;
	for (int i = cmd_line.size + 1; i >= cmd_line.pos + 1; i--) {
		cmd_line.buf[i] = cmd_line.buf[i - 1];
	}
	cmd_line.buf[cmd_line.pos] = ch;
	status = cli_out_push((_u8 *)&cmd_line.buf[cmd_line.pos],
			      cmd_line.size - cmd_line.pos + 1);
	if (status < 0)
		return status;
	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;
	int pos_move_cnt = cmd_line.size - cmd_line.pos;
	while (pos_move_cnt--) {
		status = cli_out_push((_u8 *)"\033[D", 4);
		if (status < 0)
			return status;
		if (cli_out_sync())
			return CLI_ERR_IO_SYNC;
	}
	cmd_line.size++;
	cmd_line.pos++;
	return CLI_OK;
}

static int valid_char_task(void *pch)
{
	char ch = *((char *)pch);
	if (candidate_ctx.active) {
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
		candidate_ctx_clear();
		cmd_line_redraw();
	}
	if (cmd_line.size == CMD_LINE_BUF_SIZE) {
		pr_warn("command length exceeds the limit. \r\n");
		return state_switch(&cmd_line_mec, "exit_handler");
	}
	int status;
	if (cmd_line.pos == cmd_line.size)
		status = valid_char_append(ch);
	else
		status = valid_char_insert(ch);
	if (status < 0)
		return status;
	return state_switch(&cmd_line_mec, "exit_handler");
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
		if (candidate_ctx.cycling) {
			next_state = "tab_cycle";
		} else if (candidate_ctx.active) {
			next_state = "tab_cycle_enter";
		} else {
			next_state = "tab_complete";
		}
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

/* ------------------------------------------------------------
 * ESC 序列解析与分发状态
 * ------------------------------------------------------------ */

static char *esc_resolve_horizontal(char seq)
{
	bool is_cycle = candidate_ctx.cycling;
	if (seq == 'D') { // left
		if (candidate_ctx.active == 1 && is_cycle)
			return "cmd_cycle_left";
		if ((candidate_ctx.active == 2 || candidate_ctx.active == 3) &&
		    is_cycle)
			return "opt_cycle_left";
		return "cursor_left";
	}
	// right
	if (candidate_ctx.active == 1 && is_cycle)
		return "cmd_cycle_right";
	if ((candidate_ctx.active == 2 || candidate_ctx.active == 3) && is_cycle)
		return "opt_cycle_right";
	return "cursor_right";
}

static char *esc_resolve_vertical(char seq)
{
	bool is_cycle = candidate_ctx.cycling;
	if (seq == 'A') { // up
		if (candidate_ctx.active == 1 && is_cycle)
			return "cmd_cycle_up";
		if ((candidate_ctx.active == 2 || candidate_ctx.active == 3) &&
		    is_cycle)
			return "opt_cycle_up";
		return "history_up";
	}
	// down
	if (candidate_ctx.active == 1 && is_cycle)
		return "cmd_cycle_down";
	if ((candidate_ctx.active == 2 || candidate_ctx.active == 3) && is_cycle)
		return "opt_cycle_down";
	return "history_down";
}

static int ESC_handler(void *pch)
{
	int status, esc_params_count = 2;
	char esc_params[2], ch;
	while (esc_params_count) {
		if (cli_get_in_size()) {
			status = cli_in_pop((_u8 *)&ch, 1);
			if (status < 0)
				return status;
			esc_params[2 - esc_params_count] = ch;
			esc_params_count--;
		}
	}

	char *next_state = "exit_handler";
	char seq = esc_params[1];
	if (seq == 'D' || seq == 'C')
		next_state = esc_resolve_horizontal(seq);
	else if (seq == 'A' || seq == 'B')
		next_state = esc_resolve_vertical(seq);
	else if (seq == '3') {
		status = cli_in_pop((_u8 *)&ch, 1);
		if (status < 0)
			return status;
		if (ch == '~')
			next_state = "delete";
	}

	status = state_switch(&cmd_line_mec, next_state);
	if (status < 0)
		return status;
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(ESC_handler, NULL, ESC_handler, NULL, ".cli_cmd_line");

/* ------------------------------------------------------------
 * 光标移动状态
 * ------------------------------------------------------------ */

static int cursor_left_task(void *pch)
{
	if (cmd_line.pos > 0) {
		cli_out_push((_u8 *)"\033[D", 4);
		cmd_line.pos--;
	}
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(cursor_left, NULL, cursor_left_task, NULL,
		     ".cli_cmd_line");

static int cursor_right_task(void *pch)
{
	if (cmd_line.pos < cmd_line.size) {
		cli_out_push((_u8 *)"\033[C", 4);
		cmd_line.pos++;
	}
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(cursor_right, NULL, cursor_right_task, NULL,
		     ".cli_cmd_line");

/* ------------------------------------------------------------
 * 命令候选列表导航状态
 * ------------------------------------------------------------ */

static int cmd_cycle_left_task(void *pch)
{
	candidate_ctx.highlight_index--;
	cycle_cmd_candidate_highlight();
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(cmd_cycle_left, NULL, cmd_cycle_left_task, NULL,
		     ".cli_cmd_line");

static int cmd_cycle_right_task(void *pch)
{
	candidate_ctx.highlight_index++;
	cycle_cmd_candidate_highlight();
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(cmd_cycle_right, NULL, cmd_cycle_right_task, NULL,
		     ".cli_cmd_line");

static int cmd_cycle_up_task(void *pch)
{
	candidate_ctx.highlight_index -= candidate_ctx.cols;
	cycle_cmd_candidate_highlight();
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(cmd_cycle_up, NULL, cmd_cycle_up_task, NULL,
		     ".cli_cmd_line");

static int cmd_cycle_down_task(void *pch)
{
	candidate_ctx.highlight_index += candidate_ctx.cols;
	cycle_cmd_candidate_highlight();
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(cmd_cycle_down, NULL, cmd_cycle_down_task, NULL,
		     ".cli_cmd_line");

/* ------------------------------------------------------------
 * 选项候选列表导航状态（统一处理 all_opts 和 long_opts）
 * ------------------------------------------------------------ */

static int opt_cycle_left_task(void *pch)
{
	candidate_ctx.highlight_index--;
	if (candidate_ctx.active == 2)
		cycle_all_option_highlight();
	else if (candidate_ctx.active == 3)
		cycle_long_option_highlight();
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(opt_cycle_left, NULL, opt_cycle_left_task, NULL,
		     ".cli_cmd_line");

static int opt_cycle_right_task(void *pch)
{
	candidate_ctx.highlight_index++;
	if (candidate_ctx.active == 2)
		cycle_all_option_highlight();
	else if (candidate_ctx.active == 3)
		cycle_long_option_highlight();
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(opt_cycle_right, NULL, opt_cycle_right_task, NULL,
		     ".cli_cmd_line");

static int opt_cycle_up_task(void *pch)
{
	candidate_ctx.highlight_index -= candidate_ctx.cols;
	if (candidate_ctx.active == 2)
		cycle_all_option_highlight();
	else if (candidate_ctx.active == 3)
		cycle_long_option_highlight();
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(opt_cycle_up, NULL, opt_cycle_up_task, NULL,
		     ".cli_cmd_line");

static int opt_cycle_down_task(void *pch)
{
	candidate_ctx.highlight_index += candidate_ctx.cols;
	if (candidate_ctx.active == 2)
		cycle_all_option_highlight();
	else if (candidate_ctx.active == 3)
		cycle_long_option_highlight();
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(opt_cycle_down, NULL, opt_cycle_down_task, NULL,
		     ".cli_cmd_line");

/* ------------------------------------------------------------
 * 历史记录状态
 * ------------------------------------------------------------ */

static int history_up_task(void *pch)
{
	int status;
	clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
	candidate_ctx_clear();
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
	clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
	candidate_ctx_clear();
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

/* ------------------------------------------------------------
 * Tab 补全状态（仅处理首次补全）
 * ------------------------------------------------------------ */

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

static void extract_current_cmd_name(char *cmd_name, int buf_size,
				     int cmd_start, int first_word_end)
{
	int len = first_word_end - cmd_start;
	if (len >= buf_size)
		len = buf_size - 1;
	memcpy(cmd_name, cmd_line.buf + cmd_start, len);
	cmd_name[len] = '\0';
}

static int tab_complete_task(void *pch)
{
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
		extract_current_cmd_name(cmd_name, CMD_LINE_BUF_SIZE,
					 cmd_start, first_word_end);
		const cli_command_t *cmd = find_cmd_by_name(cmd_name);
		cli_mpool_free(cmd_name);
		if (!cmd) {
			cli_out_push((_u8 *)"\a", 1);
			cli_out_sync();
		} else {
			complete_option(cmd, prefix, prefix_len);
		}
	}

	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(tab_complete, NULL, tab_complete_task, NULL,
		     ".cli_cmd_line");

/* ------------------------------------------------------------
 * Tab 循环进入状态（列表已显示，首次进入高亮循环）
 * ------------------------------------------------------------ */

static int tab_cycle_enter_task(void *pch)
{
	if (candidate_ctx.active == 1) {
		candidate_ctx.cycling = 1;
		cycle_cmd_candidate_highlight();
	} else if (candidate_ctx.active == 2) {
		candidate_ctx.cycling = 2;
		cycle_all_option_highlight();
	} else if (candidate_ctx.active == 3) {
		candidate_ctx.cycling = 2;
		cycle_long_option_highlight();
	}
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(tab_cycle_enter, NULL, tab_cycle_enter_task, NULL,
		     ".cli_cmd_line");

/* ------------------------------------------------------------
 * Tab 循环继续状态（已在高亮循环中，切到下一个）
 * ------------------------------------------------------------ */

static int tab_cycle_task(void *pch)
{
	candidate_ctx.highlight_index++;
	if (candidate_ctx.active == 1) {
		cycle_cmd_candidate_highlight();
	} else if (candidate_ctx.active == 2) {
		cycle_all_option_highlight();
	} else if (candidate_ctx.active == 3) {
		cycle_long_option_highlight();
	}
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(tab_cycle, NULL, tab_cycle_task, NULL, ".cli_cmd_line");

/* ------------------------------------------------------------
 * Delete / Backspace / Clear / Enter / Exit
 * ------------------------------------------------------------ */

static int delete_in_middle(void)
{
	int status;
	for (int i = cmd_line.pos; i < cmd_line.size; i++) {
		cmd_line.buf[i] = cmd_line.buf[i + 1];
	}
	cmd_line.buf[cmd_line.size - 1] = ' ';
	int writeNums = cmd_line.size - cmd_line.pos;
	status = cli_out_push((_u8 *)&cmd_line.buf[cmd_line.pos], writeNums);
	if (status < 0)
		return status;
	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;
	int pos_move_cnt = cmd_line.size - cmd_line.pos;
	while (pos_move_cnt--) {
		status = cli_out_push((_u8 *)"\033[D", 4);
		if (status < 0)
			return status;
		if (cli_out_sync())
			return CLI_ERR_IO_SYNC;
	}
	cmd_line.size--;
	return CLI_OK;
}

static int delete_task(void *pch)
{
	candidate_ctx_clear();
	int status;
	if (cmd_line.pos < cmd_line.size)
		status = delete_in_middle();
	else
		status = CLI_OK;
	if (status < 0)
		return status;
	return state_switch(&cmd_line_mec, "exit_handler");
}
_EXPORT_STATE_SYMBOL(delete, NULL, delete_task, NULL, ".cli_cmd_line");

static int backspace_at_tail(void)
{
	int status = cli_out_push((_u8 *)"\b \b", 4);
	if (status < 0)
		return status;
	cmd_line.size--;
	cmd_line.pos--;
	return CLI_OK;
}

static int backspace_in_middle(void)
{
	int status = cli_out_push((_u8 *)"\b \b", 4);
	if (status < 0)
		return status;
	for (int i = cmd_line.pos - 1; i < cmd_line.size - 1; i++) {
		cmd_line.buf[i] = cmd_line.buf[i + 1];
	}
	cmd_line.buf[cmd_line.size - 1] = ' ';
	status = cli_out_push((_u8 *)&cmd_line.buf[cmd_line.pos - 1],
			      cmd_line.size - cmd_line.pos + 1);
	if (status < 0)
		return status;
	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;
	int pos_move_cnt = cmd_line.size - cmd_line.pos + 1;
	while (pos_move_cnt--) {
		status = cli_out_push((_u8 *)"\033[D", 4);
		if (status < 0)
			return status;
		if (cli_out_sync())
			return CLI_ERR_IO_SYNC;
	}
	cmd_line.size--;
	cmd_line.pos--;
	return CLI_OK;
}

static int backspace_handler(void *pch)
{
	int status;
	if (candidate_ctx.active) {
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
		candidate_ctx_clear();
		cmd_line_redraw();
	}
	if (cmd_line.pos != 0 && cmd_line.pos == cmd_line.size)
		status = backspace_at_tail();
	else if (cmd_line.pos == 0)
		status = CLI_OK;
	else
		status = backspace_in_middle();
	if (status < 0)
		return status;
	return state_switch(&cmd_line_mec, "exit_handler");
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
	all_printk("\033[K");
	cli_prompt_print();
	candidate_redraw();
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
	if (candidate_ctx.active) {
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
		candidate_ctx_clear();
		cmd_line_redraw();
		return state_switch(&cmd_line_mec, "exit_handler");
	}
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
			clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
			return status;
		}
	}
	return CLI_OK;
}

/* ============================================================
 *  cli_printk（从 cli_io.c 迁移至此，直接访问 cmd_line 状态）
 * ============================================================ */

extern int scheduler_is_in_get_char(void);

static bool printk_should_drop(const char *pre)
{
	if (pre[0] != '8' && pre[0] >= '0' && pre[0] <= '7') {
		if (pre[0] > log_level[0])
			return true;
	}
	if ((!is_kern_level(pre[0]) || !strcmp(pre, KERN_CONT)) &&
	    strcmp("8", log_level))
		return true;
	return false;
}

static int printk_format_and_send(const char *pre_str, int raw_len)
{
	if (is_kern_level(buffer[0]))
		memmove(buffer, buffer + 1, CLI_PRINTK_BUF_SIZE - 1);
	int pre_len = strlen(pre_str);
	if (raw_len <= 0 || pre_len < 0)
		return 0;
	memmove(buffer + pre_len, buffer, raw_len + 1);
	memcpy(buffer, pre_str, pre_len);
	strcat(buffer, COLOR_NONE);
	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;
	int status = cli_out_push((_u8 *)buffer,
				  pre_len + raw_len + strlen(COLOR_NONE));
	if (status < 0)
		return status;
	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;
	return 0;
}

int cli_printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	char pre[2] = { buffer[0], '\0' };
	if (printk_should_drop(pre))
		return 0;

	int in_interactive = scheduler_is_in_get_char();
	if (in_interactive)
		cli_out_push((_u8 *)"\r\033[K", 4);

	const char *_pre = prefix_gen(pre);
	int status = printk_format_and_send(_pre, len);
	if (status < 0)
		return status;

	if (in_interactive) {
		if (candidate_ctx.active)
			candidate_redraw();
		else
			cmd_line_redraw();
	}
	return len;
}

/* ============================================================
 *  内存池占用情况打印（分配失败时自动调用，不申请内存）
 * ============================================================ */

void cli_mpool_dump_usage(void)
{
	const char *owners[CLI_MPOOL_COUNT];
	int used_count = 0;

	cli_mpool_get_usage(owners, &used_count);

	pr_crit("[mpool] exhausted! %d/%d blocks used\r\n", used_count,
		CLI_MPOOL_COUNT);
	for (int i = 0; i < used_count; i++) {
		pr_crit("[mpool]   [%d] %s\r\n", i,
			owners[i] ? owners[i] : "unknown");
	}
}
