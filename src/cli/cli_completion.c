/*
 * LinCLI - Tab completion engine (commands, options, values).
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

#include "cli_completion.h"
#include "cli_edit.h"
#include "cli_io.h"
#include "cli_mpool.h"
#include "cli_errno.h"
#include "init_d.h"
#include "cli_user.h"
#include <string.h>

void candidate_list_redraw(int rows);

/* ============================================================
 *  候选列表状态管理（供 cli_printk 重绘使用）
 * ============================================================ */

struct candidate_ctx candidate_ctx = { CAND_ACTIVE_NONE };

void candidate_ctx_save(cand_active_t active, const char *prefix, int prefix_len,
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
	candidate_ctx.cycling = CAND_CYCLING_NONE;
	candidate_ctx.rows = 0;
	candidate_ctx.cols = 0;
}

void candidate_ctx_clear(void)
{
	candidate_ctx.active = CAND_ACTIVE_NONE;
	candidate_ctx.cycling = CAND_CYCLING_NONE;
	candidate_ctx.rows = 0;
	candidate_ctx.cols = 0;
	candidate_ctx.repl_start = -1;
	candidate_ctx.opt = NULL;
	candidate_ctx.highlight_index = 0;
	candidate_ctx.total_count = 0;
}

void candidate_list_redraw(int rows)
{
	for (int i = 0; i < rows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
}

int str_common_prefix_len(const char *a, const char *b)
{
	int i = 0;
	while (a[i] && b[i] && a[i] == b[i])
		i++;
	return i;
}

const cli_command_t *find_cmd_by_name(const char *name)
{
	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(cmd)
	{
		if (cmd->name && strcmp(cmd->name, name) == 0)
			return cmd;
	}
	return NULL;
}

int find_cmd_match(const char *prefix, int prefix_len,
		   const cli_command_t **first_match)
{
	int match_cnt = 0;
	const cli_command_t *cmd;
	(void)prefix_len;
	_FOR_EACH_CLI_COMMAND(cmd)
	{
		if (!cmd->name || !cli_user_cmd_permitted(cmd))
			continue;
		if (strstr(cmd->name, prefix)) {
			match_cnt++;
			if (match_cnt == 1)
				*first_match = cmd;
		}
	}
	return match_cnt;
}

/* ============================================================
 *  命令名补全（全面子字符串化）
 * ============================================================ */

/* ============================================================
 *  通用候选匹配辅助结构
 * ============================================================ */
struct cand_match {
	const char *str;	/* 原始字符串 */
	const char *match;	/* prefix 匹配位置 (strstr 结果) */
};
#define MAX_CAND 32
static struct cand_match cand_buf[MAX_CAND];
static int collect_cmd_matches(const char *prefix, struct cand_match *out, int max)
{
	int n = 0;
	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(cmd)
	{
		if (!cmd->name || !cli_user_cmd_permitted(cmd))
			continue;
		const char *p = strstr(cmd->name, prefix);
		if (p && n < max) {
			out[n].str = cmd->name;
			out[n].match = p;
			n++;
		}
	}
	return n;
}
#if CLI_ENABLE_ADVANCED_COMPLETION
static int collect_opt_matches(const cli_command_t *cmd, const char *prefix,
				struct cand_match *out, int max)
{
	int n = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		const char *p = strstr(cmd->options[i].long_opt, prefix);
		if (p && n < max) {
			out[n].str = cmd->options[i].long_opt;
			out[n].match = p;
			n++;
		}
	}
	return n;
}
static int collect_val_matches(cli_option_t *opt, const char *prefix,
				struct cand_match *out, int max)
{
	int n = 0;
	for (int i = 0; i < opt->candidate_argc; i++) {
		const char *p = strstr(opt->candidate_argv[i], prefix);
		if (p && n < max) {
			out[n].str = opt->candidate_argv[i];
			out[n].match = p;
			n++;
		}
	}
	return n;
}
#endif
static void match_stats_from_array(struct cand_match *m, int count,
				   int *prefix_cnt, int *substr_cnt, int *total)
{
	*prefix_cnt = 0;
	*substr_cnt = 0;
	for (int i = 0; i < count; i++) {
		if (m[i].match == m[i].str)
			(*prefix_cnt)++;
		else
			(*substr_cnt)++;
	}
	*total = *prefix_cnt + *substr_cnt;
}
static bool all_match_pos_same_from_array(struct cand_match *m, int count,
					  int *first_pos)
{
	if (count == 0)
		return false;
	*first_pos = (int)(m[0].match - m[0].str);
	for (int i = 1; i < count; i++) {
		if ((int)(m[i].match - m[i].str) != *first_pos)
			return false;
	}
	return true;
}
static int compute_backward_lcp_from_array(struct cand_match *m, int count,
					   char *lcp_buf, int lcp_buf_size)
{
	int lcp_len = lcp_buf_size;
	for (int i = 0; i < count; i++) {
		if (i == 0) {
			int len = (int)strlen(m[i].match);
			if (len < lcp_len)
				lcp_len = len;
			memcpy(lcp_buf, m[i].match, lcp_len);
			if (lcp_len < lcp_buf_size)
				lcp_buf[lcp_len] = '\0';
		} else {
			int cpl = str_common_prefix_len(lcp_buf, m[i].match);
			if (cpl < lcp_len)
				lcp_len = cpl;
		}
	}
	return count > 0 ? lcp_len : 0;
}
static int compute_forward_len_from_array(struct cand_match *m, int count,
					  int first_pos)
{
	if (first_pos <= 0 || count == 0)
		return 0;
	int forward = 0;
	while (first_pos - forward > 0) {
		int idx = first_pos - forward - 1;
		char c = m[0].str[idx];
		bool same = true;
		for (int i = 1; i < count; i++) {
			int pos = (int)(m[i].match - m[i].str);
			if (pos - forward <= 0 || m[i].str[idx] != c) {
				same = false;
				break;
			}
		}
		if (!same)
			break;
		forward++;
	}
	return forward;
}
static int build_lcp_str(int first_pos, int forward, int backward,
			 const char *src, char *lcp_buf, int lcp_buf_size)
{
	int total = forward + backward;
	if (total > lcp_buf_size)
		total = lcp_buf_size;
	if (total <= 0)
		return 0;
	if (forward > 0 && first_pos - forward >= 0)
		memcpy(lcp_buf, src + first_pos - forward, forward);
	if (backward > 0)
		memcpy(lcp_buf + forward, src + first_pos, backward);
	lcp_buf[total] = '\0';
	return total;
}

