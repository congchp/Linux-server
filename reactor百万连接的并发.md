# epoll水平触发/边沿触发

LT，recvbuff中有数据就一直触发；

ET，recvbuff中收到数据，只触发一次。如果recvbuff中数据没有读完，不会再次触发，当recvbuff中收到新的数据时，再次触发。也就是收到一个包，只触发一次。比如客户端发送32byte的包，服务器只recv了10byte的数据，那么epoll不会再次触发，等到下一次客户端再发送32byte的数据，epoll会再一次触发。对于ET，对于recv，最好是一个循环的读，直到读完，返回-1。

所以LT适合用于大包，数据没读完会一直触发；ET适合小包，只触发一次，需要应用程序循环把数据读完。



ET模式下，sendbuff从不可发送到可发送，只触发一次。

send的情况，如果sendbuff一直为空，如果用ET，epoll会一直触发吗？

测试过，只会触发一次。



哪些场景使用水平触发？

1. 小数据，使用边沿触发
2. 数据块，数据量比较大使用水平触发。防止一次性接收不完。

listenfd用水平触发，如果多个client同时连接进来，listenfd里面积攒多个连接的话，accept一次只处理一个连接，防止漏掉连接，选择水平触发。

水平触发和边沿触发分界点，recv的BUFFER_LENGTH如果一次能接收完recv buffer中的数据，就是小数据，一次接收不完就是大数据

# 修改代码支持百万连接

## reactor怎么存储100万个event

**使用reactor实现百万并发连接的服务器，需要考虑event怎么保存，怎样分配内存, 存储百万级别的event。**

### 数据结构设计

利用fd是递增的特性，可以设计成下面的结构。这样做可扩展性非常好，reactor存储的event数量不受限制。

```c
typedef struct _eventblock {

    struct _eventblock *next;
    nevent *events; // 每一个block 1024个event

} eventblock;


typedef struct _nreactor {

    int epfd;
    int blkcnt;
    eventblock *evblk;

} nreactor;
```

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639547864629-86c92245-d83b-4db6-bc0d-549ac090b9a6.png)

### 代码实现

通过fd，可以计算得出相应的eventblock位置，以及event在该eventblock中的相应位置。之前代码中所有使用reactor->events的都需要做相应的修改。

```c
nevent *nreactor_idx(nreactor *reactor, int sockfd) {

    int blkidx = sockfd / MAX_EPOLL_EVENTS;

    while (blkidx >= reactor->blkcnt) {
        nreactor_alloc(reactor);
    }
    
    int i = 0;
    eventblock *blk = reactor->evblk;
    while (i++ < blkidx && blk != NULL) {
        blk = blk->next;
    }
    
    return &blk->events[sockfd % MAX_EPOLL_EVENTS];

}
```

reactor初始化时，也需要相应的申请eventblock内存，以及events

```c
int nreactor_alloc(nreactor *reactor) {

    if (reactor == NULL) return -1;
    if (reactor->evblk == NULL) return -1;

    eventblock *blk = reactor->evblk;

    while (blk->next != NULL) {

        blk = blk->next;
    }

    nevent *evs = (nevent *)malloc(MAX_EPOLL_EVENTS * sizeof(nevent));
    if (evs == NULL) {
        perror("nreactor_alloc malloc events failed");
        return -2;
    }
    memset(evs, 0, MAX_EPOLL_EVENTS * sizeof(nevent));

    eventblock *block = (eventblock *)malloc(sizeof(eventblock));

    if (block == NULL) {
        perror("nreactor_alloc malloc block failed");
        return -2;
    }
    memset(block, 0, sizeof(eventblock));

    block->events = evs;
    block->next = NULL;

    blk->next = block;
    reactor->blkcnt++;

    return 0;
}
```

完整代码

