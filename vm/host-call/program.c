/* The program calls two functions the host gives it, from the module "env". */
__attribute__((import_module("env"), import_name("led"))) void led(int on);
__attribute__((import_module("env"), import_name("temperature"))) int temperature(void);

__attribute__((export_name("run"))) int run(void)
{
    int warm = temperature() > 25;
    led(warm); /* on when it is warm */
    return warm;
}
