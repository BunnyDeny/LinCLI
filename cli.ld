  .cli_commands : {
    KEEP(*(.cli_commands.start))
    KEEP(*(.cli_commands))
    KEEP(*(.cli_commands.end))
  }
  .cli_cmd_line : {
    KEEP(*(.cli_cmd_line.start))
    KEEP(*(.cli_cmd_line))
    KEEP(*(.cli_cmd_line.end))
  }
  .scheduler : {
    KEEP(*(.scheduler.start))
    KEEP(*(.scheduler))
    KEEP(*(.scheduler.end))
  }
  .my_init_d : {
    KEEP(*(.my_init_d.start))
    KEEP(*(.my_init_d))
    KEEP(*(.my_init_d.end))
  }
  .dispose : {
    KEEP(*(.dispose.start))
    KEEP(*(.dispose))
    KEEP(*(.dispose.end))
  }
  .alias_cmd : {
    KEEP(*(.alias_cmd.start))
    KEEP(*(.alias_cmd))
    KEEP(*(.alias_cmd.end))
  }
