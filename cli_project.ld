/* State section definition for state machine */
.state : {
    _scheduler_start = .;
    *(.state)
    _scheduler_end = .;
}