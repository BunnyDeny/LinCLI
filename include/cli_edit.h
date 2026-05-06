#ifndef _CLI_EDIT_H_
#define _CLI_EDIT_H_

#include "cli_cmd_line.h"
#include "cli_io.h"

struct cmd_line {
	_u8 pos;
	char buf[CMD_LINE_BUF_SIZE];
	_u8 size;
};

extern struct cmd_line cmd_line;

void cmd_line_redraw(void);
void cmd_line_replace(const char *new_buf, int new_size);
int get_last_token_start(const char *buf, int size);
int get_current_segment_start(const char *buf, int size);

void replace_token_at(int seg_start, const char *token, int tok_len,
		      int append_space);
void replace_cmdline_token(const char *replacement, int repl_len,
			   int append_space);
void replace_long_opt_at(int tok_start, const char *long_opt, int long_len);
void replace_long_option_only(const char *long_opt, int long_len);
void replace_long_option(const char *long_opt, int long_len);
void replace_short_option(char c);

int valid_char_append(char ch);
int valid_char_insert(char ch);
int delete_in_middle(void);
int backspace_at_tail(void);
int backspace_in_middle(void);

void clear_and_up(int clears, int ups);

#endif
