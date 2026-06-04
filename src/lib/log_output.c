/*
 * log_output — standalone embedded-friendly logging library.
 *
 * All output goes through an internal kfifo ring buffer.
 * No "direct mode" exists — every message is pushed to the ring.
 * The ONLY output path to the transport is log_output_flush().
 *
 * LIFECYCLE HOOKS
 * ---------------
 * Register hooks via log_output_set_hooks().  When coord=1 (default),
 * hooks fire on the PRODUCER side (inside log_output/log_output_raw):
 *
 *   output_begin()  — called before push (clear line, unlock flash)
 *   output_end()    — called after  push (restore prompt, relock flash)
 *
 * The CONSUMER side (log_output_flush()) never fires hooks — it just
 * drains the ring and calls write_fn.  This separation ensures that:
 *   - ISR:  producer pushes to ring (fast, no I/O)
 *   - Task: flush writes to transport (actual I/O, with hooks already
 *           fired on the producer side)
 *
 * coord=0 disables automatic hooks (user manages them externally).
 *
 * Integration:
 *   1. log_output_set_write_fn(fn)  — mandatory (the transport)
 *   2. log_output_set_hooks()       — optional (lifecycle hooks)
 *   3. log_output_init()             — optional (reset state, inits ring)
 *   4. log_output() / log_*()        — emit messages (push to ring)
 *   5. log_output_flush()            — drain ring → write_fn (call periodically)
 */

#include "log_output.h"
#include <stdio.h>
#include <string.h>

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
static const char            *s_batch_level;
static bool                  s_batch_first;
static int                   s_term_coord = 1;
static bool                  s_hook_begin_called;

static char s_buf[LOG_BUF_SIZE];

/* Internal ring buffer — all output goes through here */
static kfifo_t      s_ring;
static uint8_t      s_ring_buf[128];
static int          s_ring_inited;

/* ============================================================
 * Level / prefix helpers
 * ============================================================ */

static const char *s_prefix_table[] = {
    LOG_COLOR_BOLD LOG_COLOR_RED " [E] " LOG_COLOR_NONE,
    LOG_COLOR_MAGENTA          " [A] " LOG_COLOR_NONE,
    LOG_COLOR_RAINBOW_2       " [C] " LOG_COLOR_NONE,
    LOG_COLOR_RED             " [E] " LOG_COLOR_NONE,
    LOG_COLOR_YELLOW          " [W] " LOG_COLOR_NONE,
    LOG_COLOR_BOLD LOG_COLOR_GREEN " " LOG_COLOR_NONE,
    LOG_COLOR_BLUE            " [I] " LOG_COLOR_NONE,
    LOG_COLOR_RAINBOW_4       " [D] " LOG_COLOR_NONE,
    "",
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
 * Truncate oversized buffer
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
 * Lifecycle helpers
 * ============================================================ */

static inline void lifecycle_begin(void)
{
    if (!s_term_coord) return;
    if (s_hook_begin_called)
        return;
    if (s_hooks.output_begin)
        s_hooks.output_begin();
    s_hook_begin_called = true;
}

static inline void lifecycle_end(void)
{
    if (!s_term_coord) return;
    if (!s_hook_begin_called)
        return;
    if (s_batch)
        return;
    if (s_hooks.output_end)
        s_hooks.output_end();
    s_hook_begin_called = false;
}

/* ============================================================
 * Single producer path — always pushes to kfifo ring
 * ============================================================ */

static int log_output_internal(const char *fmt, va_list args,
                               bool skip_filter, bool skip_hooks)
{
    if (!s_ring_inited) return -1;

    int len = LOG_VSNPRINTF(s_buf, sizeof(s_buf), fmt, args);
    if (len <= 0) return 0;

    truncate_if_needed(&len, sizeof(s_buf));

    /* Determine effective level */
    char pre[2];
    if (s_batch_level) {
        pre[0] = s_batch_level[0];
    } else {
        pre[0] = s_buf[0];
    }
    pre[1] = '\0';

    /* Level filter */
    if (!skip_filter && pre[0] && log_should_drop(pre))
        return 0;

    /* Lifecycle */
    if (!skip_hooks)
        lifecycle_begin();

    /* Emit prefix + content + color reset → ring */
    const char *content = s_buf;
    int content_len = len;
    if (is_kern_level(s_buf[0])) {
        content++;
        content_len--;
    }

    if (content_len > 0) {
        bool show_prefix = !(s_batch_level && !s_batch_first);
        if (show_prefix) {
            const char *pref = prefix_gen(pre[0]);
            kfifo_put(&s_ring, (const uint8_t *)pref, (uint32_t)strlen(pref));
        }
        kfifo_put(&s_ring, (const uint8_t *)content, (uint32_t)content_len);
        if (show_prefix) {
            kfifo_put(&s_ring, (const uint8_t *)LOG_COLOR_NONE,
                      (uint32_t)strlen(LOG_COLOR_NONE));
        }
    }
    s_batch_first = false;

    /* Lifecycle */
    if (!skip_hooks)
        lifecycle_end();

    return len;
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
    kfifo_init(&s_ring, s_ring_buf, sizeof(s_ring_buf));
    s_ring_inited = 1;
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

/* ---- Standard log (with level filtering + hooks) ---- */

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
    if (!fmt || !s_write_fn || !s_ring_inited) return -1;
    return log_output_internal(fmt, args, false, false);
}

/* ---- Raw log (bypass filter, keep hooks) ---- */

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
    if (!fmt || !s_write_fn || !s_ring_inited) return -1;
    return log_output_internal(fmt, args, true, false);
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
    if (!fmt || !s_write_fn || !s_ring_inited) return -1;

    int len = LOG_VSNPRINTF(s_buf, sizeof(s_buf), fmt, args);
    if (len <= 0) return 0;
    truncate_if_needed(&len, sizeof(s_buf));

    kfifo_put(&s_ring, (const uint8_t *)s_buf, (uint32_t)len);
    return len;
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

/* ---- Ring buffer lifecycle / flush ---- */

void log_output_set_ring(uint8_t *buf, uint32_t size)
{
    if (buf && size) {
        kfifo_init(&s_ring, buf, size);
        s_ring_inited = 1;
    }
}

int log_output_flush(void)
{
    if (!s_ring_inited || !s_write_fn) return 0;

    char buf[LOG_BUF_SIZE];
    int  total = 0;

    while (kfifo_len(&s_ring) > 0) {
        uint32_t avail = kfifo_len(&s_ring);
        if (avail > sizeof(buf)) avail = sizeof(buf);
        uint32_t rlen = kfifo_get(&s_ring, (uint8_t *)buf, avail);
        if (rlen == 0) break;
        s_write_fn(buf, (int)rlen);
        total += (int)rlen;
    }

    return total;
}

/* ---- Convenience macros (defined in header) ---- */
