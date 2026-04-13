
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <getopt.h>
#include <assert.h>

#define dispose_exit 1

/* ============================================================
 * 类型系统定义
 * ============================================================ */

typedef enum {
	CLI_TYPE_BOOL, // 开关类型，无参数
	CLI_TYPE_STRING, // 字符串类型
	CLI_TYPE_INT, // 单个整数
	CLI_TYPE_INT_ARRAY, // 整数数组
	CLI_TYPE_DOUBLE, // 浮点数
	CLI_TYPE_CALLBACK, // 自定义回调处理
} cli_type_t;

typedef struct cli_option {
	char short_opt; // 短选项，如 'v'
	const char *long_opt; // 长选项，如 "verbose"
	cli_type_t type; // 类型
	const char *help; // 帮助文本
	size_t offset; // 在结构体中的偏移量
	size_t offset_count; // 数组长度的偏移量（仅用于数组类型）
	size_t max_args; // 最大参数个数（数组类型用）
	bool required; // 是否必需
	const char *depends; // 依赖的选项（字符串形式）
	const char *conflicts; // 互斥的选项
} cli_option_t;

typedef struct cli_command {
	const char *name; // 命令名
	const char *doc; // 命令说明
	void *arg_struct; // 参数结构体指针
	size_t arg_struct_size; // 结构体大小
	const cli_option_t *options; // 选项数组
	size_t option_count; // 选项数量
	int (*validator)(void *); // 自定义验证函数
} cli_command_t;

/* ============================================================
 * 链接脚本段收集符号声明
 * ============================================================ */

extern cli_command_t _cli_commands_start;
extern cli_command_t _cli_commands_end;

#define _FOR_EACH_CLI_COMMAND(_start, _end, _cmd) \
	for ((_cmd) = (_start); (_cmd) < (_end); (_cmd)++)

static inline const cli_command_t *cli_command_find(const char *_name)
{
	cli_command_t *_cmd;
	_FOR_EACH_CLI_COMMAND(&_cli_commands_start, &_cli_commands_end, _cmd)
	{
		if (_cmd->name && strcmp(_cmd->name, _name) == 0)
			return _cmd;
	}
	return NULL;
}

/* ============================================================
 * 核心解析函数声明
 * ============================================================ */

int cli_parse(const char *cmd_name, int argc, char **argv, void *arg_struct);
int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv, void *arg_struct);
void cli_print_help(const cli_command_t *cmd);

/* ============================================================
 * 宏工具：计算偏移量
 * ============================================================ */

#define CLI_OFFSETOF(type, field) offsetof(type, field)

// 安全地获取数组长度的偏移量（假设紧跟在数组指针后面的是_count字段）
#define CLI_ARRAY_COUNT_OFFSET(type, field) \
	(offsetof(type, field) + sizeof(((type *)0)->field))

/* ============================================================
 * OPTION 宏定义
 * ============================================================
 *
 * 注意：原先使用 GET_MACRO 的变参重载存在两个缺陷：
 * 1. 宏参数名与结构体成员名冲突，导致指定初始化器被意外替换
 *    例如 .short_opt = short_opt 会被展开成 .'v' = 'v'
 * 2. OPTION_6(required) 和 OPTION_7(max_args) 都接受 7 个实参，
 *    C 预处理器无法通过参数数量区分二者。
 * 因此改为显式命名宏，且参数名前加下划线避免与成员名冲突。
 */

// 基础OPTION宏（无可选参数）
#define OPTION(_sopt, _lopt, _type, _help, _stype, _field) \
	{ .short_opt = _sopt,                                      \
	  .long_opt = _lopt,                                       \
	  .type = CLI_TYPE_##_type,                                \
	  .help = _help,                                           \
	  .offset = CLI_OFFSETOF(_stype, _field),                  \
	  .offset_count = 0,                                       \
	  .max_args = 1,                                           \
	  .required = false,                                       \
	  .depends = NULL,                                         \
	  .conflicts = NULL }

