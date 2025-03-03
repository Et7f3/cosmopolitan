#ifndef COSMOPOLITAN_LIBC_MEM_HOOK_HOOK_H_
#define COSMOPOLITAN_LIBC_MEM_HOOK_HOOK_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern void (*hook_free)(void *);
extern void *(*hook_malloc)(size_t);
extern void *(*hook_calloc)(size_t, size_t);
extern void *(*hook_memalign)(size_t, size_t);
extern void *(*hook_realloc)(void *, size_t);
extern void *(*hook_realloc_in_place)(void *, size_t);
extern void *(*hook_valloc)(size_t);
extern void *(*hook_pvalloc)(size_t);
extern int (*hook_malloc_trim)(size_t);
extern size_t (*hook_malloc_usable_size)(const void *);
extern size_t (*hook_bulk_free)(void *[], size_t);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_MEM_HOOK_HOOK_H_ */
