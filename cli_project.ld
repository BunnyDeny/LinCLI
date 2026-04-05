/* State section definition for state machine */
.state : {
    _state_sec_start = .;
    *(.state)
    _state_sec_end = .;
}