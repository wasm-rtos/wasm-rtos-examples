/*
 * The kernel on a computer: two tasks of one program, a queue the host
 * feeds them through, and their output marked with who wrote it. The
 * platform gives the kernel only a clock (here os_tick) and an output
 * port; everything else is the kernel's: priorities, waiting on a queue,
 * sleeping, sharing a program's code.
 */
#include <stdio.h>
#include <stdlib.h>

#include "../../common/file.h"
#include "os.h"

/* Output port: every line a program prints arrives with its task's id. */
static void write_out(void* context, uint32_t task_id, uint32_t fd, const void* data, uint32_t size)
{
    static int line_start = 1;
    const char* text = (const char*)data;
    (void)context, (void)fd;
    for (uint32_t i = 0; i < size; ++i)
    {
        if (line_start)
            printf("%4u ms  [task %u] ", os_get_tick_ms(), task_id);
        putchar(text[i]);
        line_start = text[i] == '\n';
    }
}

int main(int argc, char** argv)
{
    static const OsOutputPort output = {NULL, write_out};
    uint32_t size = 0;
    uint8_t* bytes = read_file(argc > 1 ? argv[1] : "program.wasm", &size);
    OsQueueHandle numbers = NULL;
    OsTaskHandle workers[2];
    int32_t next = 1, stops = 0;

    if (!bytes || os_init() != OS_STATUS_OK)
        return printf("no program\n"), 1;
    os_output_port_set(&output);
    os_queue_create(&numbers, sizeof(int32_t), 4);
    os_queue_set_name(numbers, "numbers"); /* programs open it by this name */
    for (int i = 0; i < 2; ++i)
        if (os_task_create(&workers[i], bytes, size, "_start", "worker", 0, OS_TASK_PRIORITY_NORMAL) != OS_STATUS_OK)
            return printf("task: %s\n", os_task_get_error(workers[i])), 1;

    while (os_task_get_state(workers[0]) != OS_TASK_DEAD || os_task_get_state(workers[1]) != OS_TASK_DEAD)
    {
        /* The host's own work between scheduling decisions: feed the queue, then two 0s to stop. */
        if (next <= 8 && os_queue_send(numbers, &next) == OS_STATUS_OK)
            ++next;
        else if (next > 8 && stops < 2 && os_queue_send(numbers, &(int32_t){0}) == OS_STATUS_OK)
            ++stops;
        if (os_schedule() == OS_STATUS_NO_READY_TASKS)
            os_tick(1); /* nobody ready: time passes (a board sleeps until its timer) */
    }
    for (int i = 0; i < 2; ++i)
    {
        printf("worker task %u exited with %u\n", os_task_get_id(workers[i]), os_task_get_exit_code(workers[i]));
        os_task_delete(workers[i]);
    }
    os_queue_delete(numbers);
    os_shutdown();
    free(bytes);
    return 0;
}
