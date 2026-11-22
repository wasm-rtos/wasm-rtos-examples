/*
 * Two programs at once with no operating system: a scheduler in a few
 * lines. Fuel stops a program after a number of instructions, so neither
 * can keep the other out; a host function that waits stops it with
 * wm_yield until its time comes. Each program has its own runtime (its
 * stack), so each stops and goes on where it stood. An operating system
 * kernel does the same, with priorities, queues and timers around it.
 */
#include <stdio.h>
#include <string.h>

#include "../../common/file.h"
#include "wm.h"

enum
{
    TASKS = 2,
    SLICE = 3000 /* instructions a program runs before the next gets its turn */
};

typedef struct Task
{
    int id;
    WmRuntime* runtime;
    WmInstance* instance;
    WmFunc *say, *sleep, *run;
    int started, done;
    uint32_t wake_ms, slices, result;
} Task;

static uint32_t g_now_ms; /* a pretend clock: one millisecond per idle turn */

static WmResult say(WmRuntime* runtime, void* user, uint32_t* slots, WmMemory* memory)
{
    (void)runtime, (void)memory;
    printf("%3u ms  program %d: round %u done\n", g_now_ms, ((Task*)user)->id, slots[0] % 10);
    return NULL;
}

/* The program sleeps: it stops here, and the scheduler goes on with it once its time comes. */
static WmResult sleep_ms(WmRuntime* runtime, void* user, uint32_t* slots, WmMemory* memory)
{
    (void)memory;
    ((Task*)user)->wake_ms = g_now_ms + slots[0];
    return wm_yield(runtime);
}

/* The resolver's context is the task: its own functions know whose call it is. */
static WmResult resolve(void* context, const char* module, uint32_t module_length, const char* name,
                        uint32_t name_length, uint8_t kind, WmExtern* out)
{
    Task* task = (Task*)context;
    (void)module, (void)module_length, (void)kind;
    out->kind = WM_EXTERN_FUNC;
    out->as.func = name_length == 3 && !memcmp(name, "say", 3) ? task->say : task->sleep;
    return NULL;
}

int main(int argc, char** argv)
{
    static const uint8_t i32[] = {WM_I32};
    uint32_t size = 0;
    uint8_t* bytes = read_file(argc > 1 ? argv[1] : "program.wasm", &size);
    WmModule* module = NULL;
    Task tasks[TASKS];
    int left = TASKS;
    WmResult r;

    if (!bytes || (r = wm_module_load(&module, bytes, size))) /* one module, loaded once */
        return printf("load: %s\n", bytes ? r : "no program"), 1;
    for (int i = 0; i < TASKS; ++i) /* two instances of it, each on its own runtime */
    {
        Task* t = &tasks[i];
        WmExtern run;
        memset(t, 0, sizeof(*t));
        t->id = i + 1;
        t->runtime = wm_runtime_new(8 * 1024, 64);
        t->say = wm_host_function_new(i32, 1, NULL, 0, say, t);
        t->sleep = wm_host_function_new(i32, 1, NULL, 0, sleep_ms, t);
        if ((r = wm_instantiate(&t->instance, module, t->runtime, resolve, t)) ||
            (r = wm_instance_export(t->instance, "run", 3, &run)))
            return printf("instantiate: %s\n", r), 1;
        t->run = run.as.func;
    }

    while (left) /* the scheduler: round robin over those whose time has come */
    {
        int ran = 0;
        for (int i = 0; i < TASKS; ++i)
        {
            Task* t = &tasks[i];
            uint32_t arg = (uint32_t)t->id;
            if (t->done || t->wake_ms > g_now_ms)
                continue;
            wm_set_fuel(t->runtime, SLICE);
            r = t->started ? wm_resume(t->runtime, &t->result) : wm_call(t->runtime, t->run, &arg, &t->result);
            t->started = 1;
            ++t->slices;
            ran = 1;
            if (r == wm_fuel_exhausted || r == wm_yielded)
                continue; /* stopped where it stood: its turn comes again */
            if (r)
                return printf("program %d: %s\n", t->id, r), 1;
            t->done = 1;
            --left;
            printf("%3u ms  program %d: ended with %u after %u slices\n", g_now_ms, t->id, t->result, t->slices);
        }
        if (!ran)
            ++g_now_ms; /* everyone sleeps: time passes (a real host would sleep the processor) */
    }

    for (int i = 0; i < TASKS; ++i)
    {
        wm_instance_free(tasks[i].instance);
        wm_host_function_free(tasks[i].say);
        wm_host_function_free(tasks[i].sleep);
        wm_runtime_free(tasks[i].runtime);
    }
    wm_module_free(module);
    free(bytes);
    return 0;
}
