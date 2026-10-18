/* PS1 memory: the RAM arena, the scratchpad and the libapi "heap3" allocator.
 *
 * InitHeap3/malloc3/free3 are PSY-Q's libapi heap. PSY-Z only has them as
 * assembly, so this is a small first-fit allocator over the region the game
 * hands to InitHeap3 (inside the arena, at the retail address). Keeping the
 * PS1's heap size means running out of memory happens where it did on the PS1
 * instead of being hidden by the host allocator. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dw1_native.h"

unsigned char dw1_ps1_ram[DW1_PS1_RAM_SIZE] __attribute__((aligned(16)));
unsigned char dw1_scratchpad[DW1_SCRATCHPAD_SIZE] __attribute__((aligned(16)));

typedef struct HeapBlock {
    size_t size;            /* payload size in bytes */
    struct HeapBlock *next; /* next block in address order */
    int used;
} HeapBlock;

#define HEAP_ALIGN 16
#define HEADER_SIZE ((sizeof(HeapBlock) + HEAP_ALIGN - 1) & ~(size_t)(HEAP_ALIGN - 1))

static HeapBlock *heap_first;

static size_t align_up(size_t n)
{
    return (n + HEAP_ALIGN - 1) & ~(size_t)(HEAP_ALIGN - 1);
}

void InitHeap3(unsigned long *head, unsigned long size)
{
    unsigned char *base = (unsigned char *)head;
    size_t skip = align_up((size_t)base) - (size_t)base;

    if (size <= skip + HEADER_SIZE) {
        heap_first = NULL;
        return;
    }
    heap_first = (HeapBlock *)(base + skip);
    heap_first->size = (size - skip - HEADER_SIZE) & ~(size_t)(HEAP_ALIGN - 1);
    heap_first->next = NULL;
    heap_first->used = 0;
}

void *malloc3(size_t size)
{
    HeapBlock *b;

    size = align_up(size ? size : 1);
    for (b = heap_first; b != NULL; b = b->next) {
        if (b->used || b->size < size)
            continue;
        if (b->size >= size + HEADER_SIZE + HEAP_ALIGN) {
            HeapBlock *rest = (HeapBlock *)((unsigned char *)b + HEADER_SIZE + size);
            rest->size = b->size - size - HEADER_SIZE;
            rest->next = b->next;
            rest->used = 0;
            b->next = rest;
            b->size = size;
        }
        b->used = 1;
        return (unsigned char *)b + HEADER_SIZE;
    }
    fprintf(stderr, "dw1: malloc3(%u) failed: PS1 heap exhausted\n", (unsigned)size);
    return NULL;
}

void free3(void *ptr)
{
    HeapBlock *b, *prev = NULL;

    if (ptr == NULL)
        return;
    for (b = heap_first; b != NULL; prev = b, b = b->next) {
        if ((unsigned char *)b + HEADER_SIZE != ptr)
            continue;
        b->used = 0;
        /* merge with the following free block, then with the previous one */
        if (b->next != NULL && !b->next->used) {
            b->size += HEADER_SIZE + b->next->size;
            b->next = b->next->next;
        }
        if (prev != NULL && !prev->used) {
            prev->size += HEADER_SIZE + b->size;
            prev->next = b->next;
        }
        return;
    }
    fprintf(stderr, "dw1: free3(%p): not a heap block\n", ptr);
}

void *calloc3(size_t n, size_t size)
{
    void *p = malloc3(n * size);
    if (p != NULL)
        memset(p, 0, n * size);
    return p;
}

void *realloc3(void *ptr, size_t size)
{
    HeapBlock *b;
    void *p;

    if (ptr == NULL)
        return malloc3(size);
    b = (HeapBlock *)((unsigned char *)ptr - HEADER_SIZE);
    if (b->size >= size)
        return ptr;
    p = malloc3(size);
    if (p != NULL) {
        memcpy(p, ptr, b->size);
        free3(ptr);
    }
    return p;
}
