  .cli_candidates : {
    KEEP(*(.cli_candidates.0.start))
    KEEP(*(.cli_candidates.1))
    KEEP(*(.cli_candidates.1.end))
  }
  .cli_commands : {
    KEEP(*(.cli_commands.0.start))
    KEEP(*(.cli_commands.1))
    KEEP(*(.cli_commands.1.end))
  }
  .cli_cmd_line : {
    KEEP(*(.cli_cmd_line.0.start))
    KEEP(*(.cli_cmd_line.1))
    KEEP(*(.cli_cmd_line.1.end))
  }
  .scheduler : {
    KEEP(*(.scheduler.0.start))
    KEEP(*(.scheduler.1))
    KEEP(*(.scheduler.1.end))
  }
  .my_init_d : {
    KEEP(*(.my_init_d.0.start))
    KEEP(*(.my_init_d.1))
    KEEP(*(.my_init_d.1.end))
  }
  .alias_cmd : {
    KEEP(*(.alias_cmd.0.start))
    KEEP(*(.alias_cmd.1))
    KEEP(*(.alias_cmd.1.end))
  }
  .cli_vars : {
    KEEP(*(.cli_vars.0.start))
    KEEP(*(.cli_vars.1))
    KEEP(*(.cli_vars.1.end))
  }