```c
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>



#define BUFFER_LENGTH       1024
#define MAX_EPOLL_EVENTS    1024
#define SERVER_PORT         9105
#define PORT_COUNT          100

typedef int (*NCALLBACK)(int fd, void *arg);

typedef struct _nevent {

    int fd;
    int events;
    void *arg;
    NCALLBACK callback;

    int status; // whether fd is in epoll now.
    char buffer[BUFFER_LENGTH];
    int length;

} nevent;

typedef struct _eventblock {

    struct _eventblock *next;
    nevent *events; // 每一个block 1024个event

} eventblock;


typedef struct _nreactor {

    int epfd;
    int blkcnt;
    eventblock *evblk;

} nreactor;

int recv_cb(int client_fd, void *arg);
int send_cb(int client_fd, void *arg);
int accept_cb(int listen_fd, void *arg);
nevent *nreactor_idx(nreactor *reactor, int sockfd);

int init_sock(unsigned short port) {

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (listen(listen_fd, 20) < 0) {
        perror("listen");
    }

    return listen_fd;
}

int nreactor_alloc(nreactor *reactor) {

    if (reactor == NULL) return -1;
    if (reactor->evblk == NULL) return -1;

    eventblock *blk = reactor->evblk;

    while (blk->next != NULL) {

        blk = blk->next;
    }

    nevent *evs = (nevent *)malloc(MAX_EPOLL_EVENTS * sizeof(nevent));
    if (evs == NULL) {
        perror("nreactor_alloc malloc events failed");
        return -2;
    }
    memset(evs, 0, MAX_EPOLL_EVENTS * sizeof(nevent));

    eventblock *block = (eventblock *)malloc(sizeof(eventblock));

    if (block == NULL) {
        perror("nreactor_alloc malloc block failed");
        return -2;
    }
    memset(block, 0, sizeof(eventblock));

    block->events = evs;
    block->next = NULL;

    blk->next = block;
    reactor->blkcnt++;

    return 0;
}

int nreactor_init(nreactor *reactor) {

    if (reactor == NULL)  return -1;
    memset(reactor, 0, sizeof(nreactor));

    reactor->epfd = epoll_create(1);
    if (reactor->epfd < 0) {
        perror("epoll_create");
        return -2;
    }

    nevent *evs = (nevent *)malloc(MAX_EPOLL_EVENTS * sizeof(nevent));
    if (evs == NULL) {
        perror("nreactor_init malloc events failed");
        return -2;
    }
    memset(evs, 0, MAX_EPOLL_EVENTS * sizeof(nevent));

    eventblock *block = (eventblock *)malloc(sizeof(eventblock));

    if (block == NULL) {
        perror("nreactor_init malloc block failed");
        return -2;
    }
    memset(block, 0, sizeof(eventblock));

    block->events = evs;
    block->next = NULL;

    reactor->evblk = block;
    reactor->blkcnt = 1;

    return 0;

}

int nreactor_destroy(nreactor *reactor) {

    close(reactor->epfd);
    
    eventblock *blk = reactor->evblk;
    eventblock *blk_next = NULL;

    while (blk != NULL) {

        blk_next = blk->next;

        free(blk->events);
        free(blk);
        blk = blk_next;
    }

    return 0;
}

void nreactor_event_set(nevent *ev, int fd, NCALLBACK callback, void *arg) {

    ev->fd = fd;
    ev->callback = callback;
    ev->arg = arg;
    ev->events = 0;

}

int nreactor_event_add(int epfd, nevent *ev, int events) {

    struct epoll_event ep_ev = {0, {0}};
    ep_ev.events = ev->events = events;
    ep_ev.data.ptr = ev;

    int op;
    if (ev->status == 1) {
        op = EPOLL_CTL_MOD;
    } else {
        ev->status = 1;
        op = EPOLL_CTL_ADD;
    }

    if (epoll_ctl(epfd, op, ev->fd, &ep_ev) < 0) {
        perror("epoll_ctl");
        return -1;
    }

    return 0;

}

int nreactor_event_del(int epfd, nevent *ev) {

    struct epoll_event ep_ev = {0, {0}};
    
    if (ev->status != 1) {
        return -1;
    }

    ev->status = 0;

    epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, NULL);

    return 0;

}

int recv_cb(int client_fd, void *arg) {

    nreactor *reactor = (nreactor *)arg;
    if (reactor == NULL) return -1;

    nevent *ev = nreactor_idx(reactor, client_fd);

    int len = recv(client_fd, ev->buffer, BUFFER_LENGTH, 0);
    nreactor_event_del(reactor->epfd, ev);

    if (len > 0) {

        ev->length = len;
        ev->buffer[len] = '\0';

        printf("client_fd[%d]:%s\n", client_fd, ev->buffer);

        nreactor_event_set(ev, client_fd, send_cb, reactor);
        nreactor_event_add(reactor->epfd, ev, EPOLLOUT);

    } else if (len == 0) {

        close(client_fd);
        // printf("[client_fd=%d] pos[%ld], closed\n", client_fd, ev - reactor->events);

    } else {

        close(client_fd);
        perror("recv");

    }

    return len;

}

int send_cb(int client_fd, void *arg) {

    nreactor *reactor = (nreactor *)arg;
    if (reactor == NULL) return -1;

    nevent *ev = nreactor_idx(reactor, client_fd);

    int len = send(client_fd, ev->buffer, ev->length, 0);
    nreactor_event_del(reactor->epfd, ev);
    if (len > 0) {

        printf("send[client_fd=%d], [%d]%s\n", client_fd, len, ev->buffer);

        nreactor_event_set(ev, client_fd, recv_cb, reactor);
        nreactor_event_add(reactor->epfd, ev, EPOLLIN);

    } else {

        close(ev->fd);
        perror("send");
    }

    return len;

}

int accept_cb(int listen_fd, void *arg) {

    nreactor *reactor = (nreactor *)arg;
    if (reactor == NULL) return -1;

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &len);
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }

    int flag = fcntl(client_fd, F_SETFL, O_NONBLOCK);
    if (flag < 0) {
        perror("fcntl");
        return -2;
    }

    nevent *ev = nreactor_idx(reactor, client_fd);
    nreactor_event_set(ev, client_fd, recv_cb, reactor);
    nreactor_event_add(reactor->epfd, ev, EPOLLIN);

    printf("new connect [%s:%d], client_fd[%d]\n", 
        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);
}

nevent *nreactor_idx(nreactor *reactor, int sockfd) {

    int blkidx = sockfd / MAX_EPOLL_EVENTS;

    while (blkidx >= reactor->blkcnt) {
        nreactor_alloc(reactor);
    }
    
    int i = 0;
    eventblock *blk = reactor->evblk;
    while (i++ < blkidx && blk != NULL) {
        blk = blk->next;
    }
    
    return &blk->events[sockfd % MAX_EPOLL_EVENTS];

}


int nreactor_addlistener(nreactor *reactor, int listen_fd, NCALLBACK accept_cb) {

    if (reactor == NULL || reactor->evblk == NULL) {
        return -1;
    }

    nevent *event = nreactor_idx(reactor, listen_fd);

    nreactor_event_set(event, listen_fd, accept_cb, reactor);
    nreactor_event_add(reactor->epfd, event, EPOLLIN);

    return 0;

}

int nreactor_run(nreactor *reactor) {

    if (reactor == NULL) return -1;
    if (reactor->evblk == NULL) return -1;

    struct epoll_event events[MAX_EPOLL_EVENTS];

    while (1) {

        int nready = epoll_wait(reactor->epfd, events, MAX_EPOLL_EVENTS, 1000);
        if (nready < 0) {
            perror("epoll_wait");
            continue;
        }

        int i;
        for (i = 0; i < nready; i++) {

            nevent *ev = (nevent *)events[i].data.ptr;

            if ((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)) {
                ev->callback(ev->fd, ev->arg); 
            }
            if ((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)) {
                ev->callback(ev->fd, ev->arg);
            }

        }

    }

}


#if 1

int main(int argc, char *argv[]) {

    unsigned short port = SERVER_PORT;
    if (argc == 2) {
        port = atoi(argv[1]);
    }

    nreactor *reactor = (nreactor *)malloc(sizeof(nreactor));
    nreactor_init(reactor);

    int i = 0;
    int listen_fds[PORT_COUNT] = {0};
    for (i = 0; i < PORT_COUNT; i++) {
        listen_fds[i] = init_sock(port + i);
        nreactor_addlistener(reactor, listen_fds[i], accept_cb);
    }

    
    nreactor_run(reactor);

    nreactor_destroy(reactor);

    for (i = 0; i < PORT_COUNT; i++) {
        close(listen_fds[i]);
    }

    free(reactor);

    return 0;
}


#endif
```

