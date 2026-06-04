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
 * 输出生命周期钩子
 *
 * 注册钩子后，log_output 在每个"输出事务"前后自动调用：
 *
 *   output_begin()  — 输出开始前调用（清行、解锁Flash、加锁互斥）
 *   output_end()    — 输出结束后调用（恢复提示符、锁定Flash、释放互斥）
 *
 * 钩子只会在数据推入 kfifo 环形缓冲时触发（log_output / log_output_raw），
 * 不会在 log_output_flush 刷出时再次触发。flush 只管转移数据到传输层。
 *
 * 你完全控制钩子函数的内容。如果某场景下不希望执行（如 ISR），
 * 直接在钩子函数内部判断返回即可。
 *
 * coord=1（默认）: 钩子自动触发
 * coord=0:         钩子由你手动管理
 *
 * 所有回调都是可选的（设为 NULL 跳过）。
 * ============================================================ */

struct log_output_hooks {
    void (*output_begin)(void);
    void (*output_end)(void);
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

/** Register output lifecycle hooks (optional).
 *
 *  When hooks are set and coord=1, the library calls:
 *    output_begin() before each output transaction
 *    output_end()   after each output transaction
 *
 *  Hook functions are called unconditionally — if you need
 *  to skip them (e.g. in ISR context), guard them yourself
 *  inside the hook.
 *  Pass NULL to unregister. */
void log_output_set_hooks(const struct log_output_hooks *hooks);

/**
 * Global log-level filter: messages with level > g_log_level are dropped.
 * Default "8" = show everything ("8" compares greater than all kern levels).
 */
extern char g_log_level[3];

/**
 * 输出协调模式。
 *
 * coord=1（默认）: 自定义钩子（hooks）在推入环形缓冲时自动触发。
 *   每个输出事务前调用 output_begin()，后调用 output_end()。
 *   批量模式下，begin/end 自动合并为一次。
 *
 * coord=0: 自定义钩子不会自动触发。
 *   你手动管理 begin/end 调用（常见于外部临界区）。
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
 * Batch mode — suppress output_begin/end across a group of log calls.
 *
 *   log_batch_begin/end()       — suppress hooks only
 *   log_batch_begin_cont(lvl)   — suppress hooks + continuation mode
 *
 * Continuation mode is analogous to Linux KERN_CONT: the level prefix
 * is emitted only on the first message inside the batch; subsequent
 * messages inherit the same level for filtering but omit the prefix.
 *
 * Use for building multi-part log lines:
 *   log_batch_begin_cont(KERN_INFO);
 *   log_output("hello ");          // → [I] hello
 *   log_output("world\n");         // → world\n
 *   log_batch_end();               // → [I] hello world\n
 */
void log_batch_begin(void);
void log_batch_begin_cont(const char *level);
void log_batch_end(void);

/* ============================================================
 * Ring buffer — all output goes through kfifo (hard dependency)
 * ============================================================ */

#include "kfifo.h"

/** Configure the ring buffer size.  All log_output/raw/direct calls
 *  push formatted messages to this ring.  The ONLY function that
 *  drains the ring and calls write_fn is log_output_flush().
 *
 *  Unless you call this function, a default 2048-byte internal buffer
 *  is used (initialised by log_output_init()).  Call this to override
 *  with your own buffer (e.g. 1024 bytes for a busy RTOS system).
 *
 *  Re-initialising the ring discards any unflushed data.
 *  Pass NULL/0 to keep the existing ring unchanged. */
void log_output_set_ring(uint8_t *buf, uint32_t size);

/** Drain the ring buffer: pull all buffered messages and push them
 *  through the registered write_fn.  Returns total bytes flushed. */
int log_output_flush(void);

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
