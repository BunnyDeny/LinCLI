/*
 * log_output — standalone embedded-friendly logging library.
 *
 * Zero external dependencies.  Designed to be extracted as its own
 * repository, or used as a single .c/.h drop-in.
 *
 * Integration:
 *   1. call log_output_set_write_fn()  – mandatory, sets the output transport
 *   2. call log_output_set_term_ops()  – optional, enables interactive
 *      terminal save/restore around each log line
 *   3. call log_output_init()          – optional, resets internal state
 *   4. use log_output() / log_*() macros to emit messages
 */

#ifndef LOG_OUTPUT_H
#define LOG_OUTPUT_H

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Configuration (override via -D or pre-include)
 * ============================================================ */

#ifndef LOG_BUF_SIZE
#define LOG_BUF_SIZE              128
#endif

/* ============================================================
 * ANSI terminal colour codes
 * ============================================================ */

#define LOG_COLOR_NONE            "\033[0m"
#define LOG_COLOR_RED             "\033[31m"
#define LOG_COLOR_GREEN           "\033[32m"
#define LOG_COLOR_YELLOW          "\033[33m"
#define LOG_COLOR_BLUE            "\033[34m"
#define LOG_COLOR_MAGENTA         "\033[35m"
#define LOG_COLOR_CYAN            "\033[36m"
#define LOG_COLOR_WHITE           "\033[37m"
#define LOG_COLOR_DIM             "\033[2m"
#define LOG_COLOR_BOLD            "\033[1m"
#define LOG_COLOR_RAINBOW_1       "\033[38;5;196m"  /* red        */
#define LOG_COLOR_RAINBOW_2       "\033[38;5;208m"  /* orange-red */
#define LOG_COLOR_RAINBOW_3       "\033[38;5;214m"  /* orange     */
#define LOG_COLOR_RAINBOW_4       "\033[38;5;226m"  /* yellow     */
#define LOG_COLOR_RAINBOW_5       "\033[38;5;046m"  /* green      */
#define LOG_COLOR_RAINBOW_6       "\033[38;5;051m"  /* cyan       */
#define LOG_COLOR_RAINBOW_7       "\033[38;5;033m"  /* blue       */
#define LOG_COLOR_RAINBOW_8       "\033[38;5;129m"  /* purple     */
#define LOG_COLOR_RAINBOW_9       "\033[38;5;201m"  /* magenta    */

/* ============================================================
 * Kernel-style log level markers
 *
 * Prefix the format string to set a level:
 *   log_output(KERN_ERR "something bad: %d\n", code);
 * ============================================================ */

#define LOG_KERN_EMERG            "0"
#define LOG_KERN_ALERT            "1"
#define LOG_KERN_CRIT             "2"
#define LOG_KERN_ERR              "3"
#define LOG_KERN_WARNING          "4"
#define LOG_KERN_NOTICE           "5"
#define LOG_KERN_INFO             "6"
#define LOG_KERN_DEBUG            "7"
#define LOG_KERN_DEFAULT          ""

/* ============================================================
 * Terminal lifecycle hooks
 *
 * When terminal ops are registered, log_output calls:
 *   save()              – clear current interactive line
 *   write_fn(prefix..)  – emit the log
 *   write_fn(content..)
 *   write_fn(reset..)
 *   restore()           – redraw prompt / candidate list
 *
 * save/restore are ONLY called when is_interactive() returns true.
 * in_irq() lets the adapter track first-ISR-print-before-task-print.
 * All callbacks are optional (set to NULL to skip that step).
 * ============================================================ */

struct log_term_ops {
    void (*save)(void);
    void (*restore)(void);
    int  (*in_irq)(void);           /* 1 if currently in interrupt */
    int  (*is_interactive)(void);   /* 1 if terminal is interactive */
};

/* ============================================================
 * Output write callback
 *
 * Called with formatted output (prefix + content + color reset).
 * Must either transmit or buffer; return < 0 on error.
 * ============================================================ */
