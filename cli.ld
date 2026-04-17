  .cli_commands : {
    _cli_commands_start = .;
    KEEP(*(.cli_commands))
    _cli_commands_end = .;
  }
  .cli_cmd_line : {
    _cli_cmd_line_start = .;
    KEEP(*(.cli_cmd_line))
    _cli_cmd_line_end = .;
  }
  .scheduler : {
    _scheduler_start = .;
    KEEP(*(.scheduler))
    _scheduler_end = .;
  }
  .my_init_d : {
    _init_d_start = .;
    KEEP(*(.my_init_d))
    _init_d_end = .;
  }

  .dispose : {
    _dispose_start = .;
    KEEP(*(.dispose))
    _dispose_end = .;
  }
