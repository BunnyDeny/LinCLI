#include "cli_io.h"
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

char log_level[3] = "8";

struct cli_io _cli_io = {
	.in_push_ref = 0,
	.in_pop_ref = 0,
	.out_push_ref = 0,
	.out_pop_ref = 0,
};

void cli_io_init(void)
{
	vectorInit(&_cli_io.in, (_u8 *)_cli_io.in_buf, CLI_IO_SIZE);
	vectorInit(&_cli_io.out, (_u8 *)_cli_io.out_buf, CLI_IO_SIZE);
	_cli_io.in_push_ref = 1;
	_cli_io.in_pop_ref = 1;
	_cli_io.out_push_ref = 1;
	_cli_io.out_pop_ref = 1;
}

//移植的时候实现
__attribute__((weak)) void cli_putc(char ch)
{
	write(STDOUT_FILENO, &ch, 1);
}

int cli_out_sync(void)
{
	while (cli_get_out_size() > 0) {
		char ch;
		int status = cli_out_pop((_u8 *)&ch, 1);
		if (status == 0) {
			cli_putc(ch);
		} else {
			return status;
		}
	}
	return 0;
}

__attribute__((weak)) const char *pre_EMERG_gen(void)
{
	return COLOR_BOLD COLOR_RED "[EMERG] ";
}

__attribute__((weak)) const char *pre_ALERT_gen(void)
{
	return COLOR_MAGENTA "[ALERT] ";
}

__attribute__((weak)) const char *pre_CRIT_gen(void)
{
	return COLOR_RAINBOW_2 "[CRIT] ";
}

__attribute__((weak)) const char *pre_ERR_gen(void)
{
	return COLOR_RED "[ERR] ";
}

__attribute__((weak)) const char *pre_WARNING_gen(void)
{
	return COLOR_YELLOW "[WARNING] ";
}

__attribute__((weak)) const char *pre_NOTICE_gen(void)
{
	return COLOR_BOLD COLOR_GREEN "[NOTICE] ";
}

__attribute__((weak)) const char *pre_INFO_gen(void)
{
	return COLOR_BLUE "[INFO] " COLOR_NONE;
}

__attribute__((weak)) const char *pre_DEBUG_gen(void)
{
	return COLOR_RAINBOW_4 "[DEBUG] ";
}

__attribute__((weak)) const char *pre_DEFAULT_gen(void)
{
	return "";
}

__attribute__((weak)) const char *pre_CONT_gen(void)
{
	return COLOR_CYAN "[CONT] ";
}

static const char *prefiex_gen(const char *level)
{
	char lv = level[0];
	const char *prefiex;
	switch (lv) {
	case '0':
		prefiex = pre_EMERG_gen();
		break;
	case '1':
		prefiex = pre_ALERT_gen();
		break;
	case '2':
		prefiex = pre_CRIT_gen();
		break;
	case '3':
		prefiex = pre_ERR_gen();
		break;
	case '4':
		prefiex = pre_WARNING_gen();
		break;
	case '5':
		prefiex = pre_NOTICE_gen();
		break;
	case '6':
		prefiex = pre_INFO_gen();
		break;
	case '7':
		prefiex = pre_DEBUG_gen();
		break;
	case 'c':
		prefiex = pre_CONT_gen();
		break;
	default:
		prefiex = pre_DEFAULT_gen();
		break;
	}
	return prefiex;
}

static inline int is_kern_level(char c)
{
	return (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' ||
		c == '5' || c == '6' || c == '7' || c == 'c');
}

static char buffer[CLI_PRINTK_BUF_SIZE];

/**
 * @brief Core CLI log output function
 *
 * This function provides level-based log printing with support for
 * log level filtering and prefix generation.
 *
 * @param fmt Format string, compatible with printf
 * @param ... Variable arguments
 * @return Returns the number of characters printed on success, negative error code on failure
 *
 * @note Unless using KERN_DEFAULT (level "8") level log printing,
 *       it is recommended to use the simpler macros: pr_emerg, pr_alert, pr_crit, pr_err,
 *       pr_warn, pr_notice, pr_info, pr_debug, pr_cont
 *
 * @note Log level filtering rules (set via global variable log_level):
 *       - log_level = "0": Highest filtering level, most strict, only prints EMERG (level 0)
 *       - log_level = "1"~"7": Prints logs at the corresponding level and above
 *       - log_level = "8": Lowest filtering level, most permissive, prints all logs (including prefix-less strings)
 *       - Example: When log_level = "4", only logs at level 0~4 are printed, levels 5~7 are filtered
 *
 * @note Kernel level prefix description:
 *       - "0"~"7" are standard kernel levels, the function automatically adds [EMERG]~[DEBUG] prefixes
 *       - "8" is KERN_DEFAULT, indicates no level prefix, used for printing ordinary strings
 *       - "c" is KERN_CONT, same as "8" at the lowest level
 *       - When log_level is set to "0"~"7", both "c" and "8" will be filtered out
 *
 * @note Prefix and color customization: Functions like pre_EMERG_gen, pre_ALERT_gen are defined
 *       with weak attribute, developers can redefine these functions to customize log prefix and color
 */
int cli_printk(const char *fmt, ...)
{
	int status;
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	char pre[2] = { buffer[0], '\0' };
	if (pre[0] != '8' && pre[0] >= '0' && pre[0] <= '7') {
		if (pre[0] > log_level[0]) {
			return 0;
		}
	}
	if ((!is_kern_level(pre[0]) || !strcmp(pre, KERN_CONT)) &&
	    strcmp("8", log_level)) {
		return 0;
	}
	const char *_pre = prefiex_gen(pre);
	if (is_kern_level(buffer[0])) {
		memmove(buffer, buffer + 1, CLI_PRINTK_BUF_SIZE - 1);
	}
	int pre_len = strlen(_pre);
	if (len > 0 && pre_len >= 0) {
		memmove(buffer + pre_len, buffer, len + 1);
		memcpy(buffer, _pre, pre_len);
		strcat(buffer, COLOR_NONE);
		status = cli_out_push((_u8 *)buffer,
				      pre_len + len + strlen(COLOR_NONE));
		if (status)
			return status;
		if (cli_out_sync())
			return -2;
	}
	return len;
}

void cli_printk_test(void)
{
	cli_printk("########################################\n");
	pr_emerg("这是EMERG级别 - 最严重的紧急情况\n");
	pr_alert("这是ALERT级别 - 需要立即处理\n");
	pr_crit("这是CRIT级别 - 严重故障\n");
	pr_err("这是ERR级别 - 错误\n");
	pr_warn("这是WARNING级别 - 警告\n");
	pr_notice("这是NOTICE级别 - 正常但重要\n");
	pr_info("这是INFO级别 - 一般信息\n");
	pr_debug("这是DEBUG级别 - 调试信息\n");
	cli_printk("这是DEFAULT级别 - 无前缀\n");
	pr_cont("这是CONT级别 - 继续输出\n");
	cli_printk("########################################\n");
}
