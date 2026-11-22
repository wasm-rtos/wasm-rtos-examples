/* Each copy works a while, says how far it got, and sleeps; three times. */
__attribute__((import_module("env"), import_name("say"))) void say(int what);
__attribute__((import_module("env"), import_name("sleep"))) void sleep_ms(int ms);

__attribute__((export_name("run"))) int run(int id)
{
    unsigned sum = 0;
    for (int round = 1; round <= 3; ++round)
    {
        for (int i = 0; i < 20000 * id; ++i) /* work: copy 2 works twice as long */
            sum += (unsigned)i ^ sum >> 3;
        say(id * 10 + round);
        sleep_ms(5 * id);
    }
    return (int)(sum & 0xFF);
}
