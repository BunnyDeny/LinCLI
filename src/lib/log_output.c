/*
 * log_output — standalone embedded-friendly logging library.
 *
 * DESIGN
 * ------
 * log_output is decoupled from any specific transport, terminal UI, or
 * RTOS.  It provides two output modes:
 *
 *   DIRECT  (default) — each log() call formats, filters, and writes
 *   RING    (native)  — each log() formats and pushes to a kfifo ring
 *                        buffer; a background task calls flush() to drain
 *                        the ring and write to the transport.
 *
 * Ring mode is built-in (kfifo is a hard dependency of this library).
 *
 * LIFECYCLE HOOKS
 * ---------------
 * Register hooks via log_output_set_hooks().  When coord=1 (default),
 * the library wraps every "output transaction" with:
 *
 *   output_begin()  — called before output (if !in_atomic)
 *   output_end()    — called after  output (if !in_atomic, !batch)
 *
 * An "output transaction" is:
 *   - a single log_output() call  (non-batch mode)
 *   - a batch region              (begin on first log, end at batch_end)
 *   - a ring buffer flush         (begin before drain, end after)
 *
 * coord=0 disables automatic hooks (user manages them externally).
 * in_atomic() lets hooks be skipped in ISR / spinlock context.
 *
 * Integration:
 *   1. log_output_set_write_fn(fn)  — mandatory (the transport)
 *   2. log_output_set_hooks()       — optional (lifecycle hooks)
 *   3. log_output_init()             — optional (reset state)
 *   4. log_output() / log_*() macros — emit messages
 */

#include "log_output.h"
#include <stdio.h>
#include <string.h>

/*
 * Overridable vsnprintf — default to standard library.
 * #define LOG_VSNPRINTF my_vsnprintf before including log_output.h
 * to use a custom implementation.
 */
#ifndef LOG_VSNPRINTF
#define LOG_VSNPRINTF   vsnprintf
#endif

#include "kfifo.h"

/* ============================================================
 * Static state
 * ============================================================ */

char g_log_level[3] = "8";

static log_write_fn          s_write_fn = NULL;
static struct log_output_hooks s_hooks;
static int                   s_batch;
static const char            *s_batch_level;      /* non-NULL = cont mode */
static bool                  s_batch_first;       /* first msg in cont batch */
static int                   s_term_coord = 1;  /* default: hooks auto-fire */
static bool                  s_hook_begin_called;  /* track begin for batch */

/* Shared format buffer — safe because log_output holds a critical section
 * externally on the LinCLI side, or callers serialise access. */
static char s_buf[LOG_BUF_SIZE];

static kfifo_t s_ring;
static bool    s_ring_enabled;

/* ============================================================
 * Level / prefix helpers
 * ============================================================ */

static const char *s_prefix_table[] = {
    LOG_COLOR_BOLD LOG_COLOR_RED " [E] ",      /* 0  EMERG   */
    LOG_COLOR_MAGENTA          " [A] ",         /* 1  ALERT   */
    LOG_COLOR_RAINBOW_2       " [C] ",         /* 2  CRIT    */
    LOG_COLOR_RED             " [E] ",         /* 3  ERR     */
    LOG_COLOR_YELLOW          " [W] ",         /* 4  WARNING */
    LOG_COLOR_BOLD LOG_COLOR_GREEN " ",        /* 5  NOTICE  */
    LOG_COLOR_BLUE            " [I] " LOG_COLOR_NONE,  /* 6  INFO */
    LOG_COLOR_RAINBOW_4       " [D] ",         /* 7  DEBUG   */
    "",                                        /* 8  default */
};

static inline int is_kern_level(char c)
{
    return (c >= '0' && c <= '7');
}

static const char *prefix_gen(char level)
{
    if (level >= '0' && level <= '7')
        return s_prefix_table[(unsigned)(level - '0')];
    return s_prefix_table[8];
}

