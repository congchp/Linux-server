# 存储结构

redis对外主要提供5种数据类型，string、list、set、zset、hash。对于这些数据类型，最终主要由下图数据结构进行存储。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643329624515-44ae7f16-6b07-4eed-bedb-b6a5940340eb.png)



# 存储转换

redis是一个内存数据库，非常注意内存的使用。对于每种数据类型，在不同的条件下，redis使用不同的数据结构进行存储。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643330099813-f7583331-bd4f-4765-a1eb-09215074d0ba.png)



redis有如下encoding方式，可以通过`object encoding key`命令查看实际编码方式。

```c
/* Objects encoding. Some kind of objects like Strings and Hashes can be
 * internally represented in multiple ways. The 'encoding' field of the object
 * is set to one of this fields for this object. */
#define OBJ_ENCODING_RAW 0     /* Raw representation */
#define OBJ_ENCODING_INT 1     /* Encoded as integer */
#define OBJ_ENCODING_HT 2      /* Encoded as hash table */
#define OBJ_ENCODING_ZIPMAP 3  /* Encoded as zipmap */
#define OBJ_ENCODING_LINKEDLIST 4 /* No longer used: old list encoding. */
#define OBJ_ENCODING_ZIPLIST 5 /* Encoded as ziplist */
#define OBJ_ENCODING_INTSET 6  /* Encoded as intset */
#define OBJ_ENCODING_SKIPLIST 7  /* Encoded as skiplist */
#define OBJ_ENCODING_EMBSTR 8  /* Embedded sds string encoding */
#define OBJ_ENCODING_QUICKLIST 9 /* Encoded as linked list of ziplists */
#define OBJ_ENCODING_STREAM 10 /* Encoded as a radix tree of listpacks */
```



比如下面的代码，就是对于hash类型的数据，当`字符串长度大于64`时，将ziplist转成dict。其他的转换都有类似的check。

```c
/* Check the length of a number of objects to see if we need to convert a
 * ziplist to a real hash. Note that we only check string encoded objects
 * as their string length can be queried in constant time. */
void hashTypeTryConversion(robj *o, robj **argv, int start, int end) {
    int i;

    if (o->encoding != OBJ_ENCODING_ZIPLIST) return;

    for (i = start; i <= end; i++) {
        if (sdsEncodedObject(argv[i]) &&
            sdslen(argv[i]->ptr) > server.hash_max_ziplist_value)
        {
            hashTypeConvert(o, OBJ_ENCODING_HT);
            break;
        }
    }
}
```



# 字典实现

redis db key-value的组织是通过字典来实现的。

```c
/* Redis database representation. There are multiple databases identified
 * by integers from 0 (the default database) up to the max configured
 * database. The database number is the 'id' field in the structure. */
typedef struct redisDb {
    dict *dict;                 /* The keyspace for this DB */
    dict *expires;              /* Timeout of keys with a timeout set */
    dict *blocking_keys;        /* Keys with clients waiting for data (BLPOP)*/
    dict *ready_keys;           /* Blocked keys that received a PUSH */
    dict *watched_keys;         /* WATCHED keys for MULTI/EXEC CAS */
    int id;                     /* Database ID */
    long long avg_ttl;          /* Average TTL, just for stats */
    unsigned long expires_cursor; /* Cursor of the active expire cycle. */
    list *defrag_later;         /* List of key names to attempt to defrag one by one, gradually. */
} redisDb;
```

hash结构当`节点数量大于512或者字符床长度大于64`时，使用字典进行存储。

```shell
# Hashes are encoded using a memory efficient data structure when they have a
# small number of entries, and the biggest entry does not exceed a given
# threshold. These thresholds can be configured using the following directives.
hash-max-ziplist-entries 512
hash-max-ziplist-value 64
```

## 数据结构

```c
typedef struct dictEntry {
    void *key;
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    struct dictEntry *next;
} dictEntry;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
typedef struct dictht {
    dictEntry **table;
    unsigned long size; // 数组大小, size初始值为4，必须要是2^n
    unsigned long sizemask; // size-1，把对size的取余操作转换成对sizemask的按位与(&)操作
    unsigned long used; // 当前数组中key的数量
} dictht;

typedef struct dict {
    dictType *type;
    void *privdata;
    dictht ht[2];
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
    int16_t pauserehash; /* If >0 rehashing is paused (<0 indicates coding error) */
} dict;
```

一个key-value是怎么确定存入hash table的哪个位置的？

```
hash(key) % size
```

1. key经过hash函数运算得到64位整数；
2. 再对数组大小进行取余，就可以确定存入hash table的哪个槽位了。



**整数对2^n取余可以转化为位运算，即对(2^n - 1)进行按位与操作，所以hash table的size必须要是2^n。**

## hash冲突

随着hash table中存入数据量的增加，肯定会有多个key存入hash table的同一个槽位中，这样就会产生冲突。redis使用拉链法解决冲突，即每个槽位都使用一个链表存储key。随着数据量的增大，链表会越来越长。

