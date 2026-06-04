/*
 * LinCLI - IO definitions (includes log_output for terminal logging).
 *
 * This header provides backward-compatible aliases for the original
 * cli_io.h COLOR_*, KERN_*, pr_* macros, pointing them to the
 * standalone log_output library.
 *
 * New code should include log_output.h directly when the log API is
 * sufficient.  Include cli_io.h only when you need the FIFO IO layer
 * (cli_in_push, cli_out_push, etc.).
 */

#ifndef _CLI_IO_H
#define _CLI_IO_H

#include "cli_errno.h"
#include "kfifo.h"
#include "log_output.h"
#include <stdio.h>

#if defined(_u8)
#undef _u8
typedef volatile uint8_t _u8;
#else
typedef volatile uint8_t _u8;
#endif

#if defined(_int)
#undef _int
typedef volatile int _int;
#else
typedef volatile int _int;
#endif

/* ============================================================
 * Configuration (project-local override)
 * ============================================================ */

#ifndef CLI_IO_SIZE
#define CLI_IO_SIZE 128
#endif

/* ============================================================
 * Backward-compatible colour aliases → log_output.h
 * ============================================================ */

#define COLOR_NONE       LOG_COLOR_NONE
#define COLOR_RED        LOG_COLOR_RED
#define COLOR_GREEN      LOG_COLOR_GREEN
#define COLOR_YELLOW     LOG_COLOR_YELLOW
#define COLOR_BLUE       LOG_COLOR_BLUE
#define COLOR_MAGENTA    LOG_COLOR_MAGENTA
#define COLOR_CYAN       LOG_COLOR_CYAN
#define COLOR_WHITE      LOG_COLOR_WHITE
#define COLOR_DIM        LOG_COLOR_DIM
#define COLOR_BOLD       LOG_COLOR_BOLD
#define COLOR_RAINBOW_1  LOG_COLOR_RAINBOW_1
#define COLOR_RAINBOW_2  LOG_COLOR_RAINBOW_2
#define COLOR_RAINBOW_3  LOG_COLOR_RAINBOW_3
#define COLOR_RAINBOW_4  LOG_COLOR_RAINBOW_4
#define COLOR_RAINBOW_5  LOG_COLOR_RAINBOW_5
#define COLOR_RAINBOW_6  LOG_COLOR_RAINBOW_6
#define COLOR_RAINBOW_7  LOG_COLOR_RAINBOW_7
#define COLOR_RAINBOW_8  LOG_COLOR_RAINBOW_8
#define COLOR_RAINBOW_9  LOG_COLOR_RAINBOW_9

/* ============================================================
 * Backward-compatible KERN level aliases → log_output.h
 * ============================================================ */

#define KERN_EMERG       LOG_KERN_EMERG
#define KERN_ALERT       LOG_KERN_ALERT
#define KERN_CRIT        LOG_KERN_CRIT
#define KERN_ERR         LOG_KERN_ERR
#define KERN_WARNING     LOG_KERN_WARNING
#define KERN_NOTICE      LOG_KERN_NOTICE
#define KERN_INFO        LOG_KERN_INFO
#define KERN_DEBUG       LOG_KERN_DEBUG
#define KERN_DEFAULT     LOG_KERN_DEFAULT

/* ============================================================
 * Backward-compatible log level variable
 * ============================================================ */

#define log_level        g_log_level

/*
 * Backward-compatible pr_* macros → cli_printk wrapper
 *
 * cli_printk handles the critical-section + terminal save/restore
 * sequencing.
 */
#define pr_emerg(fmt, ...)     cli_printk(KERN_EMERG   fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)     cli_printk(KERN_ALERT   fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)      cli_printk(KERN_CRIT    fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)       cli_printk(KERN_ERR     fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)      cli_printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...)    cli_printk(KERN_NOTICE  fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)      cli_printk(KERN_INFO    fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)     cli_printk(KERN_DEBUG   fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define pr_devel(fmt, ...)     cli_printk(KERN_DEBUG   fmt, ##__VA_ARGS__)
#endif

/*
 * pr_cont — 续行打印（类似 Linux KERN_CONT）。
 * 仅在 log_batch_begin_cont() 批量内有效。
 * 抑制级别前缀重复，用于构建多段日志行：
 *   cli_printk_batch_begin_cont(KERN_INFO);
 *   pr_info("hello ");
 *   pr_cont("world\n");
 *   cli_printk_batch_end();
 *   // → [I] hello world\n
 */
#define pr_cont(fmt, ...)      cli_printk(fmt, ##__VA_ARGS__)

/* ============================================================
 * Original API — still provided by cli_io.c wrappers
 * ============================================================ */

int cli_printk(const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));
int all_printk(const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));
void cli_printk_batch_begin(void);
void cli_printk_batch_begin_cont(const char *level);
void cli_printk_batch_end(void);

/* ============================================================
 * Original IO FIFO struct & functions (unchanged)
 * ============================================================ */

struct cli_io {
    kfifo_t in;
    _u8 in_ref;
    char in_buf[CLI_IO_SIZE];

    kfifo_t out;
    _u8 out_ref;
    char out_buf[CLI_IO_SIZE];
};

extern struct cli_io _cli_io;

int cli_in_push(_u8 *data, int size);
int cli_out_push(_u8 *data, int size);
int cli_in_pop(_u8 *data, int size);
int cli_out_pop(_u8 *data, int size);
int cli_get_in_size(void);
int cli_get_out_size(void);

void cli_io_init(void);
int cli_out_sync(void);
int cli_in_clear(void);
void set_cli_in_push_lock(void);
void reset_cli_in_push_lock(void);

/*
 * Lockless variants — skip cli_enter_critical/cli_exit_critical.
 * Use ONLY when the critical section is already held by the caller
 * (e.g. inside the log output path).
 */
int cli_out_push_nolock(const uint8_t *data, int size);
int cli_out_pop_nolock(uint8_t *data, int size);
int cli_get_out_size_nolock(void);
int cli_out_sync_nolock(void);

/* ============================================================
 * Terminal adapter initialisation (from cli_term.c)
 * ============================================================ */

void cli_term_init(void);

/*
 * Terminal restore for external callers.
 *
 * cli_term_restore — calls candidate_redraw() / cmd_line_redraw()
 *                  through cli_out_push (acquires spinlock). Must be
 *                  called OUTSIDE the critical section.
 *
 * cli_term_save   — historical alias.  The library now handles
 *                  output_begin automatically via log_output hooks.
 *
 * cli_term_is_interactive — 1 if scheduler is in get-char mode
 *                  and NOT in exception context.
 */
void cli_term_save(void);
void cli_term_restore(void);
int  cli_term_is_interactive(void);

#endif
