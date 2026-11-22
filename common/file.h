/* Reads a whole file into memory: how every example gets its program. */
#ifndef EXAMPLES_FILE_H
#define EXAMPLES_FILE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t* read_file(const char* path, uint32_t* size)
{
    FILE* file = fopen(path, "rb");
    uint8_t* bytes = NULL;
    long length;
    if (!file)
        return NULL;
    if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) > 0 && fseek(file, 0, SEEK_SET) == 0 &&
        (bytes = (uint8_t*)malloc((size_t)length)) && fread(bytes, 1, (size_t)length, file) != (size_t)length)
    {
        free(bytes);
        bytes = NULL;
    }
    if (bytes)
        *size = (uint32_t)length;
    fclose(file);
    return bytes;
}

#endif