使用负载因子来描述hash table整体的冲突情况。

负载因子 = `used` /` size`， `used`是当前数组中key的数量，`size`是数组的大小。负载因子越小，冲突越小；负载因子越大，冲突越大。

下面例子中，redis的负载因子是1.

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643339838797-ac59b1af-c52d-4e14-8363-bf48fce4f108.png)

## 扩容

如果负载因子太大，即hash冲突严重，redis就会考虑扩容。

如果负载因子 > 1, 则会发生扩容，扩容的规则是翻倍；

但如果有活跃子进程，即有fork的进程，正在进行持久化操作的时候，即使负载因子大于1，也不会进行过扩容，防止内存过多的进行写时复制；但如果此时负载因子 > 5, 索引效率大大降低，则会进行扩容。这里涉及fork进程使用的写时复制的原理。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643186560497-66d29249-263a-41c4-a766-61334ff0f4f4.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643186842739-f265a878-3a06-4aa2-b75d-b5b2b64ed353.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643346641267-b216fa78-1304-45e5-8e44-55553a0dc94e.png)



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643187893244-f3a4c197-59f7-486c-8c54-13fd7c6946cf.png)



以翻倍的形式进行扩容， 扩容后的size必须是2n, 并且需要大于used。



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643348290746-459066a7-81c3-4860-b5ce-bed985cd5d88.png)

## 写时复制(copy on write)

写时复制的核心思想：只有在不得不复制数据内容的时候，才去复制数据内容。

redis主进程fork(在rdb、aof rewrite、aof-rdb混用的情况下)出一个子进程做持久化的操作，父进程和子进程共用同一块内存空间，内存会被设置成只读，父进程继续处理命令，子进程做持久化。这时候如果有写命令，那么会产生一个页错误(page-fault中断)，会对所在页进行copy，父子进程各一份，这时候父进程就可以进行写操作了。这个就叫做copy on write机制。 

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643347815648-88d98746-a98c-41d3-a9d2-f2c3c848d654.png)



## 缩容

缩容的条件是什么？

delta < 1，不能马上进行缩容，避免频繁扩容缩容。

等没有活跃子进程，并且负载因子小于0.1的时候，可以进行缩容。在定时任务`databasesCron`中进行缩容。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643188644958-c651e632-d315-4365-8a32-ea5551fe7752.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643188730457-6648c612-26a1-4fcc-ba67-dc18ac910672.png)



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643348514306-fd326af7-cb11-4b9a-aec8-cd1203d7a7d0.png)

# rehash

`dict`结构中为什么有两个`dictht`呢？

`ht[0]`是用来存储key-value的，`ht[1]`是用来进行rehash的。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643348894474-ddbf79f3-6a58-4408-bedd-926c66b160bb.png)



当`hashtable`中的元素过多的时候，不能一次性`rehash`到`ht[1]`；这样会长期占用 redis，导致其他命令得不到响应；所以需要使用渐进式`rehash`；



rehash步骤：

将`ht[0]`中的元素重新经过hash函数生成64位整数，再对`ht[1]`的长度进行取余，从而映射到`ht[1]` ；



渐进式规则：

1. 1. 分治的思想，将`rehash`分到之后的每步增删改查的操作当中；
   2. 在定时器中，最大执行一毫秒`rehash`；每次`rehash`100个数组槽位；



`dictRehash`， 以数组的槽位为单位进行挪动，把该槽位链表中所有的key进行rehash。需要重新计算key的hash，并对新的数组大小进行取余，因为数组长度发生变化了。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643244341797-7956a7f8-120b-4e91-8170-fff786a3617a.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643244556119-43f53615-5f9e-4e75-9a55-e9e74d876c12.png)



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643244918074-688d4f3b-1098-4304-920c-030b6b813063.png)



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643244987834-120aeec2-756c-4046-80fc-e11afda44751.png)



处于渐进式rehash阶段时，是否会发生扩容缩容？

不会！

# scan

```shell
scan cursor [MATCH pattern] [COUNT 1 count] [TYPE type]
```

redis对于命令的处理，即网络io, 是单线程的，如果有上百万个key，使用keys这样的命令，会进行遍历，时间复杂度是O(N), redis server处理的时间就会很长，会导致暂时无法处理其他命令。生产环境，是坚决不能使用keys命令的。
	所以，在redis 2.8版本，redis提供了scan命令, 通过cursor的方式进行遍历，对于每一次遍历做了count的限制，不会造成redis server长时间的阻塞。
	scan命令的返回值是一个包含两个元素的数组，第一个元素是用于下一次迭代的新cursor，第二个元素则为一个数组，里面包含已经被遍历过的key。
	以0作为cursor开始一次新的迭代，默认的count是10，一直调用scan命令，直到命令返回cursor 0，表示一次遍历完成。
	scan遍历是顺序是什么样的？redis 的dict中使用的hash table，初始size是4，其实就是遍历这个hashtable里面存储的key。以size是4为例，遍历是顺序为0->2->1->3; size为8时，遍历顺序则为0->4->2->6->1->5->3->7。为什么顺序不是0->1->2->3...这样的呢？主要是考虑rehash情况下，遍历**不重复不遗漏**。
