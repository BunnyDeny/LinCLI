#ifndef _CMD_DISPOSE_H_
#define _CMD_DISPOSE_H_

#define dispose_exit 1

extern struct tStateEngine dispose_mec;
extern struct tState _dispose_start;
extern struct tState _dispose_end;

int dispose_task(char *cmd);

#endif