# 如何测试

## 服务器端修改配置

### 修改ulimit

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639542124771-7a6428f8-dca0-4371-86c0-a32a86c309b9.png)

```c
# /etc/security/limits.conf
* hard nofile 1048576
* soft nofile 1048576
```

### 修改file-max

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639542279640-577a5758-56bf-4ef1-93f6-47a86412c700.png)

```c
# vim /etc/sysctl.conf
# cat /proc/sys/fs/file-max
fs.file-max = 1048576
```

### 修改nf_conntrack_max

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639554396057-02dd8460-14e5-4c39-85a6-c3f304b11069.png)

```c
# /etc/sysctl.conf
# cat /proc/sys/net/netfilter/nf_conntrack_max
# sysctl -p
net.netfilter.nf_conntrack_max = 1048576
```

### 修改tcp_wmem， tcp_rmem

如果服务器的内存不够，为了测试，修改一下wmem和rmem，即将sendbuff、recvbuff改小，单位是byte。测试后，记得还原回去。

```c
# vim /etc/sysctl.conf
# cat /proc/sys/net/ipv4/tcp_wmem
# sysctl -p

net.ipv4.tcp_rmem = 512 512 1024
net.ipv4.tcp_wmem = 512 512 1024
```

### 修改tcp_mem

tcp_mem是tcp协议栈的大小，单位是页，一页4k

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639603663996-ae7f6fc0-2187-43a9-b0f3-51223bffbd91.png)

