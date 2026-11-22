/*
 * The interpreter alone: load a module, instantiate it, call what it
 * exports. Four calls are the whole of it.
 */
#include <stdio.h>

#include "../../common/file.h"
#include "wm.h"

int main(int argc, char** argv)
{
    uint32_t size = 0, args[2] = {2, 3}, result = 0;
    uint8_t* bytes = read_file(argc > 1 ? argv[1] : "program.wasm", &size);
    WmRuntime* runtime = wm_runtime_new(16 * 1024, 256); /* a stack of values and frames, for calls */
    WmModule* module = NULL;
    WmInstance* instance = NULL;
    WmExtern add, fib;
    WmResult r;

    if (!bytes)
        return printf("no program\n"), 1;
    if ((r = wm_module_load(&module, bytes, size)) ||                   /* parse and check */
        (r = wm_instantiate(&instance, module, runtime, NULL, NULL)) || /* memory, globals: a running copy */
        (r = wm_instance_export(instance, "add", 3, &add)) || (r = wm_instance_export(instance, "fib", 3, &fib)))
        return printf("load: %s\n", r), 1;

    if ((r = wm_call(runtime, add.as.func, args, &result)))
        return printf("add: %s\n", r), 1;
    printf("add(2, 3) = %u\n", result);
    args[0] = 20;
    if ((r = wm_call(runtime, fib.as.func, args, &result)))
        return printf("fib: %s\n", r), 1;
    printf("fib(20) = %u\n", result);

    wm_instance_free(instance);
    wm_module_free(module);
    wm_runtime_free(runtime);
    free(bytes);
    return result != 6765;
}
