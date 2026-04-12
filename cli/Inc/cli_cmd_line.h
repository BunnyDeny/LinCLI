#ifndef _CLI_DISP_CHAR_H_
#define _CLI_DISP_CHAR_H_

#define CMD_LINE_BUF_SIZE 256
#define cmd_line_exit 1
#define cmd_line_enter_press 2

struct origin_cmd {
	char buf[CMD_LINE_BUF_SIZE];
	int size;
};

struct cmd_line {
	_u8 pos;
	char buf[CMD_LINE_BUF_SIZE];
	_u8 size;
};

extern struct origin_cmd origin_cmd;
extern struct tStateEngine cmd_line_mec;
extern struct tState _cli_cmd_line_start;
extern struct tState _cli_cmd_line_end;

int cli_cmd_line_task(char ch);

#endif
