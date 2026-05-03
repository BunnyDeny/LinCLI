/*
 * LinCLI - Variable export demo for CLI_ENABLE_TESTS.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS && CLI_ENABLE_VAR
#include "cli_var.h"
#include "cli_io.h"

/* ============================================================
 * 示例变量（验证用，生产环境可删除或保留作为 demo）
 * ============================================================ */

static int g_loop_count = 0;
CLI_VAR(g_loop_count, "g_loop_count", INT, "Main loop counter");

static char g_device_name[32] = "lincli-dev";
CLI_VAR(g_device_name, "g_device_name", STRING, "Device name");

static bool g_verbose = false;
CLI_VAR(g_verbose, "g_verbose", BOOL, "Verbose output flag");

static float g_kp = 2.5f;
CLI_VAR_RO(g_kp, "g_kp", FLOAT, "PID Kp parameter (read-only)");

/* ============================================================
 * 自定义类型测试：二维坐标点
 * ============================================================ */

typedef struct {
	int x;
	int y;
} point_t;

static int point_from_str(void *addr, size_t size, const char *str)
{
	point_t *p = addr;
	int n = 0;
	if (sscanf(str, "%d,%d%n", &p->x, &p->y, &n) != 2) {
		pr_err("point format must be x,y (e.g. 10,20)\r\n");
		return -1;
	}
	/* 检查尾部是否还有多余字符（防止 10,20abc 被静默接受） */
	if (str[n] != '\0') {
		pr_err("point format must be x,y (e.g. 10,20)\r\n");
		return -1;
	}
	return 0;
}

static int point_to_str(const void *addr, size_t size, char *buf,
			 size_t buf_size)
{
	const point_t *p = addr;
	snprintf(buf, buf_size, "(%d,%d)", p->x, p->y);
	return 0;
}

/* 注册自定义类型（全局只需一次） */
CLI_VAR_TYPE(point, point_from_str, point_to_str);

/* 注册自定义类型变量 */
static point_t g_origin = { 10, 20 };
CLI_VAR_CUSTOM(g_origin, "g_origin", "point", "Screen origin point");

static point_t g_target = { 100, 200 };
CLI_VAR_CUSTOM_RO(g_target, "g_target", "point",
		    "Target point (read-only)");

/* ============================================================
 * 自定义类型测试：PID 参数（多字段结构体）
 * ============================================================ */

typedef struct {
	int kp;
	int ki;
	int kd;
} pid_params_t;

static int pid_from_str(void *addr, size_t size, const char *str)
{
	pid_params_t *pid = addr;
	int n = 0;
	if (sscanf(str, "%d,%d,%d%n", &pid->kp, &pid->ki, &pid->kd, &n) != 3) {
		pr_err("pid format must be kp,ki,kd (e.g. 2000,100,50)\r\n");
		return -1;
	}
	/* 检查尾部是否还有多余字符（防止 1000,2000,300a0 被静默接受） */
	if (str[n] != '\0') {
		pr_err("pid format must be kp,ki,kd (e.g. 2000,100,50)\r\n");
		return -1;
	}
	return 0;
}

static int pid_to_str(const void *addr, size_t size, char *buf,
		      size_t buf_size)
{
	const pid_params_t *pid = addr;
	snprintf(buf, buf_size, "kp=%d,ki=%d,kd=%d", pid->kp, pid->ki,
		 pid->kd);
	return 0;
}

CLI_VAR_TYPE(pid, pid_from_str, pid_to_str);

static pid_params_t g_pid = { 2000, 100, 50 };
CLI_VAR_CUSTOM(g_pid, "g_pid", "pid", "Motor PID parameters");

#endif /* CLI_ENABLE_TESTS && CLI_ENABLE_VAR */
