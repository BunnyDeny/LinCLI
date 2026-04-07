/* State section definition for state machine */
.scheduler : {
    _scheduler_start = .;
    *(.scheduler)
    _scheduler_end = .;
}

.my_init_d : {
    _init_d_start = .;
    *(.my_init_d)
    _init_d_end = .;
}