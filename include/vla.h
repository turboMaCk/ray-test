#include <stdio.h>
#include <stdlib.h>

// posix only:
#include <sys/mman.h>
#include <unistd.h>

#ifndef VLA_HEADER
#define VLA_HEADER

#define VLA_MB(mb) ((size_t)(mb) * 1024 * 1024)

typedef struct {
    size_t reserved;
    size_t comitted;
    size_t used;
    void *base;
} Vla;

Vla vla_init(size_t size);
void *vla_alloc(Vla *arena, size_t size, size_t alignment);
void vla_grow_to(Vla *arena, size_t size);
void vla_reset(Vla *arena);
void vla_destroy(Vla *arena);

#define vla_alloc_struct(arena, type) (type *)arena_alloc(arena, sizeof(type), alignof(type))
#define vla_alloc_array(arena, count, type) (type *)arena_alloc(arena, (count) * sizeof(type), alignof(type))
#define vla_reserve_char(arena) (char *)arena_alloc(arena, sizeof(char), alignof(char))

#endif // VLA_HEADER

#ifdef VLA_IMPLEMENTATION
#undef VLA_IMPLEMENTATION

#define vla__max(a,b) ((a) > (b) ? (a) : (b))
#define vla__panic(expression)                                                 \
    do {                                                                       \
        fprintf(stderr, "%s:%d Panic: %s\n", __FILE__, __LINE__, #expression); \
        __builtin_trap();                                                      \
        exit(-1);                                                              \
    } while(0)

long VLA__PAGE_SIZE;

long vla__init_page_size(void) {
    VLA__PAGE_SIZE = sysconf(_SC_PAGESIZE);
    return VLA__PAGE_SIZE;
}

#define PAGE_SIZE (VLA__PAGE_SIZE || vla__init_page_size())

static void *vla__reserve_mem(size_t len) {
    return mmap(
                NULL,                             // Starting address (NULL lets OS choose)
                vla__max(len, (size_t)PAGE_SIZE), // Size of region (1 GB)
                PROT_NONE,                        // Initial protection: No access (reserves space only)
                MAP_PRIVATE | MAP_ANONYMOUS,      // Private anonymous memory
                -1, 0                             // File descriptor and offset (not used for anonymous)
                );
}

static int vla__commit_mem(void* addr, size_t len) {
    return mprotect(
             addr,                       // Start address of the block to commit
             len,                        // Size of the block to commit
             PROT_READ | PROT_WRITE      // New protection: Read/Write
             );

}

static int vla__release_mem(void *addr, size_t len) {
    // Releases the reserved virtual address range back to the OS.
    return munmap(addr, len);
}

static int vla__uncommit_mem(void *addr, size_t len) {
    return munmap(addr, len);
}

static inline size_t align_up(size_t value, size_t alignment) {
    // A - 1 gives us a mask of all ones below the alignment bit (e.g., if A=8, A-1=7, binary 0111)
    size_t align_mask = alignment - 1;

    // 1. Add (A - 1): This ensures that if the value is not already aligned,
    //    it crosses over into the next alignment boundary. If it is aligned, it stays in the same block.
    // 2. Bitwise NOT (~): Flips the mask (e.g., if A=8, ~(A-1) = ...11111000)
    // 3. Bitwise AND (&): Clears all the low-order bits up to the alignment boundary,
    //    effectively rounding the result down to the nearest multiple of A.
    return (value + align_mask) & (~align_mask);
}

Vla vla_init(size_t size) {
    Vla arena = {0};
    arena.base = vla__reserve_mem(size);

    if (!arena.base)
        vla__panic("ERROR: Can't allocate memory for arena\n");

    arena.reserved = size;

    return arena;
}

void *vla_alloc(Vla *arena, size_t size, size_t alignment) {
    void *current_ptr = (void *)((size_t)arena->base + arena->used);
    void *aligned_ptr = (void *) align_up((size_t)current_ptr, (size_t)alignment);
    size_t padding = (size_t)aligned_ptr - (size_t)current_ptr;
    size_t alloc_size = size + padding;

    // Check for out of bounds
    if (arena->used + alloc_size > arena->reserved)
        vla__panic("ERROR: Arena allocation failed\n");

    // Commit memory if necessary
    if (arena->used + alloc_size > arena->comitted) {
        size_t needed = arena->used + alloc_size - arena->comitted;
        size_t commit_size = align_up(needed, PAGE_SIZE);

        void *commit_ptr = (void *) ((size_t)arena->base + arena->comitted);

        arena->comitted += commit_size;

        if (arena->comitted > arena->reserved)
            vla__panic("ERROR: Arena commit out of bounds\n");

        if (vla__commit_mem(commit_ptr, commit_size) != 0)
            vla__panic("ERROR: Arena failed to commit new page(s)\n");
    }

    arena->used += alloc_size;
    return aligned_ptr;
}

void vla_grow_to(Vla *arena, size_t size) {
    if (size > arena->reserved)
        vla__panic("ERROR: Arena grow_to size exceeds reserved memory\n");

    if (size > arena->comitted) {
        size_t needed = size - arena->comitted;
        size_t commit_size = align_up(needed, PAGE_SIZE);

        void *commit_ptr = (void *) ((size_t)arena->base + arena->comitted);

        if (vla__commit_mem(commit_ptr, commit_size) != 0)
            vla__panic("ERROR: Arena failed to commit new page(s) during grow_to\n");

        arena->comitted += commit_size;

        if (arena->comitted > arena->reserved)
            vla__panic("ERROR: Arena grow_to commit out of bounds\n");
    }
}

inline void vla_reset(Vla *arena) {
    arena->used = 0;
}

void vla_destroy(Vla *arena) {
    // Check if any memory was actually reserved
    if (arena->reserved > 0 && arena->base != NULL) {
        // Call the platform specific release function (munmap)
        // munmap returns 0 on success, -1 on failure.
        if (vla__release_mem(arena->base, arena->reserved) == -1) {
             // In many production environments, failing to destroy is non-critical
             // as the process exit will clean up, but it should be reported.
             vla__panic("ERROR: Arena failed to release (munmap) reserved memory.\n");
        }
    }

    // Clear the structure to a defined, invalid state
    arena->base = NULL;
    arena->reserved = 0;
    arena->comitted = 0;
    arena->used = 0;
}

#endif // VLA_IMPLEMENTATION
