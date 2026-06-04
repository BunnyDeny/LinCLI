/*
 * cli_term — LinCLI terminal adapter for log_output.
 *
 * BRIDGE LAYER — the ONLY file that knows about LinCLI's internal
 * terminal state (scheduler, cmd_line, candidate_ctx).
 *
 * LOCKING STRATEGY
 * -----------------
 * The PC spinlock (__sync_lock_test_and_set) is NOT re-entrant, so we
 * cannot call redraw functions (which go through cli_out_push → lock)
 * from inside cli_enter_critical().
 *
 * Solution:
 *   cli_term_save()     — lockless, just ANSI escape "\r\033[K"
 *   cli_term_restore()  — called OUTSIDE the critical section, uses
 *                          real redraw through cli_out_push (which
 *                          acquires/releases the spinlock normally)
 *
 * cli_printk in cli_io.c handle the sequencing:
 *   save → ENTER_CS → log_output_v → EXIT_CS → restore
 *
 * On Cortex-M the critical section is interrupt masking (re-entrant
 * via BASEPRI), so the same code works correctly there too — save
 * and restore just happen to be outside the masked window.
 */

#include "log_output.h"
#include "cli_io.h"
#include "cli_cmd_line.h"
#include "cli_completion.h"
#include "cli_critical.h"

/* ============================================================
 * Forward declarations — LinCLI internal state
 * ============================================================ */

extern int scheduler_is_in_get_char(void);
extern int cli_in_exception(void);
extern struct candidate_ctx candidate_ctx;

/* ============================================================
 * Transport write_fn — called from log_output_v (inside CS,
 * uses _nolock variants to avoid spinlock re-entry).
 * ============================================================ */

static int cli_log_write(const char *buf, int len)
{
    int ret = cli_out_push_nolock((const uint8_t *)buf, len);
    if (ret < 0) return ret;
    return cli_out_sync_nolock();
}

/* ============================================================
 * Public save / restore — called from cli_io.c around CS
 * ============================================================ */

void cli_term_save(void)
{
    cli_out_push_nolock((const uint8_t *)"\r\033[K", 4);
    cli_out_sync_nolock();
}

void cli_term_restore(void)
{
    if (candidate_ctx.active)
        candidate_redraw();
    else
        cmd_line_redraw();
}

/* ============================================================
 * Interactive-mode query — shared helper
 * ============================================================ */

int cli_term_is_interactive(void)
{
    if (!scheduler_is_in_get_char())
        return 0;
    if (cli_in_exception())
        return 0;
    return 1;
}

/* ============================================================
 * Initialisation
 * ============================================================ */

void cli_term_init(void)
{
    /* Register the transport write function */
    log_output_set_write_fn(cli_log_write);

    /*
     * Disable log_output's internal terminal coordination.
     * cli_printk in cli_io.c handle save/restore
     * externally, around the critical section.
     */
    log_output_set_term_coord(0);

    log_output_init();
}