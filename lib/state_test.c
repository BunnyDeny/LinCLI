/**
 * @file state_test.c
 * @brief Test file to verify .state section linking
 */
#include "stateM.h"
#include <stdio.h>

/* Forward declarations - must be declared before macro */
void test_state_entry(void *private);
int test_state_task(void *private);
void test_state_exit(void *private);

/* Define states using the macro - these will go into .state section */
_EXPORT_STATE_SYMBOL(test, test_state_entry, test_state_task, test_state_exit, ".state");

/* State functions implementation */
void test_state_entry(void *private)
{
    printf("[test] entry\n");
}

int test_state_task(void *private)
{
    printf("[test] task\n");
    return 0;
}

void test_state_exit(void *private)
{
    printf("[test] exit\n");
}