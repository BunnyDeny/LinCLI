/*
 * log_output — standalone embedded-friendly logging library.
 *
 * DESIGN
 * ------
 * log_output is decoupled from any specific transport, terminal UI, or
 * RTOS.  It provides two output modes:
 *
 *   DIRECT  (default) — each log() call formats, filters, and writes
 *   RING    (optional) — each log() formats and pushes to a kfifo ring
 *                        buffer; a background task calls flush() to drain
 *                        the ring and write to the transport.
 *
 * Ring mode requires LOG_OUTPUT_HAVE_KFIFO and a kfifo_t from kfifo.h.
 * Without it, the library falls back to direct mode with no ring overhead.
 *
 * INTEGRATION
 * -----------
 *   1. log_output_set_write_fn(fn)  — mandatory (the transport)
 *   2. log_output_set_term_ops()    — optional (interactive terminal)
 *   3. log_output_init()             — optional (reset state)
 *   4. log_output() / log_*() macros — emit messages
 *
 * For standalone extraction (e.g. a separate GitHub repo), copy only
 * log_output.h + log_output.c.  kfifo is pulled in only when the
 * consuming project already provides it.
 */

#include "log_output.h"
#include <stdio.h>
#include <string.h>

/*
 * Overridable vsnprintf — default to standard library.
 * LinCLI sets LOG_VSNPRINTF to cli_vsnprintf for its embedded build.
 */
#ifndef LOG_VSNPRINTF
#define LOG_VSNPRINTF   vsnprintf
#endif

#ifdef LOG_OUTPUT_HAVE_KFIFO
#include "kfifo.h"
#endif

/* ============================================================
 * Static state
 * ============================================================ */

char g_log_level[3] = "8";

static log_write_fn       s_write_fn = NULL;
static struct log_term_ops s_ops;
static int                s_batch;
static int                s_term_coord = 1;  /* default: internal save/restore enabled */

/* ISR first-print coordination (direct mode only):
 * after a task-context print restores the prompt, this flag is set.
 * The next ISR-context print uses "\r" to overwrite the prompt line
 * before emitting its content. */
static int s_isr_newline_pending;

/* Shared format buffer — safe because log_output holds a critical section
 * externally on the LinCLI side, or callers serialise access. */
static char s_buf[LOG_BUF_SIZE];

#ifdef LOG_OUTPUT_HAVE_KFIFO
static kfifo_t s_ring;
static bool    s_ring_enabled;
#endif

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
 * Format + filter + emit (direct path)
 *
 * All terminal coordination (save/restore, ISR first-print
 * handling) lives here.  Called from log_output_v when ring
 * mode is NOT active.
 * ============================================================ */

static int log_direct_internal(const char *fmt, va_list args,
                               bool skip_filter)
{
    int len = LOG_VSNPRINTF(s_buf, sizeof(s_buf), fmt, args);
    if (len <= 0) return 0;

    truncate_if_needed(&len, sizeof(s_buf));

    char pre[2] = { s_buf[0], '\0' };

    /* ---- Level filter ---- */
    if (!skip_filter && pre[0] && log_should_drop(pre))
        return 0;

    int in_irq   = (s_ops.in_irq) ? s_ops.in_irq() : 0;
    int interact = (s_ops.is_interactive) ? s_ops.is_interactive() : 0;

    /* ---- Terminal save ---- */
    if (s_term_coord && interact) {
        if (!in_irq) {
            /* Task context: clear prompt line before printing */
            if (!s_batch) {
                do_write_str("\r\033[K");
            }
            s_isr_newline_pending = 0;
        } else if (s_isr_newline_pending) {
            /* ISR context, and prompt was last restored by task:
             * need "\r" to overwrite the stale prompt line. */
            do_write_str("\r");
            s_isr_newline_pending = 0;
        }
    }

    /* ---- Emit prefix + content + reset ---- */
    const char *content = s_buf;
    int content_len = len;
    if (is_kern_level(s_buf[0])) {
        content++;
        content_len--;
    }

    if (content_len > 0) {
        do_write_str(prefix_gen(pre[0]));
        do_write(content, content_len);
        do_write_str(LOG_COLOR_NONE);
    }

    /* ---- Terminal restore ---- */
    if (s_term_coord && interact && !in_irq && !s_batch) {
        if (len > 0 && s_buf[len - 1] != '\n') {
            do_write_str("\r\n");
        }
        if (s_ops.restore)
            s_ops.restore();
        s_isr_newline_pending = 1;
    }

    return len;
}

/* ============================================================
 * Ring buffer producer path
 * ============================================================ */

#ifdef LOG_OUTPUT_HAVE_KFIFO

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

#endif /* LOG_OUTPUT_HAVE_KFIFO */

/* ============================================================
 * Public API
 * ============================================================ */

void log_output_init(void)
{
    s_batch = 0;
    s_isr_newline_pending = 0;
}

void log_output_set_write_fn(log_write_fn fn)
{
    s_write_fn = fn;
}

void log_output_set_term_ops(const struct log_term_ops *ops)
{
    if (ops)
        s_ops = *ops;
    else
        memset(&s_ops, 0, sizeof(s_ops));
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

#ifdef LOG_OUTPUT_HAVE_KFIFO
    if (s_ring_enabled)
        return log_ring_internal(fmt, args, false);
#endif

    return log_direct_internal(fmt, args, false);
}

/* ---- Raw log (bypass level filter, still do terminal co-ord) ---- */

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

#ifdef LOG_OUTPUT_HAVE_KFIFO
    if (s_ring_enabled)
        return log_ring_internal(fmt, args, true);
#endif

    return log_direct_internal(fmt, args, true);
}

/* ---- Direct log (no filter, no save/restore, just format+write) ---- */

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
    if (!s_batch) {
        do_write_str("\r\033[K");
    }
    ++s_batch;
}

void log_batch_end(void)
{
    if (s_batch > 0) --s_batch;
    if (!s_batch && s_ops.restore)
        s_ops.restore();
}

/* ---- Ring buffer lifecycle (only available with kfifo) ---- */

#ifdef LOG_OUTPUT_HAVE_KFIFO

void log_output_set_ring(kfifo_t *ring, uint8_t *buf, uint32_t size)
{
    if (ring && buf && size) {
        /* Use the caller-provided fifo structure */
        s_ring     = *ring;  /* shallow-copy the descriptor */
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

    int interact = (s_ops.is_interactive) ? s_ops.is_interactive() : 0;

    /* Save terminal once for the entire batch */
    if (interact && !s_batch && s_ops.save)
        s_ops.save();

    while (kfifo_len(&s_ring) > 0) {
        uint32_t avail = kfifo_len(&s_ring);
        if (avail > sizeof(buf)) avail = sizeof(buf);
        uint32_t rlen = kfifo_get(&s_ring, (uint8_t *)buf, avail);
        if (rlen == 0) break;
        s_write_fn(buf, (int)rlen);
        total += (int)rlen;
    }

    /* Restore terminal once */
    if (interact && !s_batch && s_ops.restore)
        s_ops.restore();

    return total;
}

#endif /* LOG_OUTPUT_HAVE_KFIFO */