static int compute_lcp_from_array(struct cand_match *m, int count,
				  const char *prefix, int prefix_len,
				  char *lcp_buf, int lcp_buf_size)
{
	int first_pos;
	if (!all_match_pos_same_from_array(m, count, &first_pos))
		return prefix_len;
	int backward = compute_backward_lcp_from_array(m, count, lcp_buf,
						       lcp_buf_size);
	int forward = compute_forward_len_from_array(m, count, first_pos);
	if (count == 0)
		return prefix_len;
	return build_lcp_str(first_pos, forward, backward, m[0].str,
			     lcp_buf, lcp_buf_size);
}
static const char *find_match_by_index_from_array(struct cand_match *m, int count,
					    int idx, bool prefix_only)
{
	int cur = 0;
	for (int i = 0; i < count; i++) {
		bool is_prefix = (m[i].match == m[i].str);
		if (is_prefix != prefix_only)
			continue;
		if (cur == idx)
			return m[i].str;
		cur++;
	}
	return NULL;
}
static const char *find_unified_match_from_array(struct cand_match *m, int count,
					   int idx)
{
	const char *s = find_match_by_index_from_array(m, count, idx, true);
	if (s)
		return s;
	int prefix_cnt = 0;
	for (int i = 0; i < count; i++) {
		if (m[i].match == m[i].str)
			prefix_cnt++;
	}
	return find_match_by_index_from_array(m, count, idx - prefix_cnt, false);
}

static void cmd_match_stats(const char *prefix, int *prefix_cnt,
				    int *substr_cnt, int *total)
{
	int n = collect_cmd_matches(prefix, cand_buf, MAX_CAND);
	match_stats_from_array(cand_buf, n, prefix_cnt, substr_cnt, total);
}





static int compute_substring_lcp(const char *prefix, int prefix_len,
					 char *lcp_buf, int lcp_buf_size)
{
	int n = collect_cmd_matches(prefix, cand_buf, MAX_CAND);
	return compute_lcp_from_array(cand_buf, n, prefix, prefix_len,
				      lcp_buf, lcp_buf_size);
}

void complete_unique_cmd(const cli_command_t *match)
{
	clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
	replace_cmdline_token(match->name, (int)strlen(match->name), 1);
	cmd_line_redraw();
}

static const cli_command_t *cmd_find_unique_str_match(const char *prefix)
{
	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(cmd)
	{
		if (!cmd->name || !cli_user_cmd_permitted(cmd))
			continue;
		if (strstr(cmd->name, prefix))
			return cmd;
	}
	return NULL;
}

static void beep_and_redraw(void)
{
	clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
	candidate_ctx_clear();
	cli_out_push((_u8 *)"\a", 1);
	cli_out_sync();
	cmd_line_redraw();
}


static void display_one_cmd_grouped(const char *name, int max_len,
				    int idx, match_type_t type,
				    int highlight_idx,
				    int *cur_cow, int *cur_idx)
{
	if (*cur_cow == 0)
		cli_out_push((_u8 *)"\r\n", 2);
	if (idx == highlight_idx)
		cli_out_push((_u8 *)"\033[7m", 4);
	if (type == MATCH_TYPE_SUBSTRING)
		cli_out_push((_u8 *)COLOR_DIM, sizeof(COLOR_DIM) - 1);
	cli_out_push((_u8 *)name, strlen(name));
	if (type == MATCH_TYPE_SUBSTRING || idx == highlight_idx)
		cli_out_push((_u8 *)COLOR_NONE, sizeof(COLOR_NONE) - 1);
	int spaces = max_len - strlen(name);
	while (spaces--)
		cli_out_push((_u8 *)" ", 1);
	(*cur_cow)++;
	if (*cur_cow >= candidate_ctx.cols)
		*cur_cow = 0;
	cli_out_sync();
	(*cur_idx)++;
}