// 带.required的OPTION
#define OPTION_REQ(_sopt, _lopt, _type, _help, _stype, _field, _req) \
	{ .short_opt = _sopt,                                      \
	  .long_opt = _lopt,                                       \
	  .type = CLI_TYPE_##_type,                                \
	  .help = _help,                                           \
	  .offset = CLI_OFFSETOF(_stype, _field),                  \
	  .offset_count = 0,                                       \
	  .max_args = 1,                                           \
	  .required = _req,                                        \
	  .depends = NULL,                                         \
	  .conflicts = NULL }

// 带.max_args的OPTION（用于数组类型）
#define OPTION_ARRAY(_sopt, _lopt, _type, _help, _stype, _field, _max) \
	{ .short_opt = _sopt,                                      \
	  .long_opt = _lopt,                                       \
	  .type = CLI_TYPE_##_type,                                \
	  .help = _help,                                           \
	  .offset = CLI_OFFSETOF(_stype, _field),                  \
	  .offset_count = CLI_OFFSETOF(_stype, _field##_count),    \
	  .max_args = _max,                                        \
	  .required = false,                                       \
	  .depends = NULL,                                         \
	  .conflicts = NULL }

// 带.max_args和.depends的OPTION
#define OPTION_ARRAY_DEP(_sopt, _lopt, _type, _help, _stype, _field, _max, _dep) \
	{ .short_opt = _sopt,                                      \
	  .long_opt = _lopt,                                       \
	  .type = CLI_TYPE_##_type,                                \
	  .help = _help,                                           \
	  .offset = CLI_OFFSETOF(_stype, _field),                  \
	  .offset_count = CLI_OFFSETOF(_stype, _field##_count),    \
	  .max_args = _max,                                        \
	  .required = false,                                       \
	  .depends = _dep,                                         \
	  .conflicts = NULL }

/* ============================================================
 * 位置参数宏（非选项参数）
 * ============================================================ */

#define POSITIONAL(index, name, _type, _stype, _field) \
	{ .short_opt = 0,                                      \
	  .long_opt = name,                                    \
	  .type = CLI_TYPE_##_type,                            \
	  .help = name " (positional)",                        \
	  .offset = CLI_OFFSETOF(_stype, _field),              \
	  .offset_count = 0,                                   \
	  .max_args = 1,                                       \
	  .required = true,                                    \
	  .depends = NULL,                                     \
	  .conflicts = NULL }

/* ============================================================
 * CLI_COMMAND宏：注册一个命令（通过链接脚本段收集）
 * ============================================================
 *
 * 参数名前加下划线，避免与 cli_command_t 成员名冲突导致
 * 指定初始化器被意外替换。
 */

#define _EXPORT_CLI_COMMAND_SYMBOL(_obj, _cmd_str, _doc_str, _size, _opts, _opts_cnt, _vld, _section) \
	static cli_command_t _cli_cmd_def_##_obj __attribute__((            \
		used, section(_section), aligned(sizeof(long)))) = {        \
		.name = _cmd_str,                                           \
		.doc = _doc_str,                                            \
		.arg_struct = NULL,                                         \
		.arg_struct_size = _size,                                   \
		.options = _opts,                                           \
		.option_count = _opts_cnt,                                  \
		.validator = _vld,                                          \
	}

#define CLI_COMMAND(name, cmd_str, parse_cb, arg_struct_ptr, ...)      \
	/* 前向声明参数结构体类型 */                                    \
	typedef typeof(*arg_struct_ptr) _cli_struct_##name;              \
	                                                                 \
	/* 定义选项数组（放在静态区） */                                \
	static const cli_option_t _cli_options_##name[] = {              \
		__VA_ARGS__                                              \
	};                                                               \
	                                                                 \
	/* 通过链接脚本段收集注册 */                                    \
	_EXPORT_CLI_COMMAND_SYMBOL(                                      \
		name, cmd_str, "Command " cmd_str,                       \
		sizeof(_cli_struct_##name), _cli_options_##name,           \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),      \
		(int (*)(void *))parse_cb, ".cli_commands")

#define END_OPTIONS /* 结束标记，实际为空 */

int dispose_init(void);
int dispose_task(char *cmd);

#endif
