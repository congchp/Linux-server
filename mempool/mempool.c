#include <stdlib.h>


#define ALIGNMENT 8

#define mp_align(n, alignment) (((n)+(alignment-1)) & ~(alignment-1))
#define mp_align_ptr(p, alignment) (void *)((((size_t)p)+(alignment-1)) & ~(alignment-1))

typedef struct _mp_node_s {

    unsigned char *last; // 内存块中未分配内存的首地址
    unsigned char *end; // 内存块的尾地址

    struct _mp_node_s *next;

} mp_node_s;


typedef struct _mp_large_s {

    struct _mp_large_s *next;
    void *alloc;

} mp_large_s;


typedef struct _mp_pool_s {

    mp_node_s *small;
    mp_large_s *large;

    int size;

} mp_pool_s;

void *mp_malloc(mp_pool_s *pool, int size);

mp_pool_s *mp_create_pool(int size) {

    mp_pool_s *pool;

    int ret = posix_memalign((void **)&pool, ALIGNMENT, size + sizeof(mp_pool_s));
    if (ret) return NULL;

    pool->small = (mp_node_s *)(pool + 1);

    pool->small->last = (unsigned char *)(pool->small + 1);
    pool->small->end = (unsigned char *)pool + size + sizeof(mp_pool_s);
    pool->small->next = NULL;

    pool->large = NULL;

    return pool;

}

void mp_destroy_pool(mp_pool_s *pool) {

    mp_large_s *large;

    for (large = pool->large; large; large = large->next) {
        if (large->alloc) {
            free(large->alloc);
        }
    }

    mp_node_s *small, *next;

    for (small = pool->small; small;) {

        next = small->next;
        free(small);
        small = next;
    }

    free(pool);
}


static void *mp_malloc_large(mp_pool_s *pool, int size) {

    mp_large_s *large = (mp_large_s *)mp_malloc(pool, sizeof(mp_large_s));

    int ret = posix_memalign((void **)&large->alloc, ALIGNMENT,  size);
    if (ret) return NULL;

    return large->alloc;

}

static void *mp_malloc_small(mp_pool_s *pool, int size) {

    mp_node_s *node = NULL;
    int ret = posix_memalign((void **)&node, ALIGNMENT,  pool->size);
    if (ret) return NULL;

    node->next = pool->small;
    pool->small = node;

    node->last = (unsigned char *)node + sizeof(mp_node_s);
    node->end = (unsigned char *)node + pool->size;

    return node->last;


}

void *mp_malloc(mp_pool_s *pool, int size) {

    if (size < pool->size) {
        mp_node_s *node = pool->small;

        if (size < node->end - node->last) { // 需要考虑字节对齐
            unsigned char *m = node->last;
            node->last = m + size;
            return m;

        } else {
            return mp_malloc_small(pool, size);
        }
    } else {
        return mp_malloc_large(pool, size);
    }

}

void *mp_free(mp_pool_s *pool, void *p) {

    mp_large_s *large = NULL;
    for (large = pool->large; large; large = large->next) {
        if (large->alloc == ()p) {
            free(large->alloc);
            large->alloc = NULL;
            break;
        }
    }
}


int main() {



}
