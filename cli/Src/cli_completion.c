/*
 * LinCLI - Tab completion engine (commands, options, values).
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "cli_completion.h"
#include "cli_edit.h"
#include "cli_io.h"
#include "cli_mpool.h"
#include "cli_errno.h"
#include "init_d.h"
#include <string.h>

/* ============================================================
 *  候选列表状态管理（供 cli_printk 重绘使用）
 * ============================================================ */

struct candidate_ctx candidate_ctx = { 0 };

void candidate_ctx_save(int active, const char *prefix, int prefix_len,
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

void candidate_ctx_clear(void)
{
	candidate_ctx.active = 0;
	candidate_ctx.cycling = 0;
	candidate_ctx.rows = 0;
	candidate_ctx.cols = 0;
	candidate_ctx.repl_start = -1;
	candidate_ctx.opt = NULL;
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
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
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

void complete_unique_cmd(const cli_command_t *match)
{
	clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
	replace_cmdline_token(match->name, (int)strlen(match->name), 1);
}

int compute_cmd_lcp(char *lcp_buf, int lcp_buf_size,
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

void compute_candidate_layout(const char *prefix, int prefix_len,
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

void display_one_candidate(const char *name, int max_len,
				  int highlight_idx, int *cur_cow, int *cur_idx)
{
	if (*cur_cow == 0)
		cli_out_push((_u8 *)"\r\n", 2);
	if (*cur_idx == highlight_idx) {
		cli_out_push((_u8 *)"\033[7m", 4);
		cli_out_push((_u8 *)name, strlen(name));
		cli_out_push((_u8 *)"\033[0m", 4);
	} else {
		cli_out_push((_u8 *)name, strlen(name));
	}
	int space_count = max_len - strlen(name);
	while (space_count--)
		cli_out_push((_u8 *)" ", 1);
	(*cur_cow)++;
	if (*cur_cow >= candidate_ctx.cols)
		*cur_cow = 0;
	cli_out_sync();
	(*cur_idx)++;
}

void display_candidates(const char *prefix, int prefix_len,
			       int display_max_cows, int highlight_idx)
{
	const cli_command_t *cmd;
	int max_len, cnt;
	compute_candidate_layout(prefix, prefix_len, display_max_cows, &max_len,
				 &cnt);

	int cur_cow = 0, cur_idx = 0;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		if (!cmd->name)
			continue;
		if (strncmp(cmd->name, prefix, prefix_len) == 0) {
			display_one_candidate(cmd->name, max_len, highlight_idx,
					      &cur_cow, &cur_idx);
		}
	}
}

void list_cmd_candidates(const char *prefix, int prefix_len)
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

void normalize_highlight_index(int total)
{
	while (candidate_ctx.highlight_index < 0)
		candidate_ctx.highlight_index =
			total + candidate_ctx.highlight_index;
	while (candidate_ctx.highlight_index >= total)
		candidate_ctx.highlight_index =
			candidate_ctx.highlight_index % total;
}

void candidate_list_redraw(int rows)
{
	for (int i = 0; i < rows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
}

int cmd_match_total(void)
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

const cli_command_t *cmd_find_match_by_index(int idx)
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

int long_opt_match_total(const cli_command_t *cmd)
{
	int total = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, candidate_ctx.prefix,
			    candidate_ctx.prefix_len) == 0)
			total++;
	}
	return total;
}

cli_option_t *
long_opt_find_match_by_index(const cli_command_t *cmd, int idx)
{
	int cur = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
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

int build_option_token(cli_option_t *opt, char *buf)
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

void cycle_cmd_candidate_highlight(void)
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

void complete_multi_cmd(const cli_command_t *first_match,
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

void complete_command_name(const char *prefix, int prefix_len)
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

void display_one_option(cli_option_t *opt, int idx,
			       int highlight_idx, int *cows)
{
	cli_out_push((_u8 *)"\r\n", 2);
	(*cows)++;
	if (idx == highlight_idx) {
		cli_out_push((_u8 *)"\033[7m", 4);
	}
	if (opt->short_opt) {
		char buf[4] = { '-', opt->short_opt, ' ', '\0' };
		cli_out_push((_u8 *)buf, 3);
	}
	if (opt->long_opt) {
		cli_out_push((_u8 *)"--", 2);
		cli_out_push((_u8 *)opt->long_opt, strlen(opt->long_opt));
	}
	if (idx == highlight_idx) {
		cli_out_push((_u8 *)"\033[0m", 4);
	}
	cli_out_sync();
}

void list_all_options(const cli_command_t *cmd, const char *prefix,
			     int prefix_len, int highlight_idx)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(2, prefix, prefix_len, cmd);
	int cows = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		display_one_option(&cmd->options[i], (int)i, highlight_idx,
				   &cows);
	}
	candidate_ctx.rows = cows;
	candidate_ctx.cols = 1;
	for (int i = 0; i < candidate_ctx.rows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
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

void display_one_long_option(cli_option_t *opt, int highlight_idx,
				    int *cows)
{
	cli_out_push((_u8 *)"\r\n", 2);
	cli_out_push((_u8 *)"--", 2);
	if (*cows == highlight_idx) {
		cli_out_push((_u8 *)"\033[7m", 4);
	}
	cli_out_push((_u8 *)opt->long_opt, strlen(opt->long_opt));
	if (*cows == highlight_idx) {
		cli_out_push((_u8 *)"\033[0m", 4);
	}
	(*cows)++;
	cli_out_sync();
}

void list_long_option_candidates(const cli_command_t *cmd,
					const char *name_prefix,
					int name_prefix_len, int highlight_idx)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(3, name_prefix, name_prefix_len, cmd);
	int cows = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, name_prefix, name_prefix_len) == 0) {
			display_one_long_option(opt, highlight_idx, &cows);
		}
	}
	candidate_ctx.rows = cows;
	candidate_ctx.cols = 1;
	for (int i = 0; i < candidate_ctx.rows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
}