static void display_unified_cmd_list(const char *prefix, int prefix_len,
				     int prefix_cnt_unused, int substr_cnt_unused,
				     int highlight_idx)
{
	(void)prefix_cnt_unused;
	(void)substr_cnt_unused;
	int old_rows = candidate_ctx.rows;
	int saved_highlight = candidate_ctx.highlight_index;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(CAND_ACTIVE_CMD, prefix, prefix_len, NULL);
	candidate_ctx.highlight_index = saved_highlight;
	int n = collect_cmd_matches(prefix, cand_buf, MAX_CAND);
	int prefix_cnt = 0, substr_cnt = 0, total = 0;
	match_stats_from_array(cand_buf, n, &prefix_cnt, &substr_cnt,
			       &total);
	int max_len = 0;
	for (int i = 0; i < n; i++) {
		int len = (int)strlen(cand_buf[i].str);
		if (len > max_len)
			max_len = len;
	}
	max_len += 3;
	int cows = DISPLAY_MAX_COWS / max_len;
	if (cows < 1)
		cows = 1;
	candidate_ctx.rows = (prefix_cnt + cows - 1) / cows;
	candidate_ctx.rows += (substr_cnt + cows - 1) / cows;
	candidate_ctx.cols = cows;
	candidate_ctx.total_count = prefix_cnt + substr_cnt;
	int cur_cow = 0, cur_idx = 0;
	for (int i = 0; i < n; i++) {
		if (cand_buf[i].match != cand_buf[i].str)
			continue;
		display_one_cmd_grouped(cand_buf[i].str, max_len, cur_idx,
					MATCH_TYPE_PREFIX,
					highlight_idx, &cur_cow, &cur_idx);
	}
	if (prefix_cnt && substr_cnt && cur_cow) {
		cli_out_push((_u8 *)"\r\n", 2);
		cur_cow = 1;
	}
	for (int i = 0; i < n; i++) {
		if (cand_buf[i].match == cand_buf[i].str)
			continue;
		display_one_cmd_grouped(cand_buf[i].str, max_len, cur_idx,
					MATCH_TYPE_SUBSTRING,
					highlight_idx, &cur_cow, &cur_idx);
	}
	candidate_list_redraw(candidate_ctx.rows);
}


const cli_command_t *cmd_find_unified_match_by_index(int idx)
{
	const char *prefix = candidate_ctx.prefix;
	int n = collect_cmd_matches(prefix, cand_buf, MAX_CAND);
	const char *name = find_unified_match_from_array(cand_buf, n, idx);
	if (!name)
		return NULL;
	return find_cmd_by_name(name);
}

static void normalize_highlight_index(int total)
{
	while (candidate_ctx.highlight_index < 0)
		candidate_ctx.highlight_index =
			total + candidate_ctx.highlight_index;
	while (candidate_ctx.highlight_index >= total)
		candidate_ctx.highlight_index =
			candidate_ctx.highlight_index % total;
}

void cycle_cmd_candidate_highlight(void)
{
	clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
	int total = candidate_ctx.total_count;
	normalize_highlight_index(total);
	int n = collect_cmd_matches(candidate_ctx.prefix, cand_buf, MAX_CAND);
	const char *target =
		find_unified_match_from_array(cand_buf, n,
					      candidate_ctx.highlight_index);
	if (!target)
		return;
	int tok_start =
		get_current_segment_start(cmd_line.buf, cmd_line.size);
	replace_token_at(tok_start, target, (int)strlen(target), 1);
	int prefix_cnt = 0, substr_cnt = 0;
	match_stats_from_array(cand_buf, n, &prefix_cnt, &substr_cnt,
			       &total);
	display_unified_cmd_list(candidate_ctx.prefix, candidate_ctx.prefix_len,
				 prefix_cnt, substr_cnt,
				 candidate_ctx.highlight_index);
	candidate_ctx.active = CAND_ACTIVE_CMD;
	candidate_ctx.cycling = CAND_CYCLING_CMD;
}

void complete_command_name(const char *prefix, int prefix_len)
{
	int prefix_cnt = 0, substr_cnt = 0, total = 0;
	cmd_match_stats(prefix, &prefix_cnt, &substr_cnt, &total);
	if (total == 0) {
		beep_and_redraw();
		return;
	}
	if (total == 1) {
		complete_unique_cmd(cmd_find_unique_str_match(prefix));
		return;
	}
	if (prefix_cnt > 0 && substr_cnt > 0) {
		display_unified_cmd_list(prefix, prefix_len, prefix_cnt,
					 substr_cnt, -1);
		return;
	}
	char *lcp = cli_mpool_alloc();
	if (!lcp) {
		pr_err("out of memory\r\n");
		return;
	}
	int lcp_len = compute_substring_lcp(prefix, prefix_len, lcp,
					    CMD_LINE_BUF_SIZE);
	if (lcp_len > prefix_len) {
		replace_cmdline_token(lcp, lcp_len, 0);
		cmd_line_redraw();
	} else {
		display_unified_cmd_list(prefix, prefix_len, prefix_cnt,
					 substr_cnt, -1);
	}
	cli_mpool_free(lcp);
}
#if CLI_ENABLE_ADVANCED_COMPLETION

/* ============================================================
 *  选项补全（全面子字符串化）
 * ============================================================ */

static int long_opt_match_pos(cli_option_t *opt, const char *prefix)
{
	if (!opt->long_opt)
		return -1;
	const char *p = strstr(opt->long_opt, prefix);
	if (!p)
		return -1;
	return (int)(p - opt->long_opt);
}

static void long_opt_match_stats(const cli_command_t *cmd, const char *prefix,
					 int *prefix_cnt, int *substr_cnt, int *total)
{
	int n = collect_opt_matches(cmd, prefix, cand_buf, MAX_CAND);
	match_stats_from_array(cand_buf, n, prefix_cnt, substr_cnt, total);
}




static int compute_option_lcp(const char *prefix, int prefix_len,
				      const cli_command_t *cmd,
				      char *lcp_buf, int lcp_buf_size)
{
	int n = collect_opt_matches(cmd, prefix, cand_buf, MAX_CAND);
	return compute_lcp_from_array(cand_buf, n, prefix, prefix_len,
				      lcp_buf, lcp_buf_size);
}

