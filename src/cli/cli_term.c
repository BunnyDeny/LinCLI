/*
 * cli_term — LinCLI terminal adapter for log_output.
 *
 * BRIDGE LAYER — the ONLY file that knows about LinCLI's internal
 * terminal state (scheduler, cmd_line, candidate_ctx).
 *
 * LIFECYCLE HOOKS
 * ---------------
 * This file registers log_output hooks so the library handles
 * output_begin automatically.  output_begin writes \r\033[K via
 * nolock (safe inside the critical section).
 *
 * output_end is NOT registered here.  On PC, cli_term_restore()
 * calls candidate_redraw / cmd_line_redraw which go through
 * cli_out_push (acquires spinlock).  Since restore must run
 * OUTSIDE the critical section, cli_printk in cli_io.c handles
 * it after cli_exit_critical().  The hooks cover the "before"
 * part; the wrapper covers the "after" part — best of both worlds.
 *
 * On Cortex-M, the critical section is interrupt masking
 * (re-entrant via BASEPRI), so the same code works correctly
 * there too — save is lockless, restore runs outside the masked
 * window.
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
 * Lifecycle hooks — called from log_output_v inside CS
 * ============================================================ */

static void cli_hook_begin(void)
{
    /* 检查提示符是否在屏幕上：调度器在 get_char 状态
     *
     * 不检查 cli_in_exception()——ISR 中打印同样需要清行。
     * 但 ISR 中不需要 restore（restore 用自旋锁，ISR 中不能拿），
     * 这个保护由 cli_printk / cli_printk_batch_end 负责。 */
    if (!scheduler_is_in_get_char())
        return;

    /* Lockless: reset color + clear current prompt line before output.
     * \033[0m 复位颜色状态（防止 ISR 在提示符绘制中途打断，
     *            \033[K 只清内容不清颜色状态）
     * \r\033[K 回到行首并清行
     * Safe inside CS because it uses _nolock variants. */
    cli_out_push_nolock((const uint8_t *)"\033[0m\r\033[K", 7);
    cli_out_sync_nolock();
}

/* ============================================================
 * Public restore — called from cli_printk OUTSIDE critical section.
 *
 * cli_term_restore uses cli_out_push (with spinlock), so it must
 * run after cli_exit_critical() on PC platforms.
 * ============================================================ */

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

    /* Register lifecycle hooks — coord=1 so the library
     * calls output_begin automatically before each output.
     * output_end is NULL because restore needs the spinlock
     * and must run outside the critical section (handled by
     * cli_printk's wrapper).
     * cli_hook_begin itself guards with cli_term_is_interactive(),
     * so no separate in_atomic mechanism is needed. */
    static const struct log_output_hooks hooks = {
        .output_begin = cli_hook_begin,
        .output_end   = NULL,          /* restore done by cli_printk wrapper */
    };
    log_output_set_hooks(&hooks);
    log_output_set_term_coord(1);

    log_output_init();
}