```c
# vi /etc/sysctl.conf
# sysctl -p
# cat /proc/sys/net/ipv4/tcp_mem

net.ipv4.tcp_mem = 757596 1010128 1515192
```

## 测试结果

最后只到61W多，四台机器，每个都是4核16G。

跑到50W左右的时候，连接处理的速度就比较慢了。

Server：

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639557439237-637c7fad-743d-4588-93b3-a097759764b7.png)

Client：

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639557478678-9e949a3f-dad3-46a3-a40d-f210fa3e66aa.png)

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639557518604-54188ebb-90b0-4e6c-9ab6-59026335e2c6.png)

# reactor如何支持多核

1. 大量客户端连到服务器，要使连接的处理速度更快，可以把100个listenfd放在不同的线程。

可以通过一个线程一个reactor来实现。每个线程一个reactor，对应一个listenfd。

2. 如果服务器只监听一个端口，怎么做？开进程

一个master进程，多个worker进程。通过加accept锁来决定由那个worker进行处理该连接。

3. 可以把listenfd和clientfd放到不同的线程里面。

可以看下libevent/redis的reactor, 将listenfd和clientfd，使用不同的线程进行处理。

开一个worker线程，main线程处理accept，worker线程处理clientfd。main线程可以使用poll。

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1634433589022-6e597505-5ce6-4a9d-a01a-bb2c382819e8.png)

单线程，libevent/redis

多线程，memcached，每个线程一个epoll_wait

多进程，nginx





我们实现的reactor是个单线程的，100个listenfd都在一个线程里，如果想要性能更高，可以把100个listenfd放在不同的线程里面。



3个思考问题：

1）reactor怎么使用多线程，比如listen 10个端口，怎么做到每个端口(listenfd)一个线程？

每个线程一个reactor，对应一个listenfd

2）服务器一般只监听一个port，比如8888，怎么解决？可以通过开进程解决。

怎么做到多个进程listen一个端口？

nginx的解决方案

3）如何做到listenfd和clientfd在不同的线程？

一个main线程，一个worker线程。main线程负责listen，处理连接；worker线程负责处理clientfd。



listen的backlog参数，linux系统上指的是accpt队列长度；unix系统上是sync + accept队列长度。