static cli_option_t *long_opt_find_unique_match(const cli_command_t *cmd,
							const char *prefix)
{
	int n = collect_opt_matches(cmd, prefix, cand_buf, MAX_CAND);
	if (n != 1)
		return NULL;
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].long_opt == cand_buf[0].str)
			return &cmd->options[i];
	}
	return NULL;
}

static void display_one_option_ex(cli_option_t *opt, int idx,
				  match_type_t type, int *cows,
				  int highlight_idx, bool only_long)
{
	cli_out_push((_u8 *)"\r\n", 2);
	(*cows)++;
	if (idx == highlight_idx)
		cli_out_push((_u8 *)"\033[7m", 4);
	if (type == MATCH_TYPE_SUBSTRING)
		cli_out_push((_u8 *)COLOR_DIM, sizeof(COLOR_DIM) - 1);
	bool is_dash_prefix = (candidate_ctx.prefix_len >= 2 &&
				       candidate_ctx.prefix[0] == '-' &&
				       candidate_ctx.prefix[1] == '-');
	if (!only_long && !is_dash_prefix && opt->short_opt) {
		char buf[4] = { '-', opt->short_opt, ' ', '\0' };
		cli_out_push((_u8 *)buf, 3);
	}
	if (opt->long_opt) {
		cli_out_push((_u8 *)"--", 2);
		cli_out_push((_u8 *)opt->long_opt, strlen(opt->long_opt));
	}
	if (idx == highlight_idx || type == MATCH_TYPE_SUBSTRING)
		cli_out_push((_u8 *)COLOR_NONE, sizeof(COLOR_NONE) - 1);
	cli_out_sync();
}

static void display_opt_group(const cli_command_t *cmd, const char *prefix,
				      int *cows, int *idx, int highlight_idx,
				      bool prefix_only)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		int pos = long_opt_match_pos(opt, prefix);
		if (pos < 0)
			continue;
		bool is_prefix = (pos == 0);
		if (is_prefix != prefix_only)
			continue;
		display_one_option_ex(opt, (*idx)++,
				      is_prefix ? MATCH_TYPE_PREFIX : MATCH_TYPE_SUBSTRING,
				      cows, highlight_idx, false);
	}
}

static void display_option_list(const cli_command_t *cmd,
				const char *prefix, int prefix_len,
				int highlight_idx)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(CAND_ACTIVE_LONG_OPTS, prefix, prefix_len, cmd);
	candidate_ctx.highlight_index = highlight_idx;
	int prefix_cnt = 0, substr_cnt = 0, total = 0;
	long_opt_match_stats(cmd, prefix, &prefix_cnt, &substr_cnt,
			     &total);
	int cows = 0, idx = 0;
	display_opt_group(cmd, prefix, &cows, &idx, highlight_idx, true);
	if (prefix_cnt && substr_cnt) {
		cli_out_push((_u8 *)"\r\n", 2);
		cows++;
	}
	display_opt_group(cmd, prefix, &cows, &idx, highlight_idx, false);
	candidate_ctx.rows = cows;
	candidate_ctx.cols = 1;
	candidate_ctx.total_count = prefix_cnt + substr_cnt;
	candidate_list_redraw(candidate_ctx.rows);
}