static bool log_should_drop(const char *pre)
{
    if (pre[0] != '8' && pre[0] >= '0' && pre[0] <= '7') {
        if (pre[0] > g_log_level[0])
            return true;
    }
    if (!is_kern_level(pre[0]) && strcmp("8", g_log_level))
        return true;
    return false;
}

/* ============================================================
 * Low-level transport calls
 * ============================================================ */

static inline int do_write(const char *s, int len)
{
    if (!s_write_fn || !s || len <= 0) return 0;
    return s_write_fn(s, len);
}

static inline int do_write_str(const char *s)
{
    if (!s) return 0;
    return do_write(s, (int)strlen(s));
}

/* ============================================================
 * Truncate oversized buffer (s_buf, called after vsnprintf)
 * ============================================================ */

static void truncate_if_needed(int *len, size_t buf_size)
{
    if ((size_t)*len < buf_size) return;

    const char *trunc = "...[trunc]\n";
    size_t tlen = strlen(trunc);
    size_t pos = buf_size > tlen ? buf_size - tlen - 1 : 0;
    memcpy(&s_buf[pos], trunc, tlen + 1);
    *len = (int)(buf_size - 1);
}

/* ============================================================
 * Lifecycle: output_begin
 *
 * Called once per output transaction (one log call, or one batch,
 * or one flush).  Skipped if atomic context or coord=0.
 * ============================================================ */

static inline void lifecycle_begin(void)
{
    if (!s_term_coord) return;
    int atomic = s_hooks.in_atomic ? s_hooks.in_atomic() : 0;
    if (atomic) return;

    if (s_hook_begin_called)
        return;  /* already begun (batch: same transaction) */

    if (s_hooks.output_begin)
        s_hooks.output_begin();
    s_hook_begin_called = true;
}

/* ============================================================
 * Lifecycle: output_end
 *
 * Called once per output transaction (if begin was called).
 * In batch mode, deferred until batch_end.
 * ============================================================ */

static inline void lifecycle_end(void)
{
    if (!s_term_coord) return;
    if (!s_hook_begin_called)
        return;

    if (s_batch)
        return;  /* batch in progress — end at batch_end */

    if (s_hooks.output_end)
        s_hooks.output_end();
    s_hook_begin_called = false;
}

/* ============================================================
 * Format + filter + emit (direct path)
 * ============================================================ */

static int log_direct_internal(const char *fmt, va_list args,
                               bool skip_filter)
{
    int len = LOG_VSNPRINTF(s_buf, sizeof(s_buf), fmt, args);
    if (len <= 0) return 0;

    truncate_if_needed(&len, sizeof(s_buf));

    /* ---- Determine effective level ---- */
    char pre[2];
    if (s_batch_level) {
        /* Continuation batch: use the batch's level for filtering */
        pre[0] = s_batch_level[0];
    } else {
        pre[0] = s_buf[0];
    }
    pre[1] = '\0';

    /* ---- Level filter ---- */
    if (!skip_filter && pre[0] && log_should_drop(pre))
        return 0;

    /* ---- Lifecycle: begin ---- */
    lifecycle_begin();

    /* ---- Emit prefix + content + reset ---- */
    const char *content = s_buf;
    int content_len = len;
    if (is_kern_level(s_buf[0])) {
        content++;
        content_len--;
    }

    if (content_len > 0) {
        /* In continuation batch: suppress prefix on 2nd+ messages */
        bool show_prefix = !(s_batch_level && !s_batch_first);
        if (show_prefix) {
            do_write_str(prefix_gen(pre[0]));
        }
        do_write(content, content_len);
        if (show_prefix) {
            do_write_str(LOG_COLOR_NONE);
        }
    }
    s_batch_first = false;

    /* ---- Lifecycle: end ---- */
    lifecycle_end();

    return len;
}

/* ============================================================
 * Ring buffer producer path
 * ============================================================ */

