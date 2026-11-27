/*
 * dylink.0 on the kernel: a program and the shared library it needs, as
 * one process per task. The loader joins the kernel with os_dylink_add
 * (wasm-rtos-dylink), the library by the name the program needs. Two
 * tasks of the program take turns, each with its own copy of the
 * library's data; a third is saved after its first line, deleted, and goes
 * on in a new task from that snapshot, the library's count included.
 */
#include <stdio.h>
#include <stdlib.h>

#include "../../common/file.h"
#include "os.h"
#include "os_dylink.h"

static uint32_t g_lines[8];

/* Output port: every line a program prints arrives with its task's id. */
static void write_out(void* context, uint32_t task_id, uint32_t fd, const void* data, uint32_t size)
{
    static int line_start = 1;
    const char* text = (const char*)data;
    (void)context, (void)fd;
    for (uint32_t i = 0; i < size; ++i)
    {
        if (line_start)
            printf("[task %u] ", task_id);
        putchar(text[i]);
        line_start = text[i] == '\n';
        g_lines[task_id % 8] += line_start;
    }
}

static OsTaskHandle task_of(uint8_t* bytes, uint32_t size, const char* name)
{
    OsTaskHandle task = NULL;
    if (os_task_create(&task, bytes, size, "_start", name, 0, OS_TASK_PRIORITY_NORMAL) != OS_STATUS_OK)
    {
        printf("%s: %s\n", name, os_get_last_error_result());
        exit(1);
    }
    return task;
}

static void run(OsTaskHandle a, OsTaskHandle b)
{
    while (os_task_get_state(a) != OS_TASK_DEAD || (b && os_task_get_state(b) != OS_TASK_DEAD))
        if (os_schedule() == OS_STATUS_NO_READY_TASKS)
            os_tick(1);
}

int main(int argc, char** argv)
{
    static const OsOutputPort output = {NULL, write_out};
    uint32_t program_size = 0, library_size = 0, written = 0;
    uint8_t* program = read_file(argc > 1 ? argv[1] : "program.wasm", &program_size);
    uint8_t* library = read_file(argc > 2 ? argv[2] : "libtext.so", &library_size);
    OsTaskHandle a, b, saved, again;
    uint8_t* image;

    if (!program || !library || os_init() != OS_STATUS_OK)
        return printf("no program or library\n"), 1;
    os_output_port_set(&output);
    os_dylink_add();                                         /* the loader joins the kernel */
    os_dylink_add_library("libtext.so", library, library_size); /* by the name the program needs */

    /* Two tasks of one program: each process has its own copy of the library's line count. */
    a = task_of(program, program_size, "a");
    b = task_of(program, program_size, "b");
    run(a, b);
    printf("tasks %u and %u exited with %u and %u: lines each counted for itself\n", os_task_get_id(a),
           os_task_get_id(b), os_task_get_exit_code(a), os_task_get_exit_code(b));
    os_task_delete(a);
    os_task_delete(b);

    /* A third, saved after its first line: the program's instance and the library's, in one snapshot. */
    saved = task_of(program, program_size, "saved");
    while (g_lines[os_task_get_id(saved) % 8] < 1)
        os_schedule();
    os_task_get_snapshot_size(saved, &written);
    image = (uint8_t*)malloc(written);
    if (!image || os_task_save_snapshot(saved, image, written, &written) != OS_STATUS_OK)
        return printf("save: %s\n", os_get_last_error_result()), 1;
    printf("task %u saved after its first line (%u bytes) and deleted\n", os_task_get_id(saved), written);
    os_task_delete(saved);

    again = task_of(program, program_size, "again");
    if (os_task_load_snapshot(again, image, written) != OS_STATUS_OK)
        return printf("load: %s\n", os_get_last_error_result()), 1;
    run(again, NULL);
    printf("task %u went on from the snapshot and exited with %u: the library's count came with it\n",
           os_task_get_id(again), os_task_get_exit_code(again));
    os_task_delete(again);

    os_shutdown();
    free(image);
    free(program);
    free(library);
    return 0;
}