static int build_option_token(cli_option_t *opt, char *buf)
{
	int pos = 0;
	if (opt->long_opt && candidate_ctx.prefix_len >= 2 &&
	    candidate_ctx.prefix[0] == '-' && candidate_ctx.prefix[1] == '-') {
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

static int get_option_repl_start(void)
{
	int tok_start = candidate_ctx.repl_start;
	if (tok_start < 0 || tok_start > cmd_line.size)
		tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	return tok_start;
}

static int apply_option_to_cmdline(cli_option_t *opt, int tok_start)
{
	char *new_buf = cli_mpool_alloc();
	if (!new_buf) {
		pr_err("out of memory\r\n");
		return -1;
	}
	memcpy(new_buf, cmd_line.buf, tok_start);
	int repl_len = build_option_token(opt, new_buf + tok_start);
	int new_size = tok_start + repl_len;
	if (new_size < CMD_LINE_BUF_SIZE - 1)
		new_buf[new_size++] = ' ';
	memset(cmd_line.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(cmd_line.buf, new_buf, new_size);
	cmd_line.size = new_size;
	cmd_line.pos = new_size;
	cli_mpool_free(new_buf);
	return 0;
}

static void candidate_ctx_restore_after_list(cand_active_t active,
					     cand_cycling_t cycling,
					     int saved_repl_start,
					     int saved_highlight)
{
	candidate_ctx.active = active;
	candidate_ctx.cycling = cycling;
	candidate_ctx.repl_start = saved_repl_start;
	candidate_ctx.highlight_index = saved_highlight;
}


static void list_all_options_internal(const cli_command_t *cmd,
					      const char *prefix,
					      int prefix_len, int highlight_idx,
					      bool only_long)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(CAND_ACTIVE_ALL_OPTS, prefix, prefix_len, cmd);
	candidate_ctx.highlight_index = highlight_idx;
	int cows = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		display_one_option_ex(&cmd->options[i], (int)i,
				      MATCH_TYPE_NONE, &cows,
				      highlight_idx, only_long);
	}
	candidate_ctx.rows = cows;
	candidate_ctx.cols = 1;
	candidate_list_redraw(candidate_ctx.rows);
}

void list_all_options(const cli_command_t *cmd, const char *prefix,
		      int prefix_len, int highlight_idx)
{
	list_all_options_internal(cmd, prefix, prefix_len, highlight_idx, false);
}

void refresh_all_option_highlight(const cli_command_t *cmd)
{
	int saved_repl_start = candidate_ctx.repl_start;
	int saved_highlight = candidate_ctx.highlight_index;
	list_all_options(cmd, candidate_ctx.prefix, candidate_ctx.prefix_len,
			 candidate_ctx.highlight_index);
	candidate_ctx_restore_after_list(CAND_ACTIVE_ALL_OPTS,
					 CAND_CYCLING_OPT,
					 saved_repl_start,
					 saved_highlight);
}

void cycle_all_option_highlight(void)
{
	const cli_command_t *cmd = candidate_ctx.cmd;
	if (!cmd)
		return;
	normalize_highlight_index((int)cmd->option_count);
	cli_option_t *target =
		&cmd->options[candidate_ctx.highlight_index];
	int tok_start = get_option_repl_start();
	if (apply_option_to_cmdline(target, tok_start) < 0)
		return;
	candidate_ctx.repl_start = tok_start;
	refresh_all_option_highlight(cmd);
}


void cycle_long_option_highlight(void)
{
	const cli_command_t *cmd = candidate_ctx.cmd;
	if (!cmd)
		return;
	int n = collect_opt_matches(cmd, candidate_ctx.prefix, cand_buf, MAX_CAND);
	if (n == 0)
		return;
	normalize_highlight_index(n);
	const char *target = find_unified_match_from_array(
		cand_buf, n, candidate_ctx.highlight_index);
	if (!target)
		return;
	int tok_start = candidate_ctx.repl_start;
	if (tok_start < 0 || tok_start > cmd_line.size)
		tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	replace_long_opt_at(tok_start, target, (int)strlen(target));
	int saved_repl_start = tok_start;
	int saved_highlight = candidate_ctx.highlight_index;
	display_option_list(cmd, candidate_ctx.prefix,
			     candidate_ctx.prefix_len,
			     candidate_ctx.highlight_index);
	candidate_ctx_restore_after_list(CAND_ACTIVE_LONG_OPTS,
					 CAND_CYCLING_OPT,
					 saved_repl_start,
					 saved_highlight);
}

bool is_token_match_option(int start, int len, cli_option_t *opt)
{
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

bool is_last_full_token_the_only_option(const cli_command_t *cmd)
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
	cli_option_t *opt = &cmd->options[0];
	return is_token_match_option(start, len, opt);
}

void do_complete_short_option(char c, const cli_command_t *cmd)
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

void complete_long_option(const cli_command_t *cmd,
			  const char *name_prefix, int name_prefix_len)
{
	int prefix_cnt = 0, substr_cnt = 0, total = 0;
	long_opt_match_stats(cmd, name_prefix, &prefix_cnt, &substr_cnt,
			     &total);
	if (total == 0) {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
		return;
	}
	if (total == 1) {
		cli_option_t *match = long_opt_find_unique_match(cmd, name_prefix);
		if (match) {
			replace_long_option_only(match->long_opt,
						 (int)strlen(match->long_opt));
			cmd_line_redraw();
		}
		return;
	}
	if (prefix_cnt > 0 && substr_cnt > 0) {
		display_option_list(cmd, name_prefix,
				    name_prefix_len, -1);
		return;
	}
	char *lcp = cli_mpool_alloc();
	if (!lcp) {
		pr_err("out of memory\r\n");
		return;
	}
	int lcp_len = compute_option_lcp(name_prefix, name_prefix_len, cmd,
					 lcp, CMD_LINE_BUF_SIZE);
	if (lcp_len > name_prefix_len) {
		replace_long_option(lcp, lcp_len);
		cmd_line_redraw();
	} else {
		display_option_list(cmd, name_prefix,
				    name_prefix_len, -1);
	}
	cli_mpool_free(lcp);
}

void complete_option_empty_prefix(const cli_command_t *cmd)
{
	if (cmd->option_count == 1) {
		cli_option_t *opt = &cmd->options[0];
		if (is_last_full_token_the_only_option(cmd)) {
			cli_out_push((_u8 *)"\a", 1);
			cli_out_sync();
		} else if (opt->long_opt) {
			replace_long_option_only(opt->long_opt,
						 (int)strlen(opt->long_opt));
			cmd_line_redraw();
		} else if (opt->short_opt) {
			replace_short_option(opt->short_opt);
			cmd_line_redraw();
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

void complete_option_dash_prefix(const cli_command_t *cmd,
				 const char *prefix, int prefix_len)
{
	if (cmd->option_count == 1) {
		cli_option_t *opt = &cmd->options[0];
		if (opt->long_opt) {
			replace_long_option(opt->long_opt,
					    (int)strlen(opt->long_opt));
			cmd_line_redraw();
		} else if (opt->short_opt) {
			replace_short_option(opt->short_opt);
			cmd_line_redraw();
		} else {
			cli_out_push((_u8 *)"\a", 1);
			cli_out_sync();
		}
	} else {
		list_all_options_internal(cmd, prefix, prefix_len, -1, true);
		candidate_ctx.repl_start =
			get_last_token_start(cmd_line.buf, cmd_line.size);
	}
}

void complete_option(const cli_command_t *cmd, const char *prefix,
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
		complete_option_dash_prefix(cmd, prefix, prefix_len);
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
 *  值补全（全面子字符串化）
 * ============================================================ */

static int value_match_pos(char *val, const char *prefix)
{
	const char *p = strstr(val, prefix);
	if (!p)
		return -1;
	return (int)(p - val);
}

static void value_match_stats(cli_option_t *opt, const char *prefix,
				      int *prefix_cnt, int *substr_cnt, int *total)
{
	int n = collect_val_matches(opt, prefix, cand_buf, MAX_CAND);
	match_stats_from_array(cand_buf, n, prefix_cnt, substr_cnt, total);
}




static int compute_value_lcp(const char *prefix, int prefix_len,
				     cli_option_t *opt,
				     char *lcp_buf, int lcp_buf_size)
{
	int n = collect_val_matches(opt, prefix, cand_buf, MAX_CAND);
	return compute_lcp_from_array(cand_buf, n, prefix, prefix_len,
				      lcp_buf, lcp_buf_size);
}

static char *value_find_unique_match(cli_option_t *opt, const char *prefix)
{
	int n = collect_val_matches(opt, prefix, cand_buf, MAX_CAND);
	if (n != 1)
		return NULL;
	return (char *)cand_buf[0].str;
}

static void complete_unique_value(char *match)
{
	replace_cmdline_token(match, (int)strlen(match), 1);
	cmd_line_redraw();
}

static void push_value_candidate_grouped(char *val, int idx,
					 match_type_t type,
						 int highlight_idx)
{
	cli_out_push((_u8 *)"\r\n", 2);
	if (idx == highlight_idx)
		cli_out_push((_u8 *)"\033[7m", 4);
	if (type == MATCH_TYPE_SUBSTRING)
		cli_out_push((_u8 *)COLOR_DIM, sizeof(COLOR_DIM) - 1);
	cli_out_push((_u8 *)val, strlen(val));
	if (idx == highlight_idx || type == MATCH_TYPE_SUBSTRING)
		cli_out_push((_u8 *)COLOR_NONE, sizeof(COLOR_NONE) - 1);
	cli_out_sync();
}

static void display_value_group(cli_option_t *opt, const char *prefix,
				int *cows, int *idx, int highlight_idx,
				bool prefix_only)
{
	for (int i = 0; i < opt->candidate_argc; i++) {
		int pos = value_match_pos(opt->candidate_argv[i], prefix);
		if (pos < 0)
			continue;
		bool is_prefix = (pos == 0);
		if (is_prefix != prefix_only)
			continue;
		push_value_candidate_grouped(opt->candidate_argv[i], (*idx)++,
					     is_prefix ? MATCH_TYPE_PREFIX : MATCH_TYPE_SUBSTRING,
					     highlight_idx);
		(*cows)++;
	}
}

static void display_value_list(const char *prefix, int prefix_len,
				       int highlight_idx)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(CAND_ACTIVE_VALUES, prefix, prefix_len,
			   candidate_ctx.cmd);
	candidate_ctx.highlight_index = highlight_idx;
	cli_option_t *opt = candidate_ctx.opt;
	if (!opt || opt->candidate_argc <= 0)
		return;
	int prefix_cnt = 0, substr_cnt = 0, total = 0;
	value_match_stats(opt, prefix, &prefix_cnt, &substr_cnt, &total);
	int cows = 0, idx = 0;
	display_value_group(opt, prefix, &cows, &idx, highlight_idx, true);
	if (prefix_cnt && substr_cnt) {
		cli_out_push((_u8 *)"\r\n", 2);
		cows++;
	}
	display_value_group(opt, prefix, &cows, &idx, highlight_idx, false);
	candidate_ctx.rows = cows;
	candidate_ctx.cols = 1;
	candidate_ctx.total_count = prefix_cnt + substr_cnt;
	candidate_list_redraw(candidate_ctx.rows);
}








void refresh_value_highlight(char *match)
{
	(void)match;
	int saved_repl_start = candidate_ctx.repl_start;
	int saved_highlight = candidate_ctx.highlight_index;
	cli_option_t *opt = candidate_ctx.opt;
	if (opt && opt->candidate_argc > 0)
		display_value_list(candidate_ctx.prefix,
				   candidate_ctx.prefix_len,
				   candidate_ctx.highlight_index);
	candidate_ctx_restore_after_list(CAND_ACTIVE_VALUES,
					 CAND_CYCLING_OPT,
					 saved_repl_start,
					 saved_highlight);
}

void cycle_value_highlight(void)
{
	const cli_command_t *cmd = candidate_ctx.cmd;
	cli_option_t *opt = candidate_ctx.opt;
	if (!cmd || !opt || opt->candidate_argc <= 0)
		return;
	int n = collect_val_matches(opt, candidate_ctx.prefix, cand_buf, MAX_CAND);
	if (n == 0)
		return;
	normalize_highlight_index(n);
	const char *target = find_unified_match_from_array(cand_buf, n,
					   candidate_ctx.highlight_index);
	if (!target)
		return;
	int tok_start = candidate_ctx.repl_start;
	if (tok_start < 0 || tok_start > cmd_line.size)
		tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	replace_token_at(tok_start, target, (int)strlen(target), 1);
	candidate_ctx.repl_start = tok_start;
	refresh_value_highlight((char *)target);
	candidate_ctx.active = CAND_ACTIVE_VALUES;
	candidate_ctx.cycling = CAND_CYCLING_OPT;
}

void do_complete_string_value(cli_option_t *opt,
			      const char *prefix, int prefix_len)
{
	int prefix_cnt = 0, substr_cnt = 0, total = 0;
	value_match_stats(opt, prefix, &prefix_cnt, &substr_cnt, &total);
	if (total == 0) {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
		return;
	}
	if (total == 1) {
		complete_unique_value(value_find_unique_match(opt, prefix));
		return;
	}
	if (prefix_cnt > 0 && substr_cnt > 0) {
		display_value_list(prefix, prefix_len, -1);
		return;
	}
	char *lcp = cli_mpool_alloc();
	if (!lcp) {
		pr_err("out of memory\r\n");
		return;
	}
	int lcp_len = compute_value_lcp(prefix, prefix_len, opt, lcp,
					CMD_LINE_BUF_SIZE);
	if (lcp_len > prefix_len) {
		replace_cmdline_token(lcp, lcp_len, 0);
		cmd_line_redraw();
	} else {
		display_value_list(prefix, prefix_len, -1);
	}
	cli_mpool_free(lcp);
}

cli_option_t *find_string_option_by_token(const cli_command_t *cmd,
					  int start, int len)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		if (opt->type == CLI_TYPE_STRING &&
		    is_token_match_option(start, len, opt))
			return opt;
	}
	return NULL;
}

bool is_value_completion(const cli_command_t *cmd,
			 const char *prefix, int prefix_len)
{
	if (prefix_len > 0 && prefix[0] == '-')
		return false;
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	int prev_start, prev_len;
	get_prev_token_bounds(tok_start, &prev_start, &prev_len);
	if (prev_len <= 0)
		return false;
	cli_option_t *opt = find_string_option_by_token(cmd, prev_start,
							prev_len);
	return opt && opt->candidate_argc > 0;
}

void complete_string_value(const cli_command_t *cmd,
			   const char *prefix, int prefix_len)
{
	candidate_ctx.cmd = cmd;
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	int prev_start, prev_len;
	get_prev_token_bounds(tok_start, &prev_start, &prev_len);
	cli_option_t *opt = find_string_option_by_token(cmd, prev_start,
							prev_len);
	if (!opt || opt->candidate_argc <= 0) {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
		return;
	}
	candidate_ctx.opt = opt;
	do_complete_string_value(opt, prefix, prefix_len);
	if (candidate_ctx.active == CAND_ACTIVE_VALUES)
		candidate_ctx.repl_start = tok_start;
}

#if CLI_ENABLE_RAW_COMMAND
static bool is_raw_cmd(const cli_command_t *cmd)
{
	return is_raw_command(cmd);
}

static void complete_raw_cmd_value(const cli_command_t *cmd,
				   const char *prefix, int prefix_len)
{
	cli_option_t *opt = &cmd->options[0];
	if (opt->candidate_argc <= 0) {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
		return;
	}
	candidate_ctx.cmd = cmd;
	candidate_ctx.opt = opt;
	do_complete_string_value(opt, prefix, prefix_len);
	if (candidate_ctx.active == CAND_ACTIVE_VALUES)
		candidate_ctx.repl_start =
			get_last_token_start(cmd_line.buf, cmd_line.size);
}
#endif

int try_complete_option(const char *prefix, int prefix_len,
			int cmd_start, int first_word_end)
{
	char *cmd_name = cli_mpool_alloc();
	if (cmd_name == NULL) {
		pr_err("out of memory\r\n");
		return CLI_ERR_NULL;
	}
	extract_current_cmd_name(cmd_name, CMD_LINE_BUF_SIZE, cmd_start,
				 first_word_end);
	const cli_command_t *cmd = find_cmd_by_name(cmd_name);
	cli_mpool_free(cmd_name);
	if (!cmd) {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
	}
#if CLI_ENABLE_RAW_COMMAND
	else if (is_raw_cmd(cmd)) {
		complete_raw_cmd_value(cmd, prefix, prefix_len);
	}
#endif
	else if (is_value_completion(cmd, prefix, prefix_len)) {
		complete_string_value(cmd, prefix, prefix_len);
	} else {
		complete_option(cmd, prefix, prefix_len);
	}
	return 0;
}
static void list_cmd_candidates(const char *prefix, int prefix_len)
{
	int prefix_cnt = 0, substr_cnt = 0, total = 0;
	cmd_match_stats(prefix, &prefix_cnt, &substr_cnt, &total);
	display_unified_cmd_list(prefix, prefix_len, prefix_cnt, substr_cnt,
				 -1);
}

void candidate_redraw_cmd(void)
{
	int saved_cycling = candidate_ctx.cycling;
	if (saved_cycling == CAND_CYCLING_CMD) {
		int prefix_cnt = 0, substr_cnt = 0, total = 0;
		cmd_match_stats(candidate_ctx.prefix, &prefix_cnt, &substr_cnt,
				&total);
		display_unified_cmd_list(candidate_ctx.prefix,
					 candidate_ctx.prefix_len,
					 prefix_cnt, substr_cnt,
					 candidate_ctx.highlight_index);
	} else {
		list_cmd_candidates(candidate_ctx.prefix,
				    candidate_ctx.prefix_len);
	}
	candidate_ctx.cycling = (cand_cycling_t)saved_cycling;
}

static void list_values_wrapper(const cli_command_t *cmd, const char *prefix,
				int prefix_len, int highlight_idx)
{
	(void)cmd;
	display_value_list(prefix, prefix_len, highlight_idx);
}

static void candidate_redraw_generic(
	void (*list_fn)(const cli_command_t *, const char *, int, int),
	cand_active_t active)
{
	int saved_highlight = candidate_ctx.highlight_index;
	int saved_cycling = candidate_ctx.cycling;
	if (saved_cycling == CAND_CYCLING_OPT)
		list_fn(candidate_ctx.cmd, candidate_ctx.prefix,
			candidate_ctx.prefix_len, saved_highlight);
	else
		list_fn(candidate_ctx.cmd, candidate_ctx.prefix,
			candidate_ctx.prefix_len, -1);
	candidate_ctx.highlight_index = saved_highlight;
	candidate_ctx.cycling = (cand_cycling_t)saved_cycling;
	candidate_ctx.active = active;
}

void candidate_redraw_all_opts(void)
{
	candidate_redraw_generic(list_all_options, CAND_ACTIVE_ALL_OPTS);
}

void candidate_redraw_long_opts(void)
{
	candidate_redraw_generic(display_option_list,
				 CAND_ACTIVE_LONG_OPTS);
}

void candidate_redraw_values(void)
{
	candidate_redraw_generic(list_values_wrapper, CAND_ACTIVE_VALUES);
}

static const struct cli_completer cmd_completer = {
	.active = CAND_ACTIVE_CMD,
	.cycling = CAND_CYCLING_CMD,
	.cycle = cycle_cmd_candidate_highlight,
	.redraw = candidate_redraw_cmd,
};

static const struct cli_completer all_opts_completer = {
	.active = CAND_ACTIVE_ALL_OPTS,
	.cycling = CAND_CYCLING_OPT,
	.cycle = cycle_all_option_highlight,
	.redraw = candidate_redraw_all_opts,
};

static const struct cli_completer long_opts_completer = {
	.active = CAND_ACTIVE_LONG_OPTS,
	.cycling = CAND_CYCLING_OPT,
	.cycle = cycle_long_option_highlight,
	.redraw = candidate_redraw_long_opts,
};

static const struct cli_completer values_completer = {
	.active = CAND_ACTIVE_VALUES,
	.cycling = CAND_CYCLING_OPT,
	.cycle = cycle_value_highlight,
	.redraw = candidate_redraw_values,
};

static const struct cli_completer *const completers[] = {
	[CAND_ACTIVE_NONE] = NULL,
	[CAND_ACTIVE_CMD] = &cmd_completer,
	[CAND_ACTIVE_ALL_OPTS] = &all_opts_completer,
	[CAND_ACTIVE_LONG_OPTS] = &long_opts_completer,
	[CAND_ACTIVE_VALUES] = &values_completer,
};

const struct cli_completer *get_completer(void)
{
	return completers[candidate_ctx.active];
}

void completer_cycle(void)
{
	const struct cli_completer *c = get_completer();
	if (c) {
		candidate_ctx.cycling = c->cycling;
		c->cycle();
	}
}

void completer_redraw(void)
{
	const struct cli_completer *c = get_completer();
	if (!c) return;
	if (c->active != CAND_ACTIVE_CMD && !candidate_ctx.cmd) return;
	c->redraw();
}

void candidate_redraw(void)
{
	completer_redraw();
}

#else /* !CLI_ENABLE_ADVANCED_COMPLETION */

/* ============================================================
 *  精简版：仅命令名补全，无选项/值/高亮循环
 * ============================================================ */

static void list_cmd_candidates_simple(const char *prefix, int prefix_len)
{
	int prefix_cnt = 0, substr_cnt = 0, total = 0;
	cmd_match_stats(prefix, &prefix_cnt, &substr_cnt, &total);
	display_unified_cmd_list(prefix, prefix_len, prefix_cnt, substr_cnt,
				 -1);
}

void candidate_redraw_cmd(void)
{
	list_cmd_candidates_simple(candidate_ctx.prefix,
				   candidate_ctx.prefix_len);
}

static const struct cli_completer cmd_completer = {
	.active = CAND_ACTIVE_CMD,
	.cycling = CAND_CYCLING_NONE,
	.cycle = NULL,
	.redraw = candidate_redraw_cmd,
};

const struct cli_completer *get_completer(void)
{
	if (candidate_ctx.active == CAND_ACTIVE_CMD)
		return &cmd_completer;
	return NULL;
}

void completer_cycle(void)
{
	/* 无高亮循环，空操作 */
}

void completer_redraw(void)
{
	const struct cli_completer *c = get_completer();
	if (c && c->redraw)
		c->redraw();
}

void candidate_redraw(void)
{
	completer_redraw();
}

#endif /* CLI_ENABLE_ADVANCED_COMPLETION */

/* ============================================================
 *  通用辅助函数
 * ============================================================ */

void extract_current_cmd_name(char *cmd_name, int buf_size,
			      int cmd_start, int first_word_end)
{
	int len = first_word_end - cmd_start;
	if (len >= buf_size)
		len = buf_size - 1;
	memcpy(cmd_name, cmd_line.buf + cmd_start, len);
	cmd_name[len] = '\0';
}

void get_token_prefix(int *tok_start, int *prefix_len,
		      const char **prefix)
{
	*tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	*prefix_len = cmd_line.size - *tok_start;
	cmd_line.buf[cmd_line.size] = '\0';
	if (*prefix_len == 0)
		*prefix = "";
	else
		*prefix = &cmd_line.buf[*tok_start];
}

void get_first_word_bounds(int *cmd_start, int *first_word_end)
{
	*cmd_start = get_current_segment_start(cmd_line.buf, cmd_line.size);
	while (*cmd_start < cmd_line.size && cmd_line.buf[*cmd_start] == ' ')
		(*cmd_start)++;
	*first_word_end = *cmd_start;
	while (*first_word_end < cmd_line.size &&
	       cmd_line.buf[*first_word_end] != ' ')
		(*first_word_end)++;
}

void get_prev_token_bounds(int tok_start, int *prev_start,
			   int *prev_len)
{
	int i = tok_start - 1;
	while (i >= 0 && cmd_line.buf[i] == ' ')
		i--;
	int end = i;
	while (i >= 0 && cmd_line.buf[i] != ' ')
		i--;
	*prev_start = i + 1;
	*prev_len = end - i;
}