int get_option_repl_start(void)
{
	int tok_start = candidate_ctx.repl_start;
	if (tok_start < 0 || tok_start > cmd_line.size)
		tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	return tok_start;
}

int apply_option_to_cmdline(cli_option_t *opt, int tok_start)
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

void candidate_ctx_restore_after_list(int active, int cycling,
					     int saved_repl_start,
					     int saved_highlight)
{
	candidate_ctx.active = active;
	candidate_ctx.cycling = cycling;
	candidate_ctx.repl_start = saved_repl_start;
	candidate_ctx.highlight_index = saved_highlight;
}

void refresh_all_option_highlight(const cli_command_t *cmd)
{
	int saved_repl_start = candidate_ctx.repl_start;
	int saved_highlight = candidate_ctx.highlight_index;
	list_all_options(cmd, candidate_ctx.prefix, candidate_ctx.prefix_len,
			 candidate_ctx.highlight_index);
	candidate_ctx_restore_after_list(2, 2, saved_repl_start,
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

	refresh_all_option_highlight(cmd);
}

void refresh_long_option_highlight(const cli_command_t *cmd,
					  cli_option_t *target)
{
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
	candidate_ctx_restore_after_list(3, 2, saved_repl_start,
					 saved_highlight);
}

void cycle_long_option_highlight(void)
{
	const cli_command_t *cmd = candidate_ctx.cmd;
	if (!cmd)
		return;
	int total = long_opt_match_total(cmd);
	if (total == 0)
		return;
	normalize_highlight_index(total);
	cli_option_t *target = long_opt_find_match_by_index(
		cmd, candidate_ctx.highlight_index);
	if (!target)
		return;
	refresh_long_option_highlight(cmd, target);
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

int long_opt_compute_lcp(const cli_command_t *cmd,
				const char *name_prefix, int name_prefix_len,
				cli_option_t *first_match)
{
	int lcp_len = (int)strlen(first_match->long_opt);
	char *lcp = cli_mpool_alloc();
	if (!lcp) {
		pr_err("out of memory\r\n");
		return -1;
	}
	memcpy(lcp, first_match->long_opt, lcp_len);

	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
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

void do_complete_long_option(const cli_command_t *cmd,
				    const char *name_prefix,
				    int name_prefix_len, int match_cnt,
				    cli_option_t *match)
{
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
			candidate_ctx.repl_start = get_last_token_start(
				cmd_line.buf, cmd_line.size);
		}
	} else {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
	}
}

void complete_long_option(const cli_command_t *cmd,
				 const char *name_prefix, int name_prefix_len)
{
	cli_option_t *match = NULL;
	int match_cnt = 0;
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		if (opt->long_opt &&
		    strncmp(opt->long_opt, name_prefix, name_prefix_len) == 0) {
			match_cnt++;
			if (match_cnt == 1)
				match = opt;
		}
	}
	do_complete_long_option(cmd, name_prefix, name_prefix_len, match_cnt,
				match);
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

void complete_option_dash_prefix(const cli_command_t *cmd,
					const char *prefix, int prefix_len)
{
	if (cmd->option_count == 1) {
		cli_option_t *opt = &cmd->options[0];
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

void candidate_redraw_cmd(void)
{
	if (candidate_ctx.cycling == 1) {
		display_candidates(candidate_ctx.prefix,
				   candidate_ctx.prefix_len, DISPLAY_MAX_COWS,
				   candidate_ctx.highlight_index);
		candidate_list_redraw(candidate_ctx.rows);
	} else {
		list_cmd_candidates(candidate_ctx.prefix,
				    candidate_ctx.prefix_len);
	}
}

void candidate_redraw_all_opts(void)
{
	int saved_highlight = candidate_ctx.highlight_index;
	int saved_cycling = candidate_ctx.cycling;
	if (saved_cycling == 2)
		list_all_options(candidate_ctx.cmd, candidate_ctx.prefix,
				 candidate_ctx.prefix_len, saved_highlight);
	else
		list_all_options(candidate_ctx.cmd, candidate_ctx.prefix,
				 candidate_ctx.prefix_len, -1);
	candidate_ctx.highlight_index = saved_highlight;
	candidate_ctx.cycling = saved_cycling;
	candidate_ctx.active = 2;
}

void candidate_redraw_long_opts(void)
{
	int saved_highlight = candidate_ctx.highlight_index;
	int saved_cycling = candidate_ctx.cycling;
	if (saved_cycling == 2)
		list_long_option_candidates(candidate_ctx.cmd,
					    candidate_ctx.prefix,
					    candidate_ctx.prefix_len,
					    saved_highlight);
	else
		list_long_option_candidates(candidate_ctx.cmd,
					    candidate_ctx.prefix,
					    candidate_ctx.prefix_len, -1);
	candidate_ctx.highlight_index = saved_highlight;
	candidate_ctx.cycling = saved_cycling;
	candidate_ctx.active = 3;
}

void candidate_redraw_values(void);

void candidate_redraw(void)
{
	if (candidate_ctx.active == 1) {
		candidate_redraw_cmd();
	} else if (candidate_ctx.active == 2 && candidate_ctx.cmd) {
		candidate_redraw_all_opts();
	} else if (candidate_ctx.active == 3 && candidate_ctx.cmd) {
		candidate_redraw_long_opts();
	} else if (candidate_ctx.active == 4 && candidate_ctx.cmd) {
		candidate_redraw_values();
	}
}

void cycle_value_highlight(void);

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

int find_value_match(char **argv, int argc, const char *prefix,
			    int prefix_len, char **first_match)
{
	int cnt = 0;
	for (int i = 0; i < argc; i++) {
		if (strncmp(argv[i], prefix, prefix_len) == 0) {
			if (cnt == 0)
				*first_match = argv[i];
			cnt++;
		}
	}
	return cnt;
}

int compute_value_lcp(char **argv, int argc, const char *prefix,
			     int prefix_len, char *first_match)
{
	int lcp_len = (int)strlen(first_match);
	for (int i = 0; i < argc; i++) {
		if (strncmp(argv[i], prefix, prefix_len) != 0)
			continue;
		int cpl = str_common_prefix_len(first_match, argv[i]);
		if (cpl < lcp_len)
			lcp_len = cpl;
	}
	return lcp_len;
}

void list_value_candidates(char **argv, int argc,
				  const char *prefix, int prefix_len)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(4, prefix, prefix_len, candidate_ctx.cmd);
	int cows = 0;
	for (int i = 0; i < argc; i++) {
		if (strncmp(argv[i], prefix, prefix_len) != 0)
			continue;
		cli_out_push((_u8 *)"\r\n", 2);
		cli_out_push((_u8 *)argv[i], strlen(argv[i]));
		cows++;
		cli_out_sync();
	}
	candidate_ctx.rows = cows;
	for (int i = 0; i < cows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
}

int value_match_total(cli_option_t *opt)
{
	int total = 0;
	for (int i = 0; i < opt->candidate_argc; i++) {
		if (strncmp(opt->candidate_argv[i], candidate_ctx.prefix,
			    candidate_ctx.prefix_len) == 0)
			total++;
	}
	return total;
}

char *value_find_match_by_index(cli_option_t *opt, int idx)
{
	int cur = 0;
	for (int i = 0; i < opt->candidate_argc; i++) {
		if (strncmp(opt->candidate_argv[i], candidate_ctx.prefix,
			    candidate_ctx.prefix_len) == 0) {
			if (cur == idx)
				return opt->candidate_argv[i];
			cur++;
		}
	}
	return NULL;
}

void push_value_candidate(char *val, int hl)
{
	cli_out_push((_u8 *)"\r\n", 2);
	if (hl)
		cli_out_push((_u8 *)"\033[7m", 4);
	cli_out_push((_u8 *)val, strlen(val));
	if (hl)
		cli_out_push((_u8 *)"\033[0m", 4);
	cli_out_sync();
}

void list_value_candidates_with_highlight(char **argv, int argc,
						 const char *prefix,
						 int prefix_len,
						 int highlight_idx)
{
	int old_rows = candidate_ctx.rows;
	clear_and_up(old_rows, old_rows);
	candidate_ctx_save(4, prefix, prefix_len, candidate_ctx.cmd);
	int cows = 0;
	for (int i = 0; i < argc; i++) {
		if (strncmp(argv[i], prefix, prefix_len) != 0)
			continue;
		push_value_candidate(argv[i], cows == highlight_idx);
		cows++;
	}
	candidate_ctx.rows = cows;
	for (int i = 0; i < cows; i++) {
		cli_out_push((_u8 *)"\033[1A", 4);
		cli_out_sync();
	}
	cmd_line_redraw();
}

void refresh_value_highlight(char *match)
{
	int saved_repl_start = candidate_ctx.repl_start;
	int saved_highlight = candidate_ctx.highlight_index;
	cli_option_t *opt = candidate_ctx.opt;
	if (opt && opt->candidate_argc > 0)
		list_value_candidates_with_highlight(
			opt->candidate_argv, opt->candidate_argc,
			candidate_ctx.prefix, candidate_ctx.prefix_len,
			candidate_ctx.highlight_index);
	candidate_ctx_restore_after_list(4, 2, saved_repl_start,
					 saved_highlight);
}

void cycle_value_highlight(void)
{
	const cli_command_t *cmd = candidate_ctx.cmd;
	cli_option_t *opt = candidate_ctx.opt;
	if (!cmd || !opt || opt->candidate_argc <= 0)
		return;
	int total = value_match_total(opt);
	if (total == 0)
		return;
	normalize_highlight_index(total);
	char *target = value_find_match_by_index(
		opt, candidate_ctx.highlight_index);
	if (!target)
		return;
	int tok_start = candidate_ctx.repl_start;
	if (tok_start < 0 || tok_start > cmd_line.size)
		tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	replace_token_at(tok_start, target, (int)strlen(target), 1);
	refresh_value_highlight(target);
	candidate_ctx.active = 4;
	candidate_ctx.cycling = 2;
}

void candidate_redraw_values(void)
{
	int saved_highlight = candidate_ctx.highlight_index;
	int saved_cycling = candidate_ctx.cycling;
	cli_option_t *opt = candidate_ctx.opt;
	if (opt && opt->candidate_argc > 0)
		list_value_candidates_with_highlight(
			opt->candidate_argv, opt->candidate_argc,
			candidate_ctx.prefix, candidate_ctx.prefix_len,
			saved_cycling == 2 ? saved_highlight : -1);
	candidate_ctx.highlight_index = saved_highlight;
	candidate_ctx.cycling = saved_cycling;
	candidate_ctx.active = 4;
}

void do_complete_string_value(cli_option_t *opt,
				     const char *prefix, int prefix_len)
{
	char *first = NULL;
	int cnt = find_value_match(opt->candidate_argv, opt->candidate_argc,
				   prefix, prefix_len, &first);
	if (cnt == 1) {
		replace_cmdline_token(first, (int)strlen(first), 1);
	} else if (cnt > 1) {
		int lcp_len = compute_value_lcp(opt->candidate_argv,
						opt->candidate_argc,
						prefix, prefix_len, first);
		if (lcp_len > prefix_len)
			replace_cmdline_token(first, lcp_len, 0);
		else
			list_value_candidates(opt->candidate_argv,
					      opt->candidate_argc,
					      prefix, prefix_len);
	} else {
		cli_out_push((_u8 *)"\a", 1);
		cli_out_sync();
	}
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
	if (candidate_ctx.active == 4)
		candidate_ctx.repl_start = tok_start;
}

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
	} else if (is_value_completion(cmd, prefix, prefix_len)) {
		complete_string_value(cmd, prefix, prefix_len);
	} else {
		complete_option(cmd, prefix, prefix_len);
	}
	return 0;
}

