#include "MSL_Common/alloc.h"
#include "MSL_Common/size_def.h"
#include <string.h>

/* Pool size table for fixed-size block pools */
const unsigned long fix_pool_sizes[6] = { 8, 16, 32, 64, 128, 256 };

typedef struct SubBlock {
    struct SubBlock *next;
    size_t size;
} SubBlock;

typedef struct Block {
    struct Block *next;
    SubBlock *free_list;
    unsigned char *mem;
    size_t size;
    size_t used;
} Block;

typedef struct Pool {
    Block *var_block;
    Block *fix_blocks[6];
    size_t total_size;
    size_t free_size;
    int initialized;
    int pad[3];
} Pool;

static Pool get_malloc_pool__Fv_protopool;
static int get_malloc_pool__Fv_init = 0;

static Pool *get_malloc_pool(void) {
    return &get_malloc_pool__Fv_protopool;
}

/* Internal helpers - stubs */
SubBlock *Block_subBlock(Block *block, size_t size) {
    (void)block;
    (void)size;
    return NULL;
}

void Block_link(Block *block, SubBlock *sub) {
    (void)block;
    (void)sub;
}

void SubBlock_merge_next(SubBlock *sub) {
    (void)sub;
}

SubBlock *allocate_from_var_pools(Pool *pool, size_t size) {
    (void)pool;
    (void)size;
    return NULL;
}

SubBlock *soft_allocate_from_var_pools(Pool *pool, size_t size) {
    (void)pool;
    (void)size;
    return NULL;
}

void FixBlock_construct(Block *block, size_t elem_size, size_t count) {
    (void)block;
    (void)elem_size;
    (void)count;
}

SubBlock *allocate_from_fixed_pools(Pool *pool, size_t size) {
    (void)pool;
    (void)size;
    return NULL;
}

void deallocate_from_fixed_pools(Pool *pool, SubBlock *sub) {
    (void)pool;
    (void)sub;
}

void *__pool_allocate_resize(Pool *pool, void *ptr, size_t old_size, size_t new_size) {
    (void)pool;
    (void)ptr;
    (void)old_size;
    (void)new_size;
    return NULL;
}

void *malloc(size_t size) {
    SubBlock *sub;
    Pool *pool = get_malloc_pool();
    if (size == 0) return NULL;
    sub = allocate_from_var_pools(pool, size);
    if (sub == NULL) sub = allocate_from_fixed_pools(pool, size);
    if (sub == NULL) return NULL;
    return (void *)((unsigned char *)sub + sizeof(size_t));
}

void free(void *ptr) {
    if (ptr == NULL) return;
    deallocate_from_fixed_pools(get_malloc_pool(), (SubBlock *)((unsigned char *)ptr - sizeof(size_t)));
}

void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }
    return __pool_allocate_resize(get_malloc_pool(), ptr, 0, size);
}

void *calloc(size_t nitems, size_t size) {
    size_t total = nitems * size;
    void *ptr = malloc(total);
    if (ptr != NULL) memset(ptr, 0, total);
    return ptr;
}
