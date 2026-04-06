/* State section definition for state machine */
.scheduler : {
    _scheduler_start = .;
    *(.scheduler)
    _scheduler_end = .;
}