#pragma once

#define KERNEL_HEAP_START_VADDR 0x120000000

#include <stddef.h> // for size_t

typedef struct allocation
{
   size_t size;
   struct allocation *prev;
   struct allocation *next;
} allocation_t;


// kmalloc may return more than the number of requested bytes.
// That's okay, but should never be relied upon to do so.
void *kmalloc(size_t bytes);
void *kzalloc(size_t bytes);
void *kcalloc(size_t bytes);
void *common_alloc(size_t bytes, int do_zero, int request_contiguous);

void kfree(void *);

// initialize the heap
void init_kernel_heap(void);

// cleaup necessary to call occasionally for good long-term performance
void coalesce_free_list(void);
void heap_shrink(void);


// helper, kernel, and debug functions
void heap_print_free_list(void);
void heap_size_of_allocation(void *allocation);
