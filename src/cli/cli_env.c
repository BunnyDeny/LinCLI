/*
 * LinCLI - Environment variable system implementation.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "cli_config.h"
#include "cli_env.h"
#include "cli_errno.h"
#include "cli_io.h"
#include "cli_mpool.h"
#include "cmd_dispose.h"
#include "init_d.h"
#include "cli_atoi.h"
#include <stdlib.h>
#include <string.h>

#if CLI_ENABLE_ENV

/* ============================================================
 *  系统内置环境变量（不依赖 CLI_ENABLE_DEMO_COMMANDS）
 * ============================================================ */

#define _CLI_VER_STR(_major, _minor, _patch) \
	#_major "." #_minor "." #_patch
#define CLI_VER_STR(_major, _minor, _patch) \
	_CLI_VER_STR(_major, _minor, _patch)

CLI_ENV(VER, "LinCLI v" CLI_VER_STR(CLI_VERSION_MAJOR, CLI_VERSION_MINOR,
					  CLI_VERSION_PATCH));
CLI_ENV(BUILD, __DATE__ " " __TIME__);
CLI_ENV(echo, "_echo --msg");

/* ============================================================
 *  echo 命令：系统内置打印命令
 * ============================================================ */

struct echo_args {
	char *msg;
};

static int _echo_handler(void *_args)
{
	struct echo_args *args = _args;
	cli_printk("%s\r\n", args->msg ? args->msg : "");
	return 0;
}

CLI_COMMAND(_echo, "_echo", "Print msg",
	    USAGE("_echo --msg <str>"), _echo_handler,
	    (struct echo_args *)0,
	    OPTION(0, "msg", STRING, "", struct echo_args, msg, 0, NULL, NULL,
		   false),
	    END_OPTIONS);

/* ============================================================
 *  纯整数名字检测与注册过滤
 * ============================================================ */

static bool is_pure_integer_name(const char *name)
{
	if (!name || !name[0])
		return true;
	for (int i = 0; name[i]; i++) {
		if (name[i] < '0' || name[i] > '9')
			return false;
	}
	return true;
}

static bool cli_env_should_ignore(const cli_env_t *env)
{
	if (!env || !env->name)
		return true;
	return is_pure_integer_name(env->name);
}

/* ============================================================
 *  ID 初始化（由 init_d 在启动时分配）
 * ============================================================ */

static void cli_env_id_init(void *arg)
{
	(void)arg;
	int id = 0;
	cli_env_t *env;
	_FOR_EACH_CLI_ENV(_cli_envs_start, _cli_envs_end, env)
	{
		if (cli_env_should_ignore(env))
			continue;
		env->id = id++;
	}
}
_EXPORT_INIT_SYMBOL(cli_env_id_init, 20, NULL, cli_env_id_init);

/* ============================================================
 *  查找与取值
 * ============================================================ */

cli_env_t *cli_env_find(const char *name)
{
	cli_env_t *env;
	_FOR_EACH_CLI_ENV(_cli_envs_start, _cli_envs_end, env)
	{
		if (env && env->name && strcmp(env->name, name) == 0)
			return env;
	}
	return NULL;
}

cli_env_t *cli_env_find_by_id(int id)
{
	cli_env_t *env;
	_FOR_EACH_CLI_ENV(_cli_envs_start, _cli_envs_end, env)
	{
		if (env && env->id == id)
			return env;
	}
	return NULL;
}

const char *cli_env_get_value(const cli_env_t *env)
{
	if (!env)
		return NULL;
	if (env->dyn_value)
		return env->dyn_value;
	return env->value;
}

/* ============================================================
 *  设置环境变量值
 * ============================================================ */

static void strip_outer_quotes(const char **src_out, size_t *len_out)
{
	const char *src = *src_out;
	size_t len = *len_out;
	if (len >= 2 && ((src[0] == '\'' && src[len - 1] == '\'') ||
			 (src[0] == '"' && src[len - 1] == '"'))) {
		*src_out = src + 1;
		*len_out = len - 2;
	}
}

