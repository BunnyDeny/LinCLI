#ifndef _CLI_COMPLETION_H_
#define _CLI_COMPLETION_H_

#include "cmd_dispose.h"
#include "cli_cmd_line.h"

struct candidate_ctx {
	int active;
	char prefix[CMD_LINE_BUF_SIZE];
	int prefix_len;
	const cli_command_t *cmd;
	cli_option_t *opt;
	int highlight_index;
	int cycling;
	int rows;
	int cols;
	int repl_start;
};

extern struct candidate_ctx candidate_ctx;

void candidate_ctx_save(int active, const char *prefix, int prefix_len,
			const cli_command_t *cmd);
void candidate_ctx_clear(void);

void complete_command_name(const char *prefix, int prefix_len);
void complete_option(const cli_command_t *cmd, const char *prefix,
		     int prefix_len);
void complete_string_value(const cli_command_t *cmd, const char *prefix,
			   int prefix_len);
int try_complete_option(const char *prefix, int prefix_len, int cmd_start,
			int first_word_end);

void cycle_cmd_candidate_highlight(void);
void cycle_all_option_highlight(void);
void cycle_long_option_highlight(void);
void cycle_value_highlight(void);

void candidate_redraw(void);

int str_common_prefix_len(const char *a, const char *b);
const cli_command_t *find_cmd_by_name(const char *name);
int find_cmd_match(const char *prefix, int prefix_len,
		   const cli_command_t **first_match);

void extract_current_cmd_name(char *cmd_name, int buf_size, int cmd_start,
			      int first_word_end);
void get_token_prefix(int *tok_start, int *prefix_len, const char **prefix);
void get_first_word_bounds(int *cmd_start, int *first_word_end);

#endif
