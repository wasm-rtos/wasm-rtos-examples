/*
 * A worker: it takes numbers from the queue "numbers" and prints their
 * squares, until it takes a 0. Two tasks run this same program; the
 * kernel shares its loaded code between them.
 */
#include <stdio.h>

#include "wasm-rtos.h"

WR_APP_NAME("worker");

int main(void)
{
    int32_t numbers = wr_queue_open("numbers", sizeof(int32_t), 4); /* the host made it: this opens it */
    int32_t n = 0, done = 0;
    if (numbers < 0)
        return 1;
    while (wr_queue_receive(numbers, &n, WR_WAIT_FOREVER) == 0 && n != 0) /* waits without using the processor */
    {
        printf("%d squared is %d\n", (int)n, (int)(n * n));
        ++done;
        wr_sleep_ms(3); /* the other worker runs meanwhile */
    }
    printf("done after %d numbers\n", (int)done);
    return 0;
}
