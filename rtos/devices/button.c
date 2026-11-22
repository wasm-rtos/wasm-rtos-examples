/* Waits for two presses of "button". It cannot take "led": another program holds it. */
#include <stdio.h>

#include "wasm-rtos.h"

WR_APP_NAME("button");

int main(void)
{
    int32_t button = wr_gpio_open_cstr("button", WR_GPIO_PULL_UP);
    int32_t led = wr_gpio_open_cstr("led", WR_GPIO_OUTPUT);
    int presses = 0;
    if (led == -WR_EINUSE)
        printf("\"led\" is in use by another program\n");
    if (button < 0)
        return 1;
    wr_gpio_watch(button, WR_GPIO_FALLING);
    while (presses < 2)
    {
        if (wr_wait_any(&button, 1, WR_WAIT_FOREVER) == 0) /* asleep until the pin's interrupt */
        {
            presses += wr_gpio_events(button);
            printf("pressed (%d)\n", presses);
        }
    }
    wr_close(button);
    return 0;
}
