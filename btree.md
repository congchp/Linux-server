# 为什么会有btree

多叉树的作用，使得节点数量变少。查找节点的数量变少。

多叉树，降层高。为了寻址次数减少。



rbtree如果用在内存，意义不大。



# 多叉树和btree之间的关系

1. 多叉树没有约束平衡
2. 多叉树没有约束每个节点子树的数量
3. btree遍历是顺序的。



# btree的定义

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1637809311523-19d0186f-a56b-41e5-94ba-29ac2c283ff5.png)

# btree和b+树的区别

1. 在btree基础上，将所有叶子节点做了一个链表；
2. b+数所有数据都存在叶子节点上，内节点不存储数据，当作索引来用。b树叶子节点和内节点都存储数据，不重复。

主要用于磁盘存储的时候做索引。



100万条数据，在数据库里是怎么存的？



# 代码实现

## 节点插入insert

所有节点都是插在叶子节点上面，理论上是可以插在内节点上。但是插在叶子节点上实现比较简单。

1. 如果根节点已满，则新创建一个节点，该节点成为新的根节点，旧的根节点成为它的子节点。旧的根节点进行分裂。之后从分裂的节点开始进行未满插入。
2. 如果根节点未满，则从根节点开始进行未满插入。
3. 未满插入的case，需要区分节点是叶子节点还是内节点。

3.1 如果是叶子节点，则直接进行插入。

3.2 如果是内节点，分需要将要进行插入的子节点是否已满，如果已满，则进行分裂。

​     之后对于分裂后得到的相应的子节点再进行递归未满插入



![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639988050978-1f995754-2f0e-4aa0-a1ef-0af68b3d03ec.png)





![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1637822937898-c41a548d-ab3d-4c4c-a60b-4ec6c42f7e44.png)



![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639988112008-d75c4d6b-7a48-4ef7-bd0f-d6321cdd0580.png)



![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639988163677-1764acd1-1bb3-45d9-99c6-40105e42427b.png)



## 节点删除





如果要删除F，应该怎么做？先合并DEFGH,然后在叶子结点删除F

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1637878369432-1f589ea5-ace4-4a3b-9171-6395cf5da129.png)

## btree如何做到线程安全

锁子树，下沉到下一层的时候，解锁上一层的节点，加锁解锁会比较频繁。

mysql里面的锁的粒度是怎样的？



```c
typedef int KEY_TYPE;

typedef struct _btree_node {

    KEY_TYPE *keys;
    _btree_node **children;

    int key_num;
    bool leaf;

} btree_node;

typedef struct _rbtree {

    btree_node *root;
    int t; // 2*t是btree的阶数


} btree;



void btree_create_node(int t, bool leaf) {

    btree_node *node = (btree_node*)calloc(1, sizeof(btree_node));
    if (node == NULL) return NULL;

    node->keys = (KEY_TYPE*)calloc(1, (2*t - 1) * sizeof(KEY_TYPE) );
    node->children = (btree_node**)calloc(1, 2*t * sizeof(btree_node*));

    node->key_num = 0;
    node->leaf = leaf;

}

void btree_destroy_node(btree_node *node) {
    if (node) {
        if (node->keys) {
            free(node->keys);
        }
        if (node->children) {
            free(node->children);

        }

        free(node);
    }
}



void btree_create(btree *T, int t) {

    T->t = t;
    btree_node *x = btree_create_node(t, 1);

    T->root = x;

}

void btree_split_child(btree *T, btree_node *x, int i) {

    btree_node *y = x->children[i];
    btree_node *z = btree_create_node(T->t, y->leaf);

    int j = 0;

    for (j = 0; j < T->t - 1; j++)  {
        z->keys[j] = y->keys[j + T->t];
    }

    if (y->leaf == 0) {
        for (j = 0; j < T->t; j++) {
            z->children[j] = y->children[j];
        }
    }

    y->num = T->t - 1;

    for (j = x->num; j >= i+1; j--) { // x有没有可能满了？
        x->children[j+1] = x->children[j];
    }

    x->children[i+1] = z;

    for (j = x->num-1; j>=i; j--) {
        x->keys[j+1] = x->keys[j];
    }
    x->keys[i] = y->keys[T->t-1];
    x->num += 1;


}



void btree_insert_nonfull(btree *T, btree_node *x, KEY_TYPE key) {

    int i = x->num - 1;

    if (x->leaf == 1) {

        while (i >= 0 && x->keys[i] > key) {

            x->keys[i+1] = x->keys[i];
            i--;

        }
        x->keys[i+1] = key;
        x->num += 1;

    } else {

        while (i >= 0 && x->keys[i] > key) i--;

        if (x->children[i+1]->num == 2*T->t - 1) {

            btree_split_child(T, x, i+1);
            if (x->keys[i+1] < key) i--;

        }

        btree_insert_nonfull(T, x->children[i+1], key);

    }

}

void btree_insert(btree *T, KEY_TYPE key) {

    btree_node *r = T->root;

   if (r->num = 2*T->t - 1) {

        btree_node *node = btree_create_node(T->t, 0);
        T->root = node;

        node->children[0] = r;

        btree_split_child(T, node, 0);

        int i = 0;
        if (node->keys[0] < key) i++;

        btree_insert_nonfull(T, node->children[i], key);
   } else {
       btree_insert_nonfull(T, r, key);
   }


}
```

## 数据结构动态演示

https://www.cs.usfca.edu/~galles/visualization/Algorithms.html

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638146917761-c7d10549-4198-476b-857f-6492a88d1b30.png)

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1638146947526-92c3a893-be31-47da-98e8-c446c8dcff5b.png)