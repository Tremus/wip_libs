#pragma once
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
#include <stddef.h>
#include <stdint.h>
#include <xhl/debug.h>

typedef struct LinkedArena
{
    size_t capacity;
    size_t size;

    size_t _padding;

    struct LinkedArena* next;
} LinkedArena;

LinkedArena* linked_arena_create(size_t min_cap);
LinkedArena* linked_arena_create_ex(void* hint, size_t cap);
void         linked_arena_destroy(LinkedArena* arena);
void*        linked_arena_alloc_aligned(LinkedArena* arena, size_t size, size_t alignment);
static void* linked_arena_alloc(LinkedArena* arena, size_t size) { return linked_arena_alloc_aligned(arena, size, 32); }
void* linked_arena_alloc_clear(LinkedArena* arena, size_t size); // Additionally zeros returned memory for convenience
void  linked_arena_release(LinkedArena* arena, const void* const ptr);
void  linked_arena_clear(LinkedArena* arena);
void  linked_arena_prune(LinkedArena* arena); // Destroy unused arenas. Won't destroy first item

#ifdef NDEBUG
#define LINKED_ARENA_LEAK_DETECT_BEGIN(arena)
#define LINKED_ARENA_LEAK_DETECT_END(arena)
#else
#define LINKED_ARENA_LEAK_DETECT_BEGIN(arena) size_t _arena_size = arena->size;
#define LINKED_ARENA_LEAK_DETECT_END(arena)   xassert(_arena_size == arena->size);
#endif