/*
 * LinCLI - State machine integer IDs for all registered states.
 *
 * Replaces string-based state names with compile-time integer constants
 * to eliminate .rodata string bloat and strcmp overhead in the state engine.
 */

#ifndef _CLI_STATE_IDS_H_
#define _CLI_STATE_IDS_H_

/* ============================================================
 *  Scheduler states (1–9)
 * ============================================================ */
#define STATE_ID_scheduler_start       1
#define STATE_ID_scheduler_get_char    2
#define STATE_ID_scheduler_dispose     3
#define STATE_ID_scheduler_cmd_run     4
#define STATE_ID_scheduler_auto_run    5

/* ============================================================
 *  Command-line states (10–50)
 * ============================================================ */
#define STATE_ID_cmd_line_start        10
#define STATE_ID_valid_char            11
#define STATE_ID_invalid_char          12
#define STATE_ID_ESC_handler           13
#define STATE_ID_cursor_left           14
#define STATE_ID_cursor_right          15
#define STATE_ID_cmd_cycle_left        16
#define STATE_ID_cmd_cycle_right       17
#define STATE_ID_cmd_cycle_up          18
#define STATE_ID_cmd_cycle_down        19
#define STATE_ID_opt_cycle_left        20
#define STATE_ID_opt_cycle_right       21
#define STATE_ID_opt_cycle_up          22
#define STATE_ID_opt_cycle_down        23
#define STATE_ID_value_cycle_prev      24
#define STATE_ID_value_cycle_next      25
#define STATE_ID_history_up            26
#define STATE_ID_history_down          27
#define STATE_ID_tab_complete          28
#define STATE_ID_tab_cycle_enter       29
#define STATE_ID_tab_cycle             30
#define STATE_ID_delete                31
#define STATE_ID_backspace_handler     32
#define STATE_ID_clear                 33
#define STATE_ID_enter                 34
#define STATE_ID_exit_handler          35

#endif
