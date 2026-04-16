#ifndef _CLI_DISP_CHAR_H_
#define _CLI_DISP_CHAR_H_

#define HISTORY_MAX 4
#define CMD_LINE_BUF_SIZE 256
#define cmd_line_exit 1
#define cmd_line_enter_press 2

struct origin_cmd {
	char buf[CMD_LINE_BUF_SIZE];
	int size;
};

extern struct origin_cmd origin_cmd;

int cli_cmd_line_init(void);
int cli_cmd_line_task(char ch);

#endif