int cli_env_set(cli_env_t *env, const char *new_value)
{
	if (!env || !new_value)
		return CLI_ERR_NULL;

	char *buf = cli_mpool_alloc();
	if (!buf) {
		pr_err("OOM\r\n");
		return -1;
	}

	const char *src = new_value;
	size_t len = strlen(src);
	strip_outer_quotes(&src, &len);
	if (len >= CLI_MPOOL_SIZE)
		len = CLI_MPOOL_SIZE - 1;
	memcpy(buf, src, len);
	buf[len] = '\0';

	if (env->dyn_value)
		cli_mpool_free(env->dyn_value);
	env->dyn_value = buf;
	return 0;
}

/* ============================================================
 *  字符串替换辅助函数
 * ============================================================ */

static bool is_env_name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_';
}

static int extract_env_name(const char *src, int pos, char *name_buf,
			    size_t name_buf_size)
{
	int i = pos;
	size_t j = 0;
	while (src[i] && is_env_name_char(src[i]) && j < name_buf_size - 1) {
		name_buf[j] = src[i];
		j++;
		i++;
	}
	name_buf[j] = '\0';
	return i - pos;
}

static int append_to_buf(char *dst, size_t dst_size, int dst_pos,
			 const char *src, int len)
{
	int i = 0;
	while (i < len && dst_pos < (int)dst_size - 1) {
		dst[dst_pos] = src[i];
		dst_pos++;
		i++;
	}
	return dst_pos;
}

static int append_value(char *dst, size_t dst_size, int dst_pos,
			const char *value)
{
	return append_to_buf(dst, dst_size, dst_pos, value,
			     (int)strlen(value));
}

static const char *lookup_env_value(const char *name_buf)
{
	if (is_pure_integer_name(name_buf)) {
		int id;
		cli_atoi(name_buf, &id, NULL);
		cli_env_t *env = cli_env_find_by_id(id);
		if (env)
			return cli_env_get_value(env);
	} else {
		cli_env_t *env = cli_env_find(name_buf);
		if (env)
			return cli_env_get_value(env);
	}
	return NULL;
}

static int substitute_one(const char *input, int *in_pos, char *out,
			  size_t out_size, int *out_pos,
			  char *name_buf, size_t name_buf_size)
{
	(*in_pos)++;
	int name_len = extract_env_name(input, *in_pos, name_buf,
					name_buf_size);
	if (name_len == 0) {
		*out_pos = append_to_buf(out, out_size, *out_pos, "$", 1);
		return CLI_OK;
	}
	*in_pos += name_len;

	const char *value = lookup_env_value(name_buf);
	if (value) {
		*out_pos = append_value(out, out_size, *out_pos, value);
	} else {
		*out_pos = append_to_buf(out, out_size, *out_pos, "$", 1);
		*out_pos = append_to_buf(out, out_size, *out_pos, name_buf,
					 name_len);
	}
	return CLI_OK;
}

static int do_substitution(const char *input, char *out, size_t out_size)
{
	int in_pos = 0;
	int out_pos = 0;
	char name_buf[32];

	while (input[in_pos] && out_pos < (int)out_size - 1) {
		if (input[in_pos] == '$' && is_env_name_char(input[in_pos + 1])) {
			substitute_one(input, &in_pos, out, out_size, &out_pos,
				     name_buf, sizeof(name_buf));
		} else {
			out[out_pos] = input[in_pos];
			out_pos++;
			in_pos++;
		}
	}
	out[out_pos] = '\0';
	return input[in_pos] == '\0' ? CLI_OK : CLI_ERR_BUF_INSUFF;
}

/* ============================================================
 *  公共接口：环境变量替换
 * ============================================================ */

int cli_env_replace(const char *input, char *out, size_t out_size)
{
	if (!input || !out || out_size == 0)
		return CLI_ERR_NULL;
	out[0] = '\0';
	return do_substitution(input, out, out_size);
}

/* ============================================================
 *  env 命令实现
 * ============================================================ */

static void cli_env_list_all(void)
{
	cli_env_t *env;

	_FOR_EACH_CLI_ENV(_cli_envs_start, _cli_envs_end, env)
	{
		if (!env || !env->name || env->id < 0)
			continue;
		all_printk("%d %s=%s\r\n", env->id, env->name,
			   cli_env_get_value(env));
	}
}

