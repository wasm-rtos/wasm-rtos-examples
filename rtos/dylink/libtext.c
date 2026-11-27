/*
 * A shared library (clang -shared): text output for the programs that need
 * it, and a count of the lines its process printed. Each process that loads
 * it has its own copy of that count, as with an ELF shared object.
 */
#include <stdint.h>

typedef struct
{
    const void* data;
    uint32_t length;
} Piece;

__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
int32_t fd_write(int32_t fd, const Piece* pieces, int32_t count, uint32_t* written);

static int lines;

void text_print(const char* text)
{
    uint32_t length = 0, written;
    Piece piece;
    while (text[length])
        ++length;
    piece.data = text;
    piece.length = length;
    fd_write(1, &piece, 1, &written);
    lines += text[length - 1] == '\n';
}

void text_number(int number)
{
    char digits[12];
    int at = 11;
    digits[at] = '\0';
    do
        digits[--at] = (char)('0' + number % 10);
    while ((number /= 10) && at > 0);
    text_print(digits + at);
}

int text_lines(void)
{
    return lines;
}
