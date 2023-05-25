#pragma once

#include <cstdio>

void hex_dump(const uint8_t* data, size_t size)
{
    size_t i = 0;

    for (; i < size; ++i)
    {
        printf("%02x ", data[i]);

        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    if (i % 16 != 0) {
        printf("\n");
    }
}
