# 什么是内存池

应用程序直接对使用的内存进行管理，避免频繁的从堆上申请释放内存，可以做到一次申请，多次分配。内存池主要是针对小块。

内存池管理的是堆内存。进程开始运行的时候，划分出一块内存

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638924969114-705b77ce-5b79-44a6-bded-dd082e3505f4.png)

# 为什么需要内存池？

对于服务器，如果不段有客户端连接，并且客户端不断发送消息，对于每个连接的每个消息，服务器都需要去malloc/free内存。如果服务器需要7*24运行，时间久了，就会产生很多内存碎片，没有整块，最后就有可能malloc比较大的内存的时候失败。最后进程就会coredump。

这种问题，出现了很难解，因为程序要运行很长时间才会出现。解决方案就是使用内存池。

频繁的malloc/free, 会造成哪些问题？

1. 不利于内存管理
2. 内存碎片

1. 出现内存泄漏，不容易查出具体位置。

# 内存池使用场景

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1634189593094-8f8835aa-8918-4e08-8ff8-5ad077c5f527.png)

典型的场景，服务器处理网络io。message较大的时候，recv、parse后，需要push到另一个thread进行处理。使用栈内存不合适，需要申请堆内存。recv的时候申请buffer，send后，释放buffer。每来一个消息，都需要申请、释放内存。

# 内存池的实现

## 内存池需要考虑哪些问题？

1. 分配
2. 回收

1. 扩展

## 内存池的演化

1. 最早的内存池雏形。

每次malloc内存后，加入到一个内存池链表中，free的时候不释放，只是修改flag标志，表示当前内存块是空闲的。一定程度解决内存碎片的问题。

存在的问题

1）内存块越分越小。

如果一个内存块64字节，现在应用程序需要54字节，那么剩余的10字节又会加入到内存池链表的尾部。就会造成出现很多小块内存，越分越小，出现很多没办法使用的小块内存。链表也会越来越长。





![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638928998187-4bac422c-7d0d-4447-87e9-50e43b1365e0.png)

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638928939131-03576a48-b194-481c-86f7-139dd3b022db.png)

1. 分配固定大小的不同的块

内存池内分别有16 byte，32byte...，这种固定大小的内存块。应用程序要申请内存，到相应大小的块中去获取。如果要申请大于1024 byte的内存，直接就申请一整块。这样就能够解决出现内存块越分越小的问题。

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638929909663-da82c0c2-ec52-4406-8809-6b487e8f4dbe.png)

1. 用hash table

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638930590617-b9738efa-6031-46bd-b94f-f2db886247b1.png)

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638931216849-a1477b35-386f-4080-a025-ee6924e48fea.png)

这种方法有什么缺点？

1） 查找速度慢， 

malloc的时候，需要查找空闲的内存块，即flag为0

free的时候需要查找，设置flag = 0.

free的时候，参数加上size，就可以在hash table中找到相应的slot。



解决方法：

a. 对于malloc时查找，可以将每一个slot中的内存块分为两个链表，一个是已使用的，一个是未使用的。

b. free时候的查找，可以用hash，rbtree。



2）内存存在浪费，会存在间隙。影响小块的回收。

小块有没有必要回收？小块内存回收是一个极其麻烦的事情。

如果一个连接对应一个内存池，连接的生命周期不会很长，可以不回收小块内存。

如果要使用全局内存池，可以使用jemalloc、tcmalloc。



![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638931828164-35b00071-a7fd-4c41-9c0f-ecf8ef8a7be6.png)

解决小块回收的方法：

对于16bytes的slot，下面挂的是4k的内存块， 即一开始就分配一个整块，每一个从这个内存块里面分配16bytes，如果4k用完了，接着再申请4k内存块，挂到链表上。



![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638932563680-f205a1d4-25e3-4130-b656-632e969e5ea5.png)

## 原理

对于一块4k的内存管理，通常有几种方法：

1. 伙伴算法，由整块划分成若干个小块，分为2048 + 1024 + 512 + 256 + 128 + 64 + 32 + 16 + 8 + 4 + 2 +1。这种不适合管理小块，适合管理大块。linux内核是通过这种方法对page进行管理的。
2. slab，一开始就划分好若干个小块。如2个512，2个256等。

对于1和2，我现在只是有个粗浅的了解，后面会看内核和nginx源码，补充详细的说明。

1. 我们实现的，是在特定场景下的内存池。

我们的实现比较粗犷，对于小块内存，没有单独回收，要回收统一回收，不单独回收一个块内的某个小块内存。对于大块，进行回收。

内存池里面保持两个list，一个是小块，一个是大块。小块大小固定是4k，大块用于大于4k的内存。如果申请小于4k的内存，直接从小块进行分配，一次申请好4k，之后在这个基础上进行分配；申请大于4k的内存，则使用大块，用多少申请多少，大块的大小不是固定的。

对于一个网络连接，我们create一个内存池，等到连接断开，destroy内存池。



![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1634190414136-2a4ba9de-086d-40b9-84d5-34dd166f3703.png)

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638939490142-b57c2892-d6d8-46d1-af73-25fc290c711d.png)

## 代码实现

```c
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
```

## 如何保证内存池线程安全？

加锁来保证内存池线程安全，锁的粒度需要把握好。

如果是单线程reactor，或者没个线程一个reactor，也就是一个连接只在一个线程处理，不需要考虑加锁，一个连接一个线程池，是线程安全的。

## 开源内存池

如果使用全局内存池，可以使用开源的内存池。

1. jemalloc
2. tcmalloc

使用的时候，加上一个宏定义，不需要改源代码，malloc/free就被hook住，使用jemalloc/tcmalloc的函数。