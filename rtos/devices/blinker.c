/* Blinks "led" on a periodic timer, six times. The pin is this program's while it holds it. */
#include <stdio.h>

#include "wasm-rtos.h"

WR_APP_NAME("blinker");

int main(void)
{
    int32_t led = wr_gpio_open_cstr("led", WR_GPIO_OUTPUT);
    int32_t tick = wr_timer_create(10, WR_TIMER_PERIODIC);
    if (led < 0 || tick < 0)
        return 1;
    wr_timer_start(tick);
    for (int i = 0; i < 6; ++i)
    {
        wr_wait_any(&tick, 1, WR_WAIT_FOREVER); /* the timer fired */
        wr_timer_read(tick);
        wr_gpio_set(led, i % 2 == 0);
        printf("led %s\n", i % 2 == 0 ? "on" : "off");
    }
    wr_close(tick);
    wr_close(led);
    return 0;
}
