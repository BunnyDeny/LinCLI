#ifndef _CLI_COMPLETION_H_
#define _CLI_COMPLETION_H_

#include "cmd_dispose.h"
#include "cli_cmd_line.h"

typedef enum {
	CAND_ACTIVE_NONE = 0,
	CAND_ACTIVE_CMD,
	CAND_ACTIVE_ALL_OPTS,
	CAND_ACTIVE_LONG_OPTS,
	CAND_ACTIVE_VALUES,
} cand_active_t;

typedef enum {
	CAND_CYCLING_NONE = 0,
	CAND_CYCLING_CMD,
	CAND_CYCLING_OPT,
} cand_cycling_t;

typedef enum {
	MATCH_TYPE_NONE = 0,
	MATCH_TYPE_PREFIX,
	MATCH_TYPE_SUBSTRING,
} match_type_t;

struct candidate_ctx {
	cand_active_t active;
	char prefix[CMD_LINE_BUF_SIZE];
	int prefix_len;
	const cli_command_t *cmd;
	cli_option_t *opt;
	int highlight_index;
	cand_cycling_t cycling;
	int rows;
	int cols;
	int repl_start;
	int total_count;
};

extern struct candidate_ctx candidate_ctx;

void candidate_ctx_save(cand_active_t active, const char *prefix, int prefix_len,
			const cli_command_t *cmd);
void candidate_ctx_clear(void);

void complete_command_name(const char *prefix, int prefix_len);
#if CLI_ENABLE_ADVANCED_COMPLETION
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
#else
static inline int try_complete_option(const char *prefix, int prefix_len,
				      int cmd_start, int first_word_end)
{
	(void)prefix;
	(void)prefix_len;
	(void)cmd_start;
	(void)first_word_end;
	return 0;
}
#endif

struct cli_completer {
	cand_active_t active;
	cand_cycling_t cycling;
	void (*cycle)(void);
	void (*redraw)(void);
};

const struct cli_completer *get_completer(void);
void completer_cycle(void);
void completer_redraw(void);

int str_common_prefix_len(const char *a, const char *b);
const cli_command_t *find_cmd_by_name(const char *name);
int find_cmd_match(const char *prefix, int prefix_len,
		   const cli_command_t **first_match);

void extract_current_cmd_name(char *cmd_name, int buf_size, int cmd_start,
			      int first_word_end);
void get_token_prefix(int *tok_start, int *prefix_len, const char **prefix);
void get_first_word_bounds(int *cmd_start, int *first_word_end);
void get_prev_token_bounds(int tok_start, int *prev_start, int *prev_len);

#endif