static int log_ring_internal(const char *fmt, va_list args, bool raw)
{
    (void)raw; /* ring mode never drops — raw is a no-op here */

    int len = LOG_VSNPRINTF(s_buf, sizeof(s_buf), fmt, args);
    if (len <= 0) return 0;

    truncate_if_needed(&len, sizeof(s_buf));

    /* Push raw bytes to ring — no terminal interaction at producer side */
    uint32_t written = kfifo_put(&s_ring, (const uint8_t *)s_buf, (uint32_t)len);
    return (int)written;
}

/* ============================================================
 * Public API
 * ============================================================ */

void log_output_init(void)
{
    s_batch = 0;
    s_batch_level = NULL;
    s_batch_first = false;
    s_hook_begin_called = false;
}

void log_output_set_write_fn(log_write_fn fn)
{
    s_write_fn = fn;
}

void log_output_set_hooks(const struct log_output_hooks *hooks)
{
    if (hooks)
        s_hooks = *hooks;
    else
        memset(&s_hooks, 0, sizeof(s_hooks));
    s_hook_begin_called = false;
}

void log_output_set_term_coord(int enable)
{
    s_term_coord = enable ? 1 : 0;
}

/* ---- Standard log (with level filtering) ---- */

int log_output(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = log_output_v(fmt, args);
    va_end(args);
    return ret;
}

int log_output_v(const char *fmt, va_list args)
{
    if (!fmt || !s_write_fn) return -1;

    if (s_ring_enabled)
        return log_ring_internal(fmt, args, false);

    return log_direct_internal(fmt, args, false);
}

/* ---- Raw log (bypass level filter, still do hooks) ---- */

int log_output_raw(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = log_output_raw_v(fmt, args);
    va_end(args);
    return ret;
}

int log_output_raw_v(const char *fmt, va_list args)
{
    if (!fmt || !s_write_fn) return -1;

    if (s_ring_enabled)
        return log_ring_internal(fmt, args, true);

    return log_direct_internal(fmt, args, true);
}

/* ---- Direct log (no filter, no hooks) ---- */

int log_output_direct(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = log_output_direct_v(fmt, args);
    va_end(args);
    return ret;
}

int log_output_direct_v(const char *fmt, va_list args)
{
    if (!fmt || !s_write_fn) return -1;

    int len = LOG_VSNPRINTF(s_buf, sizeof(s_buf), fmt, args);
    if (len <= 0) return 0;
    truncate_if_needed(&len, sizeof(s_buf));

    return do_write(s_buf, len);
}

/* ---- Batch mode ---- */

void log_batch_begin(void)
{
    ++s_batch;
}

void log_batch_begin_cont(const char *level)
{
    ++s_batch;
    s_batch_level = level;
    s_batch_first = true;
}

void log_batch_end(void)
{
    if (s_batch > 0) --s_batch;
    if (!s_batch) {
        if (s_term_coord && s_hook_begin_called) {
            if (s_hooks.output_end)
                s_hooks.output_end();
            s_hook_begin_called = false;
        }
        s_batch_level = NULL;
    }
}

/* ---- Ring buffer lifecycle ---- */

void log_output_set_ring(uint8_t *buf, uint32_t size)
{
    if (buf && size) {
        kfifo_init(&s_ring, buf, size);
        s_ring_enabled = true;
    } else {
        s_ring_enabled = false;
    }
}

int log_output_flush(void)
{
    if (!s_ring_enabled || !s_write_fn) return 0;

    char buf[LOG_BUF_SIZE];
    int  total = 0;

    /* Lifecycle: begin */
    if (s_term_coord && !s_batch && s_hooks.output_begin)
        s_hooks.output_begin();

    while (kfifo_len(&s_ring) > 0) {
        uint32_t avail = kfifo_len(&s_ring);
        if (avail > sizeof(buf)) avail = sizeof(buf);
        uint32_t rlen = kfifo_get(&s_ring, (uint8_t *)buf, avail);
        if (rlen == 0) break;
        s_write_fn(buf, (int)rlen);
        total += (int)rlen;
    }

    /* Lifecycle: end */
    if (s_term_coord && !s_batch && s_hooks.output_end)
        s_hooks.output_end();

    return total;
}

/* ---- Convenience macros (defined in header) ---- */
