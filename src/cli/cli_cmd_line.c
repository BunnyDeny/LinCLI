/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
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

#include "cli_cmd_line.h"
#include "cli_errno.h"
#include "cli_io.h"
#include "cli_mpool.h"
#include "cmd_dispose.h"
#include "init_d.h"
#include "stateM.h"
#include <stdarg.h>
#include <string.h>
#include "cli_edit.h"
#include "cli_history.h"
#include "cli_completion.h"

extern struct tState *const _cli_cmd_line_start[];
extern struct tState *const _cli_cmd_line_end[];

static bool is_valid_char(char c);

struct origin_cmd origin_cmd = {
	.size = 0,
};

struct tStateEngine cmd_line_mec;

/* ============================================================
 *  状态机任务函数
 * ============================================================ */

static int cmd_line_start_task(void *pch)
{
	int status;
	char ch = *((char *)pch);
	if (is_valid_char(ch)) {
		status = state_switch(&cmd_line_mec, STATE_ID_valid_char);
		if (status < 0) {
			return status;
		}
	} else {
		status = state_switch(&cmd_line_mec, STATE_ID_invalid_char);
		if (status < 0) {
			return status;
		}
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(cmd_line_start, STATE_ID_cmd_line_start, NULL,
		     cmd_line_start_task, NULL, ".cli_cmd_line");

static int valid_char_task(void *pch)
{
	char ch = *((char *)pch);
	if (candidate_ctx.active != CAND_ACTIVE_NONE) {
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
		candidate_ctx_clear();
		cmd_line_redraw();
	}
	if (cmd_line.size == CMD_LINE_BUF_SIZE) {
		pr_warn("command length exceeds the limit. \r\n");
		return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
	}
	int status;
	if (cmd_line.pos == cmd_line.size)
		status = valid_char_append(ch);
	else
		status = valid_char_insert(ch);
	if (status < 0)
		return status;
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(valid_char, STATE_ID_valid_char, NULL, valid_char_task,
		     NULL, ".cli_cmd_line");

static int invalid_char_task(void *pch)
{
	char ch = *((char *)pch);
	int next_state;
	switch ((unsigned char)ch) {
	case 27:
		next_state = STATE_ID_ESC_handler;
		break;
	case 127:
		next_state = STATE_ID_backspace_handler;
		break;
	case '\n':
		next_state = STATE_ID_enter;
		break;
	case '\r':
		next_state = STATE_ID_enter;
		break;
	case '\t':
		if (candidate_ctx.cycling != CAND_CYCLING_NONE) {
			next_state = STATE_ID_tab_cycle;
		} else if (candidate_ctx.active != CAND_ACTIVE_NONE) {
			next_state = STATE_ID_tab_cycle_enter;
		} else {
			next_state = STATE_ID_tab_complete;
		}
		break;
	case 12:
		next_state = STATE_ID_clear;
		break;
	default:
		next_state = STATE_ID_exit_handler;
		break;
	}
	int status = state_switch(&cmd_line_mec, next_state);
	if (status < 0)
		return status;
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(invalid_char, STATE_ID_invalid_char, NULL,
		     invalid_char_task, NULL, ".cli_cmd_line");

/* ------------------------------------------------------------
 * ESC 序列解析与分发状态
 * ------------------------------------------------------------ */

#if CLI_ENABLE_ADVANCED_COMPLETION
static bool is_opt_cycle_active(void)
{
	cand_active_t a = candidate_ctx.active;
	return a == CAND_ACTIVE_ALL_OPTS || a == CAND_ACTIVE_LONG_OPTS;
}
#endif

static int esc_resolve_horizontal(char seq)
{
#if CLI_ENABLE_ADVANCED_COMPLETION
	cand_cycling_t is_cycle = candidate_ctx.cycling;
	if (seq == 'D') { // left
		if (candidate_ctx.active == CAND_ACTIVE_CMD && is_cycle)
			return STATE_ID_cmd_cycle_left;
		if (is_opt_cycle_active() && is_cycle)
			return STATE_ID_opt_cycle_left;
		if (candidate_ctx.active == CAND_ACTIVE_VALUES)
			return STATE_ID_value_cycle_prev;
		return STATE_ID_cursor_left;
	}
	// right
	if (candidate_ctx.active == CAND_ACTIVE_CMD && is_cycle)
		return STATE_ID_cmd_cycle_right;
	if (is_opt_cycle_active() && is_cycle)
		return STATE_ID_opt_cycle_right;
	if (candidate_ctx.active == CAND_ACTIVE_VALUES)
		return STATE_ID_value_cycle_next;
	return STATE_ID_cursor_right;
#else
	(void)seq;
	return (seq == 'D') ? STATE_ID_cursor_left : STATE_ID_cursor_right;
#endif
}

static int esc_resolve_vertical(char seq)
{
#if CLI_ENABLE_ADVANCED_COMPLETION
	cand_cycling_t is_cycle = candidate_ctx.cycling;
	if (seq == 'A') { // up
		if (candidate_ctx.active == CAND_ACTIVE_CMD && is_cycle)
			return STATE_ID_cmd_cycle_up;
		if (is_opt_cycle_active() && is_cycle)
			return STATE_ID_opt_cycle_up;
		if (candidate_ctx.active == CAND_ACTIVE_VALUES)
			return STATE_ID_value_cycle_prev;
		return STATE_ID_history_up;
	}
	// down
	if (candidate_ctx.active == CAND_ACTIVE_CMD && is_cycle)
		return STATE_ID_cmd_cycle_down;
	if (is_opt_cycle_active() && is_cycle)
		return STATE_ID_opt_cycle_down;
	if (candidate_ctx.active == CAND_ACTIVE_VALUES)
		return STATE_ID_value_cycle_next;
	return STATE_ID_history_down;
#else
	(void)seq;
	return (seq == 'A') ? STATE_ID_history_up : STATE_ID_history_down;
#endif
}

static int esc_read_params(char *esc_params)
{
	int status;
	char ch;
	int esc_params_count = 2;

	while (esc_params_count) {
		status = cli_in_pop((_u8 *)&ch, 1);
		if (status < 0)
			return status;
		if (status == 0)
			return CLI_ERR_FIFO_EMPTY;
		esc_params[2 - esc_params_count] = ch;
		esc_params_count--;
	}
	return CLI_OK;
}

static int esc_resolve_sequence(char seq, int *next_state)
{
	char ch;
	int status;

	*next_state = STATE_ID_exit_handler;
	if (seq == 'D' || seq == 'C')
		*next_state = esc_resolve_horizontal(seq);
	else if (seq == 'A' || seq == 'B')
		*next_state = esc_resolve_vertical(seq);
	else if (seq == '3') {
		status = cli_in_pop((_u8 *)&ch, 1);
		if (status < 0)
			return status;
		if (status == 0)
			return CLI_ERR_FIFO_EMPTY;
		if (ch == '~')
			*next_state = STATE_ID_delete;
	}
	return CLI_OK;
}

static int ESC_handler(void *pch)
{
	int status;
	char esc_params[2];
	int next_state;

	status = esc_read_params(esc_params);
	if (status < 0)
		return status;

	status = esc_resolve_sequence(esc_params[1], &next_state);
	if (status < 0)
		return status;

	status = state_switch(&cmd_line_mec, next_state);
	if (status < 0)
		return status;
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(ESC_handler, STATE_ID_ESC_handler, NULL, ESC_handler, NULL,
		     ".cli_cmd_line");

/* ------------------------------------------------------------
 * 光标移动状态
 * ------------------------------------------------------------ */

static int cursor_left_task(void *pch)
{
	if (cmd_line.pos > 0) {
		cli_out_push((_u8 *)"\033[D", 4);
		cmd_line.pos--;
	}
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(cursor_left, STATE_ID_cursor_left, NULL, cursor_left_task,
		     NULL, ".cli_cmd_line");

static int cursor_right_task(void *pch)
{
	if (cmd_line.pos < cmd_line.size) {
		cli_out_push((_u8 *)"\033[C", 4);
		cmd_line.pos++;
	}
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(cursor_right, STATE_ID_cursor_right, NULL,
		     cursor_right_task, NULL, ".cli_cmd_line");

#if CLI_ENABLE_ADVANCED_COMPLETION
/* ------------------------------------------------------------
 * 候选列表导航状态（命令/选项/值共用）
 * ------------------------------------------------------------ */

static int cycle_left_task(void *pch)
{
	candidate_ctx.highlight_index--;
	completer_cycle();
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(cmd_cycle_left, STATE_ID_cmd_cycle_left, NULL,
		     cycle_left_task, NULL, ".cli_cmd_line");
_EXPORT_STATE_SYMBOL(opt_cycle_left, STATE_ID_opt_cycle_left, NULL,
		     cycle_left_task, NULL, ".cli_cmd_line");

static int cycle_right_task(void *pch)
{
	candidate_ctx.highlight_index++;
	completer_cycle();
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(cmd_cycle_right, STATE_ID_cmd_cycle_right, NULL,
		     cycle_right_task, NULL, ".cli_cmd_line");
_EXPORT_STATE_SYMBOL(opt_cycle_right, STATE_ID_opt_cycle_right, NULL,
		     cycle_right_task, NULL, ".cli_cmd_line");

static int cycle_up_task(void *pch)
{
	candidate_ctx.highlight_index -= candidate_ctx.cols;
	completer_cycle();
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(cmd_cycle_up, STATE_ID_cmd_cycle_up, NULL, cycle_up_task,
		     NULL, ".cli_cmd_line");
_EXPORT_STATE_SYMBOL(opt_cycle_up, STATE_ID_opt_cycle_up, NULL, cycle_up_task,
		     NULL, ".cli_cmd_line");

static int cycle_down_task(void *pch)
{
	candidate_ctx.highlight_index += candidate_ctx.cols;
	completer_cycle();
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(cmd_cycle_down, STATE_ID_cmd_cycle_down, NULL,
		     cycle_down_task, NULL, ".cli_cmd_line");
_EXPORT_STATE_SYMBOL(opt_cycle_down, STATE_ID_opt_cycle_down, NULL,
		     cycle_down_task, NULL, ".cli_cmd_line");

/* ------------------------------------------------------------
 * 值候选列表导航状态
 * ------------------------------------------------------------ */

static int value_cycle_prev_task(void *pch)
{
	if (candidate_ctx.cycling == CAND_CYCLING_NONE)
		candidate_ctx.highlight_index = -1;
	else
		candidate_ctx.highlight_index--;
	completer_cycle();
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(value_cycle_prev, STATE_ID_value_cycle_prev, NULL,
		     value_cycle_prev_task, NULL, ".cli_cmd_line");

static int value_cycle_next_task(void *pch)
{
	if (candidate_ctx.cycling == CAND_CYCLING_NONE)
		candidate_ctx.highlight_index = 0;
	else
		candidate_ctx.highlight_index++;
	completer_cycle();
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(value_cycle_next, STATE_ID_value_cycle_next, NULL,
		     value_cycle_next_task, NULL, ".cli_cmd_line");
#endif /* CLI_ENABLE_ADVANCED_COMPLETION */

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
	status = state_switch(&cmd_line_mec, STATE_ID_exit_handler);
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(history_up, STATE_ID_history_up, NULL, history_up_task,
		     NULL, ".cli_cmd_line");

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
	status = state_switch(&cmd_line_mec, STATE_ID_exit_handler);
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(history_down, STATE_ID_history_down, NULL,
		     history_down_task, NULL, ".cli_cmd_line");

/* ------------------------------------------------------------
 * Tab 补全状态（仅处理首次补全）
 * ------------------------------------------------------------ */

/* ============================================================
 *  字符串选项值候选补全
 * ============================================================ */

static int tab_complete_task(void *pch)
{
	int tok_start, prefix_len;
	const char *prefix;
	get_token_prefix(&tok_start, &prefix_len, &prefix);

	int cmd_start, first_word_end;
	get_first_word_bounds(&cmd_start, &first_word_end);

	if (cmd_line.size == 0 ||
	    (tok_start >= cmd_start && tok_start < first_word_end) ||
	    cmd_start >= cmd_line.size) {
		complete_command_name(prefix, prefix_len);
#if CLI_ENABLE_ADVANCED_COMPLETION
	} else {
		int status = try_complete_option(prefix, prefix_len, cmd_start,
						 first_word_end);
		if (status < 0)
			return status;
#endif
	}

	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(tab_complete, STATE_ID_tab_complete, NULL,
		     tab_complete_task, NULL, ".cli_cmd_line");

#if CLI_ENABLE_ADVANCED_COMPLETION
/* ------------------------------------------------------------
 * Tab 循环进入状态（列表已显示，首次进入高亮循环）
 * ------------------------------------------------------------ */

static int tab_cycle_enter_task(void *pch)
{
	if (candidate_ctx.highlight_index < 0)
		candidate_ctx.highlight_index = 0;
	completer_cycle();
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(tab_cycle_enter, STATE_ID_tab_cycle_enter, NULL,
		     tab_cycle_enter_task, NULL, ".cli_cmd_line");

/* ------------------------------------------------------------
 * Tab 循环继续状态（已在高亮循环中，切到下一个）
 * ------------------------------------------------------------ */

static int tab_cycle_task(void *pch)
{
	candidate_ctx.highlight_index++;
	completer_cycle();
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(tab_cycle, STATE_ID_tab_cycle, NULL, tab_cycle_task, NULL,
		     ".cli_cmd_line");
#endif /* CLI_ENABLE_ADVANCED_COMPLETION */

/* ------------------------------------------------------------
 * Delete / Backspace / Clear / Enter / Exit
 * ------------------------------------------------------------ */

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
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(delete, STATE_ID_delete, NULL, delete_task, NULL,
		     ".cli_cmd_line");

static int backspace_handler(void *pch)
{
	int status;
	if (candidate_ctx.active != CAND_ACTIVE_NONE) {
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
	return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
}
_EXPORT_STATE_SYMBOL(backspace_handler, STATE_ID_backspace_handler, NULL,
		     backspace_handler, NULL, ".cli_cmd_line");

static int clear_handler(void *arg)
{
	int status;
	status = cli_out_push((_u8 *)"\x1b[H\x1b[2J", sizeof("\x1b[H\x1b[2J"));
	if (status < 0) {
		return status;
	}
	status = cli_out_push((_u8 *)"\033[K", 3);
	if (status < 0) {
		return status;
	}
	if (cli_out_sync()) {
		return CLI_ERR_IO_SYNC;
	}
	cmd_line_redraw();
	candidate_redraw();
	status = state_switch(&cmd_line_mec, STATE_ID_exit_handler);
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(clear, STATE_ID_clear, NULL, clear_handler, NULL,
		     ".cli_cmd_line");

static void enter_entry(void *pch)
{
	memset(origin_cmd.buf, 0, CMD_LINE_BUF_SIZE);
}
static int enter_press(void *pch)
{
	if (candidate_ctx.active != CAND_ACTIVE_NONE) {
		clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
		candidate_ctx_clear();
		cmd_line_redraw();
		return state_switch(&cmd_line_mec, STATE_ID_exit_handler);
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
_EXPORT_STATE_SYMBOL(enter, STATE_ID_enter, enter_entry, enter_press, NULL,
		     ".cli_cmd_line");

static int cmd_line_exit_handler(void *pch)
{
	int status = state_switch(&cmd_line_mec, STATE_ID_cmd_line_start);
	if (status < 0) {
		return status;
	}
	reset_cli_in_push_lock();
	return cmd_line_exit;
}
_EXPORT_STATE_SYMBOL(exit_handler, STATE_ID_exit_handler, NULL,
		     cmd_line_exit_handler, NULL, ".cli_cmd_line");

__attribute__((used)) static bool is_valid_char(char c)
{
	if (c >= 'a' && c <= 'z')
		return true;
	if (c >= 'A' && c <= 'Z')
		return true;
	if (c >= '0' && c <= '9')
		return true;
	switch (c) {
	case ' ':
	case '~':
	case '!':
	case '@':
	case '#':
	case '$':
	case '%':
	case '^':
	case '&':
	case '*':
	case '(':
	case ')':
	case '-':
	case '_':
	case '=':
	case '+':
	case '[':
	case ']':
	case '{':
	case '}':
	case '|':
	case '\\':
	case ';':
	case ':':
	case '\'':
	case '"':
	case ',':
	case '.':
	case '<':
	case '>':
	case '/':
	case '?':
		return true;
	}
	return false;
}

int cli_cmd_line_init(void)
{
	int status = engine_init(&cmd_line_mec, STATE_ID_cmd_line_start,
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
			state_switch(&cmd_line_mec, STATE_ID_cmd_line_start);
			return status;
		}
		if (status == cmd_line_enter_press) {
			clear_and_up(candidate_ctx.rows, candidate_ctx.rows);
			return status;
		}
	}
	return CLI_OK;
}
