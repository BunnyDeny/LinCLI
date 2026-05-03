/*
 * LinCLI - Command line editing buffer and terminal helpers.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "cli_edit.h"
#include "cli_completion.h"
#include "cli_io.h"
#include "cli_mpool.h"
#include "cli_errno.h"
#include <string.h>

void cli_prompt_print(void);

struct cmd_line cmd_line = {
	.pos = 0,
	.size = 0,
	.buf = { 0 },
};

void clear_and_up(int clears, int ups)
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

void cmd_line_replace(const char *new_buf, int new_size)
{
	if (candidate_ctx.rows > 1)
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
	candidate_ctx_clear();
	int status;
	status = cli_out_push((_u8 *)"\r\033[K", 4);
	if (status < 0 || cli_out_sync())
		return;
	status = cli_out_push((_u8 *)"\033[K", 3);
	if (status < 0 || cli_out_sync())
		return;
	cli_prompt_print();
	if (new_size > 0) {
		status = cli_out_push((_u8 *)new_buf, new_size);
		if (status < 0 || cli_out_sync())
			return;
	}
	memset(cmd_line.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(cmd_line.buf, new_buf, new_size);
	cmd_line.size = new_size;
	cmd_line.pos = new_size;
}

int get_last_token_start(const char *buf, int size)
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

void cmd_line_redraw(void)
{
	int status;
	status = cli_out_push((_u8 *)"\r\033[K", 4);
	if (status < 0 || cli_out_sync())
		return;
	status = cli_out_push((_u8 *)"\033[K", 3);
	if (status < 0 || cli_out_sync())
		return;
	cli_prompt_print();
	if (cmd_line.size > 0) {
		status = cli_out_push((_u8 *)cmd_line.buf, cmd_line.size);
		if (status < 0 || cli_out_sync())
			return;
	}
	int back = cmd_line.size - cmd_line.pos;
	while (back-- > 0) {
		status = cli_out_push((_u8 *)"\033[D", 4);
		if (status < 0 || cli_out_sync())
			return;
	}
}

void replace_cmdline_token(const char *replacement, int repl_len,
				  int append_space)
{
	int tok_start = get_last_token_start(cmd_line.buf, cmd_line.size);
	replace_token_at(tok_start, replacement, repl_len, append_space);
}

void replace_token_at(int seg_start, const char *token, int tok_len,
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

void replace_long_opt_at(int tok_start, const char *long_opt,
				int long_len)
{
	char tmp[CMD_LINE_BUF_SIZE];
	tmp[0] = '-';
	tmp[1] = '-';
	memcpy(tmp + 2, long_opt, long_len);
	replace_token_at(tok_start, tmp, 2 + long_len, 1);
}

void replace_long_option_only(const char *long_opt, int long_len)
{
	char tmp[CMD_LINE_BUF_SIZE];
	tmp[0] = '-';
	tmp[1] = '-';
	memcpy(tmp + 2, long_opt, long_len);
	replace_token_at(get_last_token_start(cmd_line.buf, cmd_line.size),
			 tmp, 2 + long_len, 1);
}

void replace_long_option(const char *long_opt, int long_len)
{
	char tmp[CMD_LINE_BUF_SIZE];
	tmp[0] = '-';
	tmp[1] = '-';
	memcpy(tmp + 2, long_opt, long_len);
	replace_token_at(get_last_token_start(cmd_line.buf, cmd_line.size),
			 tmp, 2 + long_len, 0);
}

void replace_short_option(char c)
{
	char tmp[2] = { '-', c };
	replace_token_at(get_last_token_start(cmd_line.buf, cmd_line.size),
			 tmp, 2, 1);
}

int valid_char_append(char ch)
{
	int status = cli_out_push((_u8 *)&ch, 1);
	if (status < 0)
		return status;
	cmd_line.buf[cmd_line.pos] = ch;
	cmd_line.size++;
	cmd_line.pos++;
	return CLI_OK;
}

int valid_char_insert(char ch)
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

int get_current_segment_start(const char *buf, int size)
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

int delete_in_middle(void)
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

int backspace_at_tail(void)
{
	int status = cli_out_push((_u8 *)"\b \b", 4);
	if (status < 0)
		return status;
	cmd_line.size--;
	cmd_line.pos--;
	return CLI_OK;
}

int backspace_in_middle(void)
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

