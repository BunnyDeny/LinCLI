
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
	void *arg_struct; // 参数结构体指针（运行时填充）
	size_t arg_struct_size; // 结构体大小
	const cli_option_t *options; // 选项数组
	size_t option_count; // 选项数量
	int (*validator)(void *); // 自定义验证函数
	void *arg_buf; // 命令参数解析缓冲区指针
	size_t arg_buf_size; // 缓冲区大小
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
int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv,
		   void *arg_struct);
void cli_print_help(const cli_command_t *cmd);

/* ============================================================
 * 宏工具：计算偏移量
 * ============================================================ */

#define CLI_OFFSETOF(type, field) offsetof(type, field)

// 安全地获取数组长度的偏移量（假设紧跟在数组指针后面的是_count字段）
#define CLI_ARRAY_COUNT_OFFSET(type, field) \
	(offsetof(type, field) + sizeof(((type *)0)->field))

/* ============================================================
 * OPTION 宏定义（统一入口）
 * ============================================================
 *
 * 为了彻底消除预处理器歧义，所有变体通过参数数量唯一确定：
 *
 *   6 参数：基础类型（BOOL / STRING / INT / DOUBLE / CALLBACK）
 *           格式：OPTION(short, long, TYPE, help, struct, field)
 *
 *   7 参数：基础类型 + required
 *           格式：OPTION(short, long, TYPE, help, struct, field, required)
 *
 *   8 参数：数组类型（INT_ARRAY）+ max_args + depends
 *           格式：OPTION(short, long, INT_ARRAY, help, struct, field,
 *                       max_args, depends)
 *           若不需要依赖，请将 depends 填为 NULL。
 *
 *   9 参数：数组类型 + max_args + depends + required
 *           格式：OPTION(short, long, INT_ARRAY, help, struct, field,
 *                       max_args, depends, required)
 */

#define _GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME

#define OPTION(...)                                                      \
	_GET_MACRO(__VA_ARGS__, OPTION_9, OPTION_8, OPTION_7, OPTION_6)(__VA_ARGS__)

/* 基础类型：6 参数 */
#define OPTION_6(_sopt, _lopt, _type, _help, _stype, _field)             \
	{ .short_opt = _sopt,                                              \
	  .long_opt = _lopt,                                               \
	  .type = CLI_TYPE_##_type,                                        \
	  .help = _help,                                                   \
	  .offset = CLI_OFFSETOF(_stype, _field),                          \
	  .offset_count = _OPTION_COUNT_##_type(_stype, _field),           \
	  .max_args = 1,                                                   \
	  .required = false,                                               \
	  .depends = NULL,                                                 \
	  .conflicts = NULL }

/* 基础类型：7 参数 (+ required) */
#define OPTION_7(_sopt, _lopt, _type, _help, _stype, _field, _req)       \
	{ .short_opt = _sopt,                                              \
	  .long_opt = _lopt,                                               \
	  .type = CLI_TYPE_##_type,                                        \
	  .help = _help,                                                   \
	  .offset = CLI_OFFSETOF(_stype, _field),                          \
	  .offset_count = _OPTION_COUNT_##_type(_stype, _field),           \
	  .max_args = 1,                                                   \
	  .required = _req,                                                \
	  .depends = NULL,                                                 \
	  .conflicts = NULL }

/* 数组类型：8 参数 (+ max_args + depends) */
#define OPTION_8(_sopt, _lopt, _type, _help, _stype, _field, _max, _dep) \
	{ .short_opt = _sopt,                                              \
	  .long_opt = _lopt,                                               \
	  .type = CLI_TYPE_##_type,                                        \
	  .help = _help,                                                   \
	  .offset = CLI_OFFSETOF(_stype, _field),                          \
	  .offset_count = _OPTION_COUNT_##_type(_stype, _field),           \
	  .max_args = _max,                                                \
	  .required = false,                                               \
	  .depends = _dep,                                                 \
	  .conflicts = NULL }

/* 数组类型：9 参数 (+ max_args + depends + required) */
#define OPTION_9(_sopt, _lopt, _type, _help, _stype, _field, _max,      \
		 _dep, _req)                                                   \
	{ .short_opt = _sopt,                                              \
	  .long_opt = _lopt,                                               \
	  .type = CLI_TYPE_##_type,                                        \
	  .help = _help,                                                   \
	  .offset = CLI_OFFSETOF(_stype, _field),                          \
	  .offset_count = _OPTION_COUNT_##_type(_stype, _field),           \
	  .max_args = _max,                                                \
	  .required = _req,                                                \
	  .depends = _dep,                                                 \
	  .conflicts = NULL }

