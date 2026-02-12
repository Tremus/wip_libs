#pragma once
#include <xhl/array2.h>

typedef struct Stringpool
{
    char* buffer; // xarray
} Stringpool;

static size_t stringpool_push(Stringpool* pool, const char* str, size_t len)
{
    size_t offset = xarr_len(pool->buffer);
    xassert((offset & 15) == 0);

    size_t nextlen = offset + len + 1;       // +1 for '\0' byte
    nextlen        = (nextlen + 0xf) & ~0xf; // align up by 16 bytes
    xarr_setlen(pool->buffer, nextlen);

    memcpy(pool->buffer + offset, str, len);
    pool->buffer[offset + len] = '\0';

    return offset;
}