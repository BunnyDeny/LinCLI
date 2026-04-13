
//实现注册命令本质上是实现一个注册选项，将用户定义的所有的struct option
//收集到同一个段中，上电后程序读取这个段的所有内容，并给每一个命令创建一个
//选项红黑树，红黑树的每一个节点对应一个struct option，校验某个命令，本质上
//是遍历校验这个命令对应的选项红黑树的所有节点选项

//所以开发主要分为两步进行，第一步是实现选项注册宏，将所有的struct option
//收集到同一个段
//第二步是根据段中的内容创建每个命令的选项红黑树
//第三步是根据第二步的红黑树完成输入命令的每个选项的校验
#ifndef _CMD_DISPOSE_H_
#define _CMD_DISPOSE_H_

#include <stdbool.h>

#define dispose_exit 1

enum option_type {
	op_null,
	op_int,
	op_int_arr,
	op_string,
	op_string_arr,
};

struct option {
	char cmd_name[16];
	char option_name[16];
	enum option_type type;
	int param_num;
	void (*bad_handler)(int argc, char **argv);
	void *mem;
	int mem_size;
	bool is_required;
	char *denp_option;
} __attribute__((aligned(sizeof(long))));

#define EXPORT_OPTION_SYMBOL(cmd, option, _type, _num, _bad_handler, _mem, \
			     _size, _is_required, denp)                    \
	static struct init_d init_d_##obj __attribute__((                  \
		used, section(".my_option"),                               \
		aligned(sizeof(long)))) = { .cmd_name = #cmd,              \
					    .option_name = #option,        \
					    .type = _type,                 \
					    .param_num = _num,             \
					    .bad_handler = _bad_handler,   \
					    .mem = _mem,                   \
					    .mem_size = _size,             \
					    .is_required = _is_required,   \
					    .denp_option = denp }

int dispose_init(void);
int dispose_task(char *cmd);

#endif