static int cli_env_handle_read(const char *name)
{
	cli_env_t *env = cli_env_find(name);
	if (!env) {
		pr_err("unknown: %s\r\n", name);
		return -1;
	}
	all_printk("%s = %s\r\n", env->name, cli_env_get_value(env));
	return 0;
}

static bool cli_env_parse_set_str(const char *set_str, char *name_buf,
				  size_t name_buf_size, const char **value_out)
{
	const char *eq = strchr(set_str, '=');
	if (!eq) {
		pr_err("fmt: name=value\r\n");
		return false;
	}

	size_t name_len = eq - set_str;
	if (name_len >= name_buf_size) {
		pr_err("name too long\r\n");
		return false;
	}

	memcpy(name_buf, set_str, name_len);
	name_buf[name_len] = '\0';
	*value_out = eq + 1;
	return true;
}

static int cli_env_handle_set(const char *set_str)
{
	char name_buf[32];
	const char *value;

	if (!cli_env_parse_set_str(set_str, name_buf, sizeof(name_buf),
				   &value))
		return -1;

	if (is_pure_integer_name(name_buf)) {
		pr_err("name not number\r\n");
		return -1;
	}

	cli_env_t *env = cli_env_find(name_buf);
	if (!env) {
		pr_err("unknown env: %s\r\n", name_buf);
		return -1;
	}

	return cli_env_set(env, value);
}

struct env_args {
	bool list;
	char *set;
	char *read;
};

static int env_handler(void *_args)
{
	struct env_args *args = _args;

	if (args->list)
		return cli_env_list_all(), 0;
	if (args->set)
		return cli_env_handle_set(args->set);
	if (args->read)
		return cli_env_handle_read(args->read);

	pr_err("env -l | env -s <n=v> | env -r <n>\r\n");
	return -1;
}

CLI_COMMAND(env, "env", "Environment",
	    USAGE("env -l", "env -s <n=v>",
		  "env -r <n>"),
	    env_handler, (struct env_args *)0,
	    OPTION('l', "list", BOOL, "",
		   struct env_args, list, 0, NULL, "set read", false),
	    OPTION('s', "set", STRING,
		   "", struct env_args,
		   set, 0, NULL, "list read", false),
	    OPTION('r', "read", STRING, "",
		   struct env_args, read, 0, NULL, "list set", false),
	    END_OPTIONS);

/* ============================================================
 *  运行时填充 env 命令 --read / --set 选项的候选列表
 * ============================================================ */

#define MAX_CLI_ENV_CANDS 64

static char *env_names[MAX_CLI_ENV_CANDS + 1];
static int env_name_count;

static void cli_env_collect_candidates(void)
{
	cli_env_t *env;
	env_name_count = 0;
	_FOR_EACH_CLI_ENV(_cli_envs_start, _cli_envs_end, env)
	{
		if (!env || !env->name || env->id < 0)
			continue;
		if (env_name_count < MAX_CLI_ENV_CANDS)
			env_names[env_name_count++] = (char *)env->name;
	}
}

static void cli_env_attach_candidates(const cli_command_t *cmd)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		if (!opt->long_opt)
			continue;
		if (strcmp(opt->long_opt, "read") == 0 ||
		    strcmp(opt->long_opt, "set") == 0) {
			opt->candidate_argc = env_name_count;
			opt->candidate_argv = env_names;
		}
	}
}

static void cli_env_candidate_init(void *arg)
{
	(void)arg;
	cli_env_collect_candidates();

	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(cmd)
	{
		if (!cmd || !cmd->name || strcmp(cmd->name, "env") != 0)
			continue;
		cli_env_attach_candidates(cmd);
	}
}
_EXPORT_INIT_SYMBOL(cli_env_candidate_init, 25, NULL,
		    cli_env_candidate_init);

#else /* !CLI_ENABLE_ENV */

/* ============================================================
 *  存根：环境变量替换直通（不做任何替换）
 * ============================================================ */

int cli_env_replace(const char *input, char *out, size_t out_size)
{
	if (!input || !out || out_size == 0)
		return CLI_ERR_NULL;
	size_t n = strlen(input);
	if (n >= out_size)
		n = out_size - 1;
	memcpy(out, input, n);
	out[n] = '\0';
	return CLI_OK;
}

#endif /* CLI_ENABLE_ENV */