typedef int (*log_write_fn)(const char *buf, int len);

/* ============================================================
 * Public API
 * ============================================================ */

/** Initialise internal state (reset batch counter, etc.). */
void log_output_init(void);

/** Register the output transport (mandatory before first log). */
void log_output_set_write_fn(log_write_fn fn);

/** Register terminal lifecycle hooks (optional). */
void log_output_set_term_ops(const struct log_term_ops *ops);

/**
 * Global log-level filter: messages with level > g_log_level are dropped.
 * Default "8" = show everything ("8" compares greater than all kern levels).
 */
extern char g_log_level[3];

/**
 * Terminal coordination mode.
 *
 * When ENABLED (default): log_output_v() internally calls term_ops->save()
 * before writing and term_ops->restore() after writing.  This is the
 * standalone / Cortex-M mode (where cli_enter_critical maps to interrupt
 * masking, which is properly nested).
 *
 * When DISABLED: log_output_v() ONLY formats and writes.  The wrapper
 * (e.g. cli_printk) is responsible for calling save/restore around the
 * critical section.  This is the PC mode (where cli_enter_critical maps to
 * a non-reentrant spinlock, and redrawing inside the CS would deadlock).
 */
void log_output_set_term_coord(int enable);

/*
 * Core logging — reads level from first char of fmt.
 *
 *   log_output(KERN_ERR "error %d\n", code);
 *   log_output("no-prefix plain text\n");
 *   log_output(KERN_INFO "info\n");
 */
int log_output(const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));
int log_output_v(const char *fmt, va_list args);

/*
 * Raw logging — same as log_output but bypasses level filtering.
 * Use for probe / debug output that must always appear.
 */
int log_output_raw(const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));
int log_output_raw_v(const char *fmt, va_list args);

/*
 * Direct logging — no level filter, no terminal save/restore.
 * Use for prompt drawing, progress bars, etc.
 */
int log_output_direct(const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));
int log_output_direct_v(const char *fmt, va_list args);

/*
 * Batch mode — suppress terminal save/restore across a group of logs.
 * Useful for commands that emit many log lines (save once, restore once).
 */
void log_batch_begin(void);
void log_batch_end(void);

/* ============================================================
 * Ring buffer mode (requires kfifo.h / LOG_OUTPUT_HAVE_KFIFO)
 * ============================================================ */

#ifdef LOG_OUTPUT_HAVE_KFIFO
#include "kfifo.h"

/** Enable ring buffer mode: log_output pushes to this kfifo
 *  instead of calling the write_fn directly.  Caller must
 *  periodically drain with log_output_flush().  Pass NULL/NULL/0
 *  to disable ring mode and revert to direct mode. */
void log_output_set_ring(kfifo_t *ring, uint8_t *buf, uint32_t size);

/** Drain the ring buffer: pull all buffered messages and push them
 *  through the registered write_fn.  Returns total bytes flushed. */
int log_output_flush(void);
#endif

/* ============================================================
 * Convenience macros (log_* namespace)
 * ============================================================ */
#define log_emerg(fmt, ...)    log_output(LOG_KERN_EMERG   fmt, ##__VA_ARGS__)
#define log_alert(fmt, ...)    log_output(LOG_KERN_ALERT   fmt, ##__VA_ARGS__)
#define log_crit(fmt, ...)     log_output(LOG_KERN_CRIT    fmt, ##__VA_ARGS__)
#define log_err(fmt, ...)      log_output(LOG_KERN_ERR     fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)     log_output(LOG_KERN_WARNING fmt, ##__VA_ARGS__)
#define log_notice(fmt, ...)   log_output(LOG_KERN_NOTICE  fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)     log_output(LOG_KERN_INFO    fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...)    log_output(LOG_KERN_DEBUG   fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOG_OUTPUT_H */
