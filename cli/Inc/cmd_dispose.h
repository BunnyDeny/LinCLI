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
	char option_name[16]; //选项的名字-T,--help等
	enum option_type
		type; //选项的类型，用于表示选项的参数类型（空，字符串，字符串数组 int数组...）
	int param_num; //选项类型对应的参数个数，填-1代表无需校验个数
	void (*bad_handlers)(
		int argc,
		char **argv); //校验失败处理（可选，填入NULL则启用系统缺省处理）
	void *mem; //如果校验成功了，选项要存储的位置，在解析成功的响应处理中使用这个位置来访问选项的各个参数
	int mem_size; //mem的大小（字节）
	bool is_required; //选项是否必须
	char denp_option[16]; //依赖选项
};
//实现注册命令本质上是实现一个注册选项，将用户定义的所有的struct option
//收集到同一个段中，上电后程序读取这个段的所有内容，并给每一个命令创建一个
//选项红黑树，红黑树的每一个节点对应一个struct option，校验某个命令，本质上
//是遍历校验这个命令对应的选项红黑树的所有节点选项

//所以开发主要分为两步进行，第一步是实现选项注册宏，将所有的struct option
//收集到同一个段
//第二步是根据段中的内容创建每个命令的选项红黑树
//第三步是根据第二步的红黑树完成输入命令的每个选项的校验

int dispose_init(void);
int dispose_task(char *cmd);

#endif