/* 各类型的 offset_count 计算 */
#define _OPTION_COUNT_BOOL(_stype, _field) 0
#define _OPTION_COUNT_STRING(_stype, _field) 0
#define _OPTION_COUNT_INT(_stype, _field) 0
#define _OPTION_COUNT_DOUBLE(_stype, _field) 0
#define _OPTION_COUNT_CALLBACK(_stype, _field) 0
#define _OPTION_COUNT_INT_ARRAY(_stype, _field)                          \
	CLI_OFFSETOF(_stype, _field##_count)

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
 *
 * 警告：arg_struct_ptr 必须传入类型明确的结构体指针表达式，
 * 例如 (struct xxx *)0 或 &((struct xxx){0})，否则 typeof 无法
 * 正确推导类型。严禁传入 NULL，否则 sizeof 会退化为 1，
 * 导致运行时缓冲区分配错误。
 */

#define _EXPORT_CLI_COMMAND_SYMBOL(_obj, _cmd_str, _doc_str, _size, _opts, \
				   _opts_cnt, _vld, _buf, _buf_size, _section) \
	static cli_command_t _cli_cmd_def_##_obj __attribute__((           \
		used, section(_section), aligned(sizeof(long)))) = {       \
		.name = _cmd_str,                                          \
		.doc = _doc_str,                                           \
		.arg_struct = NULL,                                        \
		.arg_struct_size = _size,                                  \
		.options = _opts,                                          \
		.option_count = _opts_cnt,                                 \
		.validator = _vld,                                         \
		.arg_buf = _buf,                                           \
		.arg_buf_size = _buf_size,                                 \
	}

/* 全局共享命令参数缓冲区，所有使用 CLI_COMMAND 注册的命令串行复用该内存。
 * 由于 CLI 解析与执行在单一线程中串行完成，不会产生并发冲突。
 * 若用户结构体超过此大小，请使用 CLI_COMMAND_WITH_BUF 宏自行指定缓冲区。
 */
#ifndef CLI_CMD_BUF_SIZE
#define CLI_CMD_BUF_SIZE 512
#endif
extern char g_cli_cmd_buf[CLI_CMD_BUF_SIZE];

#define CLI_COMMAND(name, cmd_str, doc_str, parse_cb, arg_struct_ptr, ...) \
	/* 前向声明参数结构体类型 */ \
	typedef typeof(*arg_struct_ptr) _cli_struct_##name; \
	                                                                   \
	/* 定义选项数组（放在静态区） */                                   \
	static const cli_option_t _cli_options_##name[] = { __VA_ARGS__ }; \
	                                                                   \
	/* 通过链接脚本段收集注册，使用全局共享缓冲区 */                   \
	_EXPORT_CLI_COMMAND_SYMBOL(                                        \
		name, cmd_str, doc_str, sizeof(_cli_struct_##name),        \
		_cli_options_##name,                                       \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),      \
		(int (*)(void *))parse_cb, g_cli_cmd_buf,                  \
		CLI_CMD_BUF_SIZE, ".cli_commands")

#define CLI_COMMAND_WITH_BUF(name, cmd_str, doc_str, parse_cb, arg_struct_ptr, buf, buf_size, ...) \
	/* 前向声明参数结构体类型 */                                       \
	typedef typeof(*arg_struct_ptr) _cli_struct_##name;                \
	                                                                   \
	/* 定义选项数组（放在静态区） */                                   \
	static const cli_option_t _cli_options_##name[] = { __VA_ARGS__ }; \
	                                                                   \
	/* 通过链接脚本段收集注册，使用用户指定的缓冲区 */               \
	_EXPORT_CLI_COMMAND_SYMBOL(                                        \
		name, cmd_str, doc_str, sizeof(_cli_struct_##name),        \
		_cli_options_##name,                                       \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),      \
		(int (*)(void *))parse_cb, buf, buf_size, ".cli_commands")

#define END_OPTIONS /* 结束标记，实际为空 */

int dispose_init(void);
int dispose_task(char *cmd);

#endif
