
#define MP_ALIGNMENT 32
#define MAX_ALLOC_BLOCK 4096

typedef struct mp_node_s {
    unsigned char *start;
    unsigned char *end;

    mp_node_s *next;
    int flag;
} mp_node_s;

typedef struct mp_large_s {
    mp_large_s *next;
    void *alloc;
} mp_large_s;

typedef struct mp_pool_s {
    size_t max;
    mp_node_s *current;
    mp_large_s *large;
    
    mp_node_s head[0];
} mp_pool_s;

// create pool, init
mp_pool_s* mp_create_pool(size_t size) {
    mp_pool_s *p;
    int ret = posix_memalign(&p, MP_ALIGNMENT, size + sizeof(mp_pool_s) + sizeof(mp_node_s));
    if (ret) {
        return NULL;
    }

    p->max = size < MAX_ALLOC_BLOCK ? size : MAX_ALLOC_BLOCK;
    p->current = p->head;

    p->large = NULL;

    p->head->start = (unsigned char *)p + sizeof(mp_pool_s) + sizeof(mp_node_s);
    p->head->end = p->head->start + size;

    p->flag = 0;

    return p;
}

// destroy pool
void mp_destroy_pool(mp_pool_s *pool) {
    mp_large_s *l;
    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    mp_node_s *n, *h = pool->head->next;
    while (h) {
        n = h->next;
        free(h);
        h = n;
    }
    
    free(pool);

}

// pmalloc/calloc

static void *mp_alloc_block(mp_pool_s *pool, size_t size) {
    mp_node_s *h = pool->head;
    size_t psize = (size_t)(h->end - (unsigned char*)h->head);
    unsigned char *m = NULL;

    int ret = posix_memalign(&m, MP_ALIGNMENT, psize + sizeof(mp_node_s));  // whether need sum sizeof(mp_node_s) ???
    if (ret) {
        return NULL;
    }

    mp_node_s *n = pool->current;
    mp_node_s *new_node = (mp_node_s *)m;

    new_node->next = pool->current;
    pool->current = new_node;

    pool->head = m + sizeof(mp_node_s); // it is OK? I think this line is no use.
    new_node->start = m + sizeof(mp_node_s);
    new_node->end = new_node->start + psize; // Is it psize? Should not be size?


    return new_node->start;

}

static void *mp_alloc_large(mp_pool_s *pool, size_t size) {
    void *p = NULL;
    int ret = posix_memalign(&p, MP_ALIGNMENT, size);

    mp_large_s *large;
    for (large = pool->large; large; large = large->next) {
        if (large->alloc == NULL) {
            large->alloc = p;
            return p;
        }
    }
}

void *mp_alloc(mp_pool_s *pool, size_t size) {
    if (size <= pool->max) {
        mp_node_s *p = pool->current;

        do {
            unsigned char *m = p->start;
            if (p->end - p->start >= size) {
                p->start += size;
                p->flag++;
                return m;
            }
            p = p->next;

        } while (p);


        return mp_alloc_block(pool, size);
    }

    return mp_alloc_large(pool, size);
}

// free
void mp_free(mp_pool_s *pool, void *p) {

}

