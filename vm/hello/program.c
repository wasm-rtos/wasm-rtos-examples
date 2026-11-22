/* The program: two functions the host calls. No imports, no C library. */
__attribute__((export_name("add"))) int add(int a, int b) { return a + b; }

__attribute__((export_name("fib"))) int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
