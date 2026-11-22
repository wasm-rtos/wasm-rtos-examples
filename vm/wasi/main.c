/*
 * A program built with a WASI C library, run by the interpreter alone:
 * wm_wasi answers WASI's calls, and the host says only what they mean here
 * (where output goes, the clock, the arguments, what exit does). Calls it
 * does not answer, such as files, link and return ENOSYS.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../common/file.h"
#include "wm.h"
#include "wm_wasi.h"

static const char g_exited[] = "program exited";
static uint32_t g_exit_code;
static WmModule* g_module;

static uint64_t now_ns(void* context, uint32_t clock)
{
    struct timespec t;
    (void)context;
    clock_gettime(clock == 0 ? CLOCK_REALTIME : CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}

static uint32_t write_out(void* context, WmRuntime* runtime, uint32_t fd, const void* data, uint32_t length)
{
    (void)context, (void)runtime;
    if (!data)
        return 8; /* EBADF: there are no files */
    fwrite(data, 1, length, fd == 2 ? stderr : stdout);
    return 0;
}

static uint32_t args(void* context, WmRuntime* runtime, const char* const** out)
{
    static const char* const argv[] = {"program.wasm", "one", "two"};
    (void)context, (void)runtime;
    *out = argv;
    return 3;
}

/* proc_exit ends the call with a trap of the host's: the host knows it from any other. */
static WmResult program_exit(void* context, WmRuntime* runtime, uint32_t code)
{
    (void)context, (void)runtime;
    g_exit_code = code;
    return g_exited;
}

static const WmWasiHost g_wasi = {.now_ns = now_ns, .write = write_out, .args = args, .exit = program_exit};

static WmResult resolve(void* context, const char* module, uint32_t module_length, const char* name,
                        uint32_t name_length, uint8_t kind, WmExtern* out)
{
    WmResult r = wm_wasi_resolve(&g_wasi, module, module_length, name, name_length, &out->as.func);
    (void)context, (void)kind;
    out->kind = WM_EXTERN_FUNC;
    if (!r && !out->as.func) /* a WASI call nothing here answers: ENOSYS */
        r = wm_wasi_resolve_absent(g_module, module, module_length, name, &out->as.func);
    return r ? r : out->as.func ? NULL : "unknown import";
}

int main(int argc, char** argv)
{
    uint32_t size = 0;
    uint8_t* bytes = read_file(argc > 1 ? argv[1] : "program.wasm", &size);
    WmRuntime* runtime = wm_runtime_new(64 * 1024, 256);
    WmInstance* instance = NULL;
    WmExtern start;
    WmResult r;

    if (!bytes || (r = wm_module_load(&g_module, bytes, size)) ||
        (r = wm_instantiate(&instance, g_module, runtime, resolve, NULL)) ||
        (r = wm_instance_export(instance, "_start", 6, &start)))
        return printf("load: %s\n", bytes ? r : "no program"), 1;
    r = wm_call(runtime, start.as.func, NULL, NULL); /* _start runs main */
    if (r && r != g_exited)
        return printf("trap: %s\n", r), 1;
    printf("host: the program ended with code %u\n", r ? g_exit_code : 0);

    wm_instance_free(instance);
    wm_module_free(g_module);
    wm_runtime_free(runtime);
    wm_wasi_free();
    free(bytes);
    return 0;
}
