/* An ordinary C program: the C library for WASI turns printf into fd_write. */
#include <stdio.h>
#include <time.h>

int main(int argc, char** argv)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    printf("hello from %s, %d argument(s)\n", argc > 0 ? argv[0] : "?", argc - 1);
    for (int i = 1; i < argc; ++i)
        printf("  argument %d: %s\n", i, argv[i]);
    printf("the host's clock says %lld s\n", (long long)now.tv_sec);
    return argc > 1 ? 0 : 3; /* the exit code comes back to the host */
}
