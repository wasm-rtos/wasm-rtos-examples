/*
 * The kernel with devices: the board registers a pin port and names its
 * pins; programs open them by those names through the devices' ABI, one
 * owner at a time, and wait for them as for any kernel object. Here the
 * port is pretend (a real one writes GPIO registers) and the button is
 * pressed twice by the "interrupt" in the loop below.
 */
#include <stdio.h>
#include <stdlib.h>

#include "../../common/file.h"
#include "hw.h"
#include "os.h"

enum
{
    LED = 13,
    BUTTON = 2
};

static uint8_t g_levels[32];

static HwStatus pin_configure(void* context, uint32_t pin, uint32_t flags)
{
    (void)context, (void)flags;
    if (pin == BUTTON)
        g_levels[pin] = 1; /* pulled up */
    return HW_STATUS_OK;
}

static HwStatus pin_write(void* context, uint32_t pin, uint8_t level)
{
    (void)context;
    g_levels[pin] = level;
    return HW_STATUS_OK;
}

static HwStatus pin_read(void* context, uint32_t pin, uint8_t* out_level)
{
    (void)context;
    *out_level = g_levels[pin];
    return HW_STATUS_OK;
}

static void write_out(void* context, uint32_t task_id, uint32_t fd, const void* data, uint32_t size)
{
    static int line_start = 1;
    const char* text = (const char*)data;
    (void)context, (void)fd;
    for (uint32_t i = 0; i < size; ++i)
    {
        if (line_start)
            printf("%4u ms  [%s] ", os_get_tick_ms(), os_task_get_app_name(os_task_find_by_id(task_id)));
        putchar(text[i]);
        line_start = text[i] == '\n';
    }
}

static OsTaskHandle start(const char* path, uint8_t** bytes)
{
    uint32_t size = 0;
    OsTaskHandle task = NULL;
    if (!(*bytes = read_file(path, &size)) ||
        os_task_create(&task, *bytes, size, "_start", path, 0, OS_TASK_PRIORITY_NORMAL) != OS_STATUS_OK)
        return NULL;
    return task;
}

int main(int argc, char** argv)
{
    static const HwGpioPort gpio = {NULL, pin_configure, pin_write, pin_read};
    static const OsOutputPort output = {NULL, write_out};
    uint8_t *blinker_bytes = NULL, *button_bytes = NULL;
    OsTaskHandle blinker, button;

    if (argc < 3 || os_init() != OS_STATUS_OK)
        return printf("usage: rtos-devices blinker.wasm button.wasm\n"), 1;
    os_output_port_set(&output);
    hw_gpio_port_set(&gpio); /* the board: its pins, by name */
    hw_gpio_add_pin("led", LED, HW_GPIO_CAN_OUTPUT, 0);
    hw_gpio_add_pin("button", BUTTON, HW_GPIO_CAN_INPUT | HW_GPIO_CAN_PULL_UP | HW_GPIO_CAN_EDGES, 0);
    if (!(blinker = start(argv[1], &blinker_bytes)) || !(button = start(argv[2], &button_bytes)))
        return printf("could not start the programs\n"), 1;

    while (os_task_get_state(blinker) != OS_TASK_DEAD || os_task_get_state(button) != OS_TASK_DEAD)
    {
        uint32_t now = os_get_tick_ms();
        if (os_schedule() != OS_STATUS_NO_READY_TASKS)
            continue;
        os_tick(1);
        if (now == 25 || now == 47) /* the button goes down: what its interrupt handler would call */
        {
            g_levels[BUTTON] = 0;
            hw_gpio_edge(BUTTON, HW_GPIO_EDGE_FALLING);
        }
        else
            g_levels[BUTTON] = 1;
    }
    printf("blinker exited with %u, button with %u\n", os_task_get_exit_code(blinker), os_task_get_exit_code(button));
    os_task_delete(blinker);
    os_task_delete(button);
    os_shutdown();
    free(blinker_bytes);
    free(button_bytes);
    return 0;
}
