
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

/* 全局命令注册表（简化版，实际用链接脚本） */
#define CLI_MAX_COMMANDS 64
extern cli_command_t *g_cli_commands[CLI_MAX_COMMANDS];
extern int g_cli_command_count;

/* ============================================================
 * 核心解析函数声明
 * ============================================================ */

int cli_parse(const char *cmd_name, int argc, char **argv, void *arg_struct);
int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv);
void cli_print_help(const cli_command_t *cmd);

/* ============================================================
 * 宏工具：计算偏移量
 * ============================================================ */

// 获取字段偏移量的宏
#define CLI_OFFSETOF(type, field) offsetof(type, field)

// 安全地获取数组长度的偏移量（假设紧跟在数组指针后面的是_count字段）
#define CLI_ARRAY_COUNT_OFFSET(type, field) \
	(offsetof(type, field) + sizeof(((type *)0)->field))

/* ============================================================
 * OPTION宏的重载实现
 * 使用C11 _Generic或宏技巧实现可选参数
 * ============================================================ */

// 基础OPTION宏（无可选参数）
#define OPTION_5(short_opt, long_opt, type_enum, help, struct_type, field) \
	{ .short_opt = short_opt,                                          \
	  .long_opt = long_opt,                                            \
	  .type = CLI_TYPE_##type_enum,                                    \
	  .help = help,                                                    \
	  .offset = CLI_OFFSETOF(struct_type, field),                      \
	  .offset_count = 0,                                               \
	  .max_args = 1,                                                   \
	  .required = false,                                               \
	  .depends = NULL,                                                 \
	  .conflicts = NULL }

// 带.required的OPTION
#define OPTION_6(short_opt, long_opt, type_enum, help, struct_type, field, \
		 req)                                                      \
	{ .short_opt = short_opt,                                          \
	  .long_opt = long_opt,                                            \
	  .type = CLI_TYPE_##type_enum,                                    \
	  .help = help,                                                    \
	  .offset = CLI_OFFSETOF(struct_type, field),                      \
	  .offset_count = 0,                                               \
	  .max_args = 1,                                                   \
	  .required = req,                                                 \
	  .depends = NULL,                                                 \
	  .conflicts = NULL }

// 带.max_args的OPTION（用于数组类型）
#define OPTION_7(short_opt, long_opt, type_enum, help, struct_type, field, \
		 max)                                                      \
	{ .short_opt = short_opt,                                          \
	  .long_opt = long_opt,                                            \
	  .type = CLI_TYPE_##type_enum,                                    \
	  .help = help,                                                    \
	  .offset = CLI_OFFSETOF(struct_type, field),                      \
	  .offset_count = CLI_OFFSETOF(struct_type, field##_count),        \
	  .max_args = max,                                                 \
	  .required = false,                                               \
	  .depends = NULL,                                                 \
	  .conflicts = NULL }

// 带.max_args和.depends的OPTION
#define OPTION_8(short_opt, long_opt, type_enum, help, struct_type, field, \
		 max, dep)                                                 \
	{ .short_opt = short_opt,                                          \
	  .long_opt = long_opt,                                            \
	  .type = CLI_TYPE_##type_enum,                                    \
	  .help = help,                                                    \
	  .offset = CLI_OFFSETOF(struct_type, field),                      \
	  .offset_count = CLI_OFFSETOF(struct_type, field##_count),        \
	  .max_args = max,                                                 \
	  .required = false,                                               \
	  .depends = dep,                                                  \
	  .conflicts = NULL }

// 参数选择宏
#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME

// 统一的OPTION宏
#define OPTION(...)                                          \
	GET_MACRO(__VA_ARGS__, OPTION_8, OPTION_7, OPTION_6, \
		  OPTION_5)(__VA_ARGS__)

/* ============================================================
 * 位置参数宏（非选项参数）
 * ============================================================ */

#define POSITIONAL(index, name, type_enum, struct_type, field) \
	{ .short_opt = 0,                                      \
	  .long_opt = name,                                    \
	  .type = CLI_TYPE_##type_enum,                        \
	  .help = name " (positional)",                        \
	  .offset = CLI_OFFSETOF(struct_type, field),          \
	  .offset_count = 0,                                   \
	  .max_args = 1,                                       \
	  .required = true,                                    \
	  .depends = NULL,                                     \
	  .conflicts = NULL }

/* ============================================================
 * CLI_COMMAND宏：注册一个命令
 * ============================================================ */

// 辅助宏：计算选项数量
#define CLI_COUNT_OPTIONS(...) \
	(sizeof((cli_option_t[]){ __VA_ARGS__ }) / sizeof(cli_option_t))

// 主要的CLI_COMMAND宏
#define CLI_COMMAND(name, cmd_str, parse_cb, arg_struct_ptr, ...)           \
                                                                            \
	/* 前向声明参数结构体类型 */                                        \
	typedef typeof(*arg_struct_ptr) _cli_struct_##name;                 \
                                                                            \
	/* 定义选项数组（放在静态区） */                                    \
	static const cli_option_t _cli_options_##name[] = { __VA_ARGS__ };  \
                                                                            \
	/* 定义命令结构体 */                                                \
	static cli_command_t _cli_cmd_def_##name = {                        \
		.name = cmd_str,                                            \
		.doc = "Command " cmd_str,                                  \
		.arg_struct = NULL, /* 运行时填充 */                        \
		.arg_struct_size = sizeof(_cli_struct_##name),              \
		.options = _cli_options_##name,                             \
		.option_count =                                             \
			sizeof(_cli_options_##name) / sizeof(cli_option_t), \
		.validator = (int (*)(void *))parse_cb,                     \
	};                                                                  \
                                                                            \
	/* 构造函数：自动注册到全局表（GCC特性） */                         \
	__attribute__((constructor)) static void _cli_register_##name(void) \
	{                                                                   \
		if (g_cli_command_count < CLI_MAX_COMMANDS) {               \
			g_cli_commands[g_cli_command_count++] =             \
				&_cli_cmd_def_##name;                       \
		}                                                           \
	}

#define END_OPTIONS /* 结束标记，实际为空 */

int dispose_init(void);
int dispose_task(char *cmd);

#endif
