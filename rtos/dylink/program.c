/*
 * A program built with clang -pie that needs libtext.so: three rounds of
 * work, a line after each. Its exit code is how many lines the library
 * counted for it.
 */
extern void text_print(const char* text);
extern void text_number(int number);
extern int text_lines(void);

__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit")))
void proc_exit(int code);

void _start(void)
{
    volatile int work = 0;
    for (int round = 1; round <= 3; ++round)
    {
        for (int i = 0; i < 40000; ++i)
            work += i & 1;
        text_print("round ");
        text_number(round);
        text_print(" done\n");
    }
    proc_exit(text_lines());
}
