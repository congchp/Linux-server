# 用户态协议栈，为什么要实现epoll？

epoll并不是协议栈里面的，为什么要实现用户态协议栈？

因为内核的epoll是对内核文件系统vfs fd进行的管理，是跟内核协议栈一起使用的，内核协议栈处理io后通过回调的方式来操作epoll中的就绪队列；而用户态协议栈，fd是用户空间的，内核的epoll没办法对用户空间fd进行管理，所以用户态协议栈必须要有用户态的epoll。



用户态epoll是参考内核的epoll，在用户空间实现了epoll的功能。

内核epoll代码：fs/eventpoll.c



epoll设计需要考虑以下4个方面：

1. 数据结构选择；
2. 协议栈如何与epoll模块通信；
3. epoll如果加锁；
4. ET与LT如何实现？

# epoll数据结构

epoll至少有两个集合

1. 所有交由epoll管理的fd的总集；
2. 就绪fd，可读可写的集合。

## 总集用什么数据结构去存储？

是key-value的格式，通过fd要能够找到value。

1. hash
2. ~~数组~~
3. 红黑树
4. b树/b+树
5. ~~avl树~~

数组大小受限，不容易扩展；查找效率低。

hash，存储空间浪费；对于数量足够大的时候，查找效率高。

avl，对于查找、删除、性能，红黑树由于avl树。

btree/b+tree，多叉树，叶子节点都在同一层，降低层高，主要用于磁盘存储。

rbtree，对于查找效率和空间利用率综合考虑，是最优的。

**总集选择用红黑树。**

## 就绪集合选择什么数据结构去存储？

就绪集合不是以查找为主，所有的就绪fd都需要被拿出去处理。

可以选择线性数据结构：

1. 队列
2. 栈

**就绪集合选择队列**，先进先出。



epoll使用红黑树和队列，红黑树存放需要检测的节点，队列存放就绪的节点。


![img](https://upload-images.jianshu.io/upload_images/9664813-ae8a19ae3e2b9a8a.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)



# epoll工作环境

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644541169142-10f06422-8c31-4a16-a025-03a181ac2a25.png)

# epoll与select/poll的区别

1. 使用：每次调用poll，都需要把fd的总集传进去，从用户空间copy到内核空间；epoll不需要。poll返回后，应用程序需要遍历fd集合，看哪些fd可读可写了；而epoll返回的直接是就绪队列，其中所有fd都是可读可写的。
2. 实现原理：poll在实现的时候，内核采用循环遍历总集的方式去查看每一个fd是否就绪；epoll是在协议栈中通过callback将就绪的节点加入到就绪队列。

# epoll三个函数

int epoll_create(int size);

1. 分配一个eventpoll;
2. 初始化红黑树的根节点epfd。
   eventpoll与epfd一一对应。



int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
操作红黑树，根据op对红黑树进行增删改



int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
把就绪队列的数据从内核copy到用户空间。

如果maxevents小于就绪队列大小，比如就绪队列大小为100，maxevents传入50，怎么办呢？

先将就绪队列里面的50个节点copy到用户空间，之后下次epoll_wait再copy 50个节点。

# 协议栈通知epoll的时机

epoll怎么知道哪个io就绪了，需要将节点加入到就绪队列？

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644544264778-3ac33eec-9e93-4ab6-a38c-1025999f46dc.png)

recv的情况，是在回复了ack后，协议栈才通知epoll。

send的情况，如果之前sendbuff是满的，需要等发送了一次数据，收到对端回复的ack，清空一部分sendbuff数据，再通知epoll。



收到网络包后，能够解析出五元组，根据五元组能够查找到对应的fd，在到红黑树去查找到对应的节点。



红黑树和就绪队列是个什么关系？红黑树的节点和就绪队列的节点是一个节点。每次将红黑树的节点加入到就绪队列，并不是将节点从红黑树中delete掉，

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644545386683-0da5a297-3592-4bf5-b440-f4c233ff543d.png)

# 协议栈回调到epoll，都需要做什么？

回调函数都需要做哪些事情？

需要传哪些参数？

fd，EPOLLIN、EPOLLOUT事件



需要做的事情：

1. 通过fd查找对应的节点；
2. 把节点加入到就绪队列里面。

# epoll是线程安全的吗

epoll是否线程安全，就需要考虑epoll三个接口是否线程安全？



epoll_create，对epoll进行初始化，是线程安全的。

epoll_ctl, 是操作红黑树；epoll_wait, 是操作就绪队列。

需要考虑epoll工作环境，epoll是工作在应用程序和协议栈之间。应用程序调用epoll_ctl的时候，协议栈是否会有回调操作红黑树？调用epoll_wait从就绪队列里面copy出来的时候，协议栈是否会操作就绪队列？要保证线程安全，需要对红黑树和就绪队列加锁。

红黑树加锁，有两种加锁方法：

1. 对整棵树加锁；
2. 对子树加锁。

对子树加锁是一件很麻烦的事情。所以实现的是对整棵树加锁，使用mutex。

就绪队列，使用spinlock。

epoll_wait，使用条件等待，cond + cdmtx

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644549321046-f5bbc8fa-8e63-4423-b322-860f3ed15146.png)

# ET、LT如何实现？

ET和LT是如何实现的？会不会回调？
LT :  水平触发，如果没有读完，会一直触发
ET：不管有没有读完，只触发一次

**ET、LT本质区别就是回调次数。**

ET 接收到数据，调用一次回调

LT recvbuffer里面有数据，就调用回调。

协议栈里面有一个while(1)循环检测。

```c
while (1) {
	// 协议栈处理网卡数据
    // 这里就会判断调用回调的次数
}
```


一直回调怎么实现？就是上面提到的，有一个while(1)，不断去检测。

ET适合小块

LT适合大块

# epoll代码

## 数据结构

```c
struct epitem {
	RB_ENTRY(epitem) rbn;
	LIST_ENTRY(epitem) rdlink;
	int rdy; //exist in list 
	
	int sockfd;
	struct epoll_event event; 
};

struct eventpoll {
	ep_rb_tree rbr;
	int rbcnt;
	
	LIST_HEAD( ,epitem) rdlist;
	int rdnum;

	int waiting;

	pthread_mutex_t mtx; //rbtree update
	pthread_spinlock_t lock; //rdlist update
	
	pthread_cond_t cond; //block for event
	pthread_mutex_t cdmtx; //mutex for cond
	
};
```

## epoll api实现

1. int epoll_create(int size);
   创建eventpoll，初始化rbtree，rdlist，以及epoll中使用的锁和条件变量
2. int epoll_ctl(int epid, int op, int sockid, struct epoll_event *event)
   根据op，EPOLL_CTL_ADD/EPOLL_CTL_DEL/EPOLL_CTL_MOD，操作红黑树。
3. int epoll_wait(int epid, struct epoll_event *events, int maxevents, int timeout)
   将就绪队列中的数据(fd, events)从内核空间copy到用户空间, 并将数据从就绪队列中移除。
   timeout > 0, 使用 pthread_cond_timedwait()
   timeout < 0, 一直阻塞，使用pthread_cond_wait(), 阻塞到就绪队列里面有数据。协议栈调用callback通知epoll，发送signal，epoll_wait解除阻塞。
4. int epoll_event_callback(struct eventpoll *ep, int sockid, uint32_t event)
   根据fd，从rbtree中找到节点，并加入到就绪队列中。协议栈通过callback来通知epoll模块fd就绪。