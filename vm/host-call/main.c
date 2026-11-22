/*
 * Functions of the host a program calls: made with wm_host_function_new and
 * given to the program by the resolver, which wm_instantiate asks for each
 * import by its module and name. This is how anything reaches the outside:
 * the operating system and the devices are such functions too.
 */
#include <stdio.h>
#include <string.h>

#include "../../common/file.h"
#include "wm.h"

/* slots hold the arguments, and the result goes into the first. */
static WmResult led(WmRuntime* runtime, void* user, uint32_t* slots, WmMemory* memory)
{
    (void)runtime, (void)user, (void)memory;
    printf("host: led %s\n", slots[0] ? "on" : "off");
    return NULL;
}

static WmResult temperature(WmRuntime* runtime, void* user, uint32_t* slots, WmMemory* memory)
{
    (void)runtime, (void)memory;
    slots[0] = *(const uint32_t*)user;
    printf("host: temperature %u\n", slots[0]);
    return NULL;
}

static WmFunc *g_led, *g_temperature;

static WmResult resolve(void* context, const char* module, uint32_t module_length, const char* name,
                        uint32_t name_length, uint8_t kind, WmExtern* out)
{
    (void)context;
    if (kind != WM_EXTERN_FUNC || module_length != 3 || memcmp(module, "env", 3))
        return "unknown import";
    out->kind = WM_EXTERN_FUNC;
    out->as.func = name_length == 3 && !memcmp(name, "led", 3) ? g_led : g_temperature;
    return NULL;
}

int main(int argc, char** argv)
{
    static const uint8_t i32[] = {WM_I32};
    static const uint32_t degrees = 31;
    uint32_t size = 0, result = 0;
    uint8_t* bytes = read_file(argc > 1 ? argv[1] : "program.wasm", &size);
    WmRuntime* runtime = wm_runtime_new(16 * 1024, 64);
    WmModule* module = NULL;
    WmInstance* instance = NULL;
    WmExtern run;
    WmResult r;

    g_led = wm_host_function_new(i32, 1, NULL, 0, led, NULL);                     /* (i32) -> () */
    g_temperature = wm_host_function_new(NULL, 0, i32, 1, temperature, (void*)&degrees); /* () -> i32 */
    if (!bytes || (r = wm_module_load(&module, bytes, size)) ||
        (r = wm_instantiate(&instance, module, runtime, resolve, NULL)) ||
        (r = wm_instance_export(instance, "run", 3, &run)) || (r = wm_call(runtime, run.as.func, NULL, &result)))
        return printf("error: %s\n", bytes ? r : "no program"), 1;
    printf("run() = %u\n", result);

    wm_instance_free(instance);
    wm_module_free(module);
    wm_host_function_free(g_led);
    wm_host_function_free(g_temperature);
    wm_runtime_free(runtime);
    free(bytes);
    return result != 1;
}