**遍历的算法使用的是高位加1，向低位进位。**

采用高位进位加法的遍历顺序， rehash 后的槽位在遍历顺序上是相邻的；

遍历目标是：不重复，不遗漏 ;



```plain
        /* Increment the reverse cursor */
        v = rev(v);
        v++;
        v = rev(v);
```



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643351969805-0a4dc84d-e17e-48ea-9bf0-398d353aead6.png)



对于连续两次缩容的情况，有可能会出现重复。



**扩容的条件是什么？**

**缩容的条件是什么？**

**负载因子决定扩容和缩容，也需要考虑是否正在进行持久化。**

**怎么进行渐进式rehash？**

#  skiplist

redis中对于一些数据类型，比如list, set, zset, 在数据量小的时候，是一种存储类型；在数据量大的时候，会变成另一种类型存储。对于zset，在集合中元素小于128时，使用的是ziplist，大于128使用的是skiplist。也就是说，对于大量数据，skiplist的表现要更好，增删改查的时间复杂度大概率为O(log2N)。 



```plain
zset-max-ziplist-entries 128
zset-max-ziplist-value 64
```



## 理想跳表

理想跳表，每隔一个节点，增加一个层级。理想跳表增加，删除节点的时候，维护结构比较困难。

skiplist本质是一个多层有序双向链表，加上了level的概念，构建了多层级的链表。理想skiplist，第一层是一个完整的链表；第二层有一半的节点，对于第一层来说，有一半的节点有第二层，每隔一个节点有第二层；依次构建第三层、第四层...。对于理想skiplist，每一次增加或者删除节点，都需要去维护理想跳表结构，维护理想跳表结构是非常复杂的运算。考虑用概率的方法来进行优化；从每一个节点出发，每增加一个节点都有 1/2 的概率增加一个层级， 1/4 的概率增加两个层级， 1/8 的概率增加3个层级，以此类推；经过证明，当数据量足够大（256）时，通过概率构造的跳表趋向于理想跳表，并且此时如果删除节点，无需重构跳表结构，此时依然趋向于理想跳表；此时时间复杂度为(1 - 1/n^c)*O(log2n)；

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643361108034-0e6bb26e-b2e7-4f4a-b589-077edac7fc77.png)

## redis跳表

redis中为每个节点随机出一个层数，有25%的概率有第二层，有25%*25%的概率有第三层..., 这样在数据量很大的情况下，就趋近于理想跳表，在增删的时候，不需要去维护理想跳表结构。



```c
#define ZSKIPLIST_MAXLEVEL 32 /* Should be enough for 2^64 elements */
#define ZSKIPLIST_P 0.25      /* Skiplist P = 1/4 */

/* ZSETs use a specialized version of Skiplists */
typedef struct zskiplistNode {
    sds ele; // member
    double score; // score
    struct zskiplistNode *backward;
    struct zskiplistLevel {
        struct zskiplistNode *forward; // 0层是双向链表，上面的层级是单向链表
        unsigned long span;
    } level[];
} zskiplistNode;

typedef struct zskiplist {
    struct zskiplistNode *header, *tail;
    unsigned long length; // zcard
    int level;
} zskiplist;

typedef struct zset {
    dict *dict;
    zskiplist *zsl;
} zset;
```



```c
/* Returns a random level for the new skiplist node we are going to create.
 * The return value of this function is between 1 and ZSKIPLIST_MAXLEVEL
 * (both inclusive), with a powerlaw-alike distribution where higher
 * levels are less likely to be returned. */
int zslRandomLevel(void) {
    int level = 1;
    while ((random()&0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
        level += 1;
    return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}
```



## zset结构图

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643377234828-1622026f-1fbb-4720-a39f-4a851d428716.png)



## skiplist与rbtree对比

结构的区别：skiplist是多层有序链表；rbtree是二叉树
增删改查时间复杂度：skiplist大概率O(logN)；rbtree O(logN)
范围查找zrange：skiplist O(logN); rbtree log(N)*O(logN)
实现上的区别：skiplist 实现简单；rbtree实现复杂，增加、删除比较复杂。

## skiplist与B+树对比

结构的区别：skiplist是多层有序链表；B+树是多叉平衡搜索树，叶子节点是有序双向链表

增删改查时间复杂度：skiplist要由于B+树；

范围查找：skiplist和B+树都适合范围查找；

skiplist适用于内存查找；B+树适合磁盘存储

实现上的区别：skiplist实现简单；B+树节点分裂复杂。