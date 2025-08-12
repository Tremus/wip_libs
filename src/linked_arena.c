/*
MIT No Attribution

Copyright 2025 Tré Dudman

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "linked_arena.h"

#include <string.h>
#include <xhl/alloc.h>
#include <xhl/debug.h>

static inline uint64_t linked_arena_align(uint64_t value, uint64_t alignment)
{
    uint64_t mask = alignment - 1;
    uint64_t inc  = (alignment - (value & mask)) & mask;
    return value + inc;
}

LinkedArena* linked_arena_create(size_t init_cap)
{
    xassert(init_cap > 0);
    LinkedArena* arena = NULL;

    size_t alloc_size = init_cap + sizeof(LinkedArena);
    alloc_size        = linked_arena_align(alloc_size, 4096);

    arena = xvalloc(0, alloc_size);
    xassert(arena);
    arena->capacity = alloc_size - sizeof(LinkedArena);
    xassert(arena->capacity > 0);

    return arena;
}

void linked_arena_destroy(LinkedArena* arena)
{
    xassert(arena);
    while (arena)
    {
        xassert(arena->capacity >= arena->size);
        LinkedArena* next = arena->next;

        size_t alloc_size = arena->capacity + sizeof(LinkedArena);
        xvfree(arena, alloc_size);

        arena = next;
    }
}

void* linked_arena_alloc(LinkedArena* arena, size_t size)
{
    xassert(size > 0);
    void* ptr = NULL;

    size = linked_arena_align(size, 32);

    while (ptr == NULL)
    {
        xassert(arena->capacity >= arena->size);
        size_t remaining = arena->capacity - arena->size;
        if (size <= remaining)
        {
            ptr          = arena + 1;
            ptr         += arena->size;
            arena->size += size;
        }
        else
        {
            if (arena->next == NULL) // Reached the end of the list
            {
                size_t alloc_size = size > arena->capacity ? size : arena->capacity;
                arena->next       = linked_arena_create(alloc_size);
            }

            arena = arena->next;
        }
        xassert(arena->next != arena);
    }

    return ptr;
}

void* linked_arena_alloc_clear(LinkedArena* arena, size_t size)
{
    void* ptr = linked_arena_alloc(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void linked_arena_release(LinkedArena* arena, const void* const ptr)
{
    while (arena)
    {
        void* start = arena + 1;
        void* end   = start + arena->size;
        if (ptr >= start && ptr < end)
        {
            size_t alloc_size = end - ptr;
            xassert(arena->size >= alloc_size);
            arena->size -= alloc_size;
            return;
        }
        arena = arena->next;
    }
}

void linked_arena_clear(LinkedArena* arena)
{
    while (arena)
    {
        xassert(arena->capacity >= arena->size);
        arena->size = 0;
        arena       = arena->next;
    }
}

void linked_arena_prune(LinkedArena* arena)
{
    while (arena)
    {
        xassert(arena->capacity >= arena->size);

        LinkedArena* n1 = arena->next;

        if (n1 && n1->size == 0)
        {
            arena->next = n1->next;

            size_t alloc_size = n1->capacity + sizeof(LinkedArena);
            xvfree(n1, alloc_size);
        }
        arena = arena->next;
    }
}
