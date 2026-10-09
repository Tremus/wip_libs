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
    xassert(alignment > 0);
    xassert((alignment & 7) == 0); // Must be aligned to 8 bytes
    uint64_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

LinkedArena* linked_arena_create(size_t cap)
{
    LinkedArena* arena = NULL;

    if (cap == 0)
    {
        // 128mb ADDRESS SPACE allocated, not actually paged
        cap = (1024 * 1024 * 128);
    }
    xassert(cap > sizeof(LinkedArena));

    void*        hint      = NULL;
    const size_t alignment = 4096;
    // xvalloc_info(&alignment, 0);
    size_t alloc_size = linked_arena_align(cap, alignment);

    arena = xvalloc(hint, alloc_size);
    xassert(arena);
    arena->capacity = alloc_size - sizeof(LinkedArena);
    arena->current  = arena;
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

void* linked_arena_alloc_aligned(LinkedArena* head, size_t size, size_t alignment)
{
    xassert(size > 0);
    xassert(head->current);
    void* ptr = NULL;

    size = linked_arena_align(size, alignment);

    LinkedArena* arena = head->current;
    while (ptr == NULL)
    {
        xassert(arena->capacity >= arena->size);
        size_t remaining = arena->capacity - arena->size;
        if (size <= remaining)
        {
            ptr          = arena + 1;
            ptr          = (char*)ptr + arena->size;
            arena->size += size;
        }
        else
        {
            arena->size = arena->capacity; // max out arena so it's really obvious this arena is full

            if (arena->next == NULL) // Reached the end of the list
            {
                size_t alloc_size = size > arena->capacity ? (size + sizeof(LinkedArena)) : arena->capacity;
                arena->next       = linked_arena_create(alloc_size);
            }

            arena = arena->next;
        }
        xassert(arena->next != arena);
    }
    head->current = arena;

    return ptr;
}

void* linked_arena_alloc_clear(LinkedArena* arena, size_t size)
{
    void* ptr = linked_arena_alloc(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void linked_arena_release(LinkedArena* head, const void* const ptr)
{
    LinkedArena* prev  = NULL;
    LinkedArena* arena = head;
    while (arena)
    {
        char* start = (char*)(arena + 1);
        char* end   = start + arena->size;
        // Inclusive end: ptr may be a top-of-stack tag from linked_arena_get_top() that sits exactly at the end of a
        // full arena, in which case this arena keeps its size and everything further down the chain is released
        if ((char*)ptr >= start && (char*)ptr <= end)
        {
            size_t alloc_size = (size_t)(end - (char*)ptr);
            xassert(arena->size >= alloc_size);
            arena->size -= alloc_size;

            // Linked list items further down the chain may still have allocations
            for (LinkedArena* n = arena->next; n; n = n->next)
                n->size = 0;

            // If this arena is now empty, the end of the stack is the (full) previous arena. Never leave 'current'
            // pointing at an empty arena, else linked_arena_prune() could destroy it
            head->current = (arena->size == 0 && prev) ? prev : arena;
            return;
        }
        prev  = arena;
        arena = arena->next;
    }
    // ptr not found!!!
    xassert(0);
}

void* linked_arena_resize_aligned(LinkedArena* arena, void* ptr, size_t old_size, size_t new_size, size_t alignment)
{
    xassert(ptr);
    xassert(new_size > 0);
    xassert(new_size > old_size);

    size_t old_aligned = linked_arena_align(old_size, alignment);
    size_t new_aligned = linked_arena_align(new_size, alignment);

    if (new_aligned <= old_aligned)
        return ptr;

    LinkedArena* top_arena = arena->current;

    char* mem_begin = (char*)(top_arena + 1);
    char* mem_end   = mem_begin + top_arena->capacity;
    if ((char*)ptr >= mem_begin && (char*)ptr < mem_end)
    {
        // pointer belongs to this arena
        char* old_val = (char*)ptr + old_aligned;
        char* top     = mem_begin + top_arena->size;
        if (old_val == top)
        {
            // Pointer passed to function matches the last allocated ptr
            size_t diff = new_aligned - old_aligned;
            if (top_arena->size + diff <= top_arena->capacity)
            {
                top_arena->size += diff;
                return ptr;
            }
        }
    }

    void* new_ptr = linked_arena_alloc_aligned(arena, new_size, alignment);
    memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

void linked_arena_clear(LinkedArena* arena)
{
    arena->current = arena;
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
            linked_arena_destroy(n1);
            arena->next = NULL;
        }
        arena = arena->next;
    }
}

void* linked_arena_get_top(const LinkedArena* arena)
{
    const LinkedArena* current = arena->current;
    return (char*)(current + 1) + current->size;
}
