# 什么是reactor？

reactor释义`反应堆`，是一种事件驱动机制。

reactor模式是对epoll的一层封装，将网络io转换成event，将对fd的管理转成对事件的管理。redis/nginx/libevent对网络io的处理，都采用了reactor模式。

reactor对大量io进行管理，每一个fd对应一个item的事件，做了单独的管理，fd通过epoll进行检测，当一个fd的事件到来，调用相应的callback进行处理，reactor就像一个反应堆一样，对网络io进行反应。将之前对io的管理换成了对事件的管理。

epoll检测到fd有事件，调用与之对应的callback进行处理。每个sockfd对应一个item，里面包含fd，callback等，在EPOLL_CTL_ADD的时候，通过epoll_event的data成员的ptr指针，保存epoll里面。epoll检测出fd有事件需要处理的时候，通过epoll_wait将这个值带出来，就可以调用对应的callback了。

# reactor设计

1. init_socket(), 初始化socket。reactor是一种服务器模型，所以需要初始化服务器的socket。
2. reactor_init(), 初始化reactor。主要是创建epoll fd，并且申请reactor中保存event的内存。
3. reactor_addlistener(), 将listenfd加入epoll，并设置相应的callback。
4. reactor_run(), 启动reactor，核心是一个while(1)循环 + epoll_wait，检测到相应的读写事件后，调用相应的callback。
5. reactor_destroy(), close epfd，并释放reactor中的item。



# 代码实现

## 数据结构定义

```c
struct nevent {
	int fd;
	int events;
	void *arg;
	int (*callback)(int fd, int events, void *arg);

	char buffer[BUFFER_LENGTH];
    int status // 1表示fd已经
};



struct nreactor {
	int epfd;
	struct nevent *events; // 暂时用数组
};
```

reactor中如果有大量io，event可以两种数据结构存储

1. 用红黑树存储，fd作为key。
2. 或者可以使用list+数组的方式，使用fd/1024可以确定是在list中的哪一个节点，再使用fd%1024可以定位在数组中的具体位置。

## init_socket

- socket
- bind

- listen

```c
int init_sock(short port) {

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	fcntl(fd, F_SETFL, O_NONBLOCK); // reactor使用非阻塞io

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

	if (listen(fd, 20) < 0) {
		printf("listen failed : %s\n", strerror(errno));
	}

	return fd;
}
```

## reactor_init

```c
int nreactor_init(struct nreactor *reactor) {

	if (reactor == NULL) return -1;
	memset(reactor, 0, sizeof(struct nreactor));

	reactor->epfd = epoll_create(1);
	if (reactor->epfd <= 0) {
		printf("create epfd in %s err %s\n", __func__, strerror(errno));
		return -2;
	}

	reactor->events = (struct nevent*)malloc((MAX_EPOLL_EVENTS) * sizeof(struct nevent));
	if (reactor->events == NULL) {
		printf("create epfd in %s err %s\n", __func__, strerror(errno));
		close(reactor->epfd);
		return -3;
	}
}
```

## reactor_addlistener

```c
int nreactor_addlistener(struct nreactor *reactor, int sockfd, NCALLBACK *acceptor) {

	if (reactor == NULL) return -1;
	if (reactor->events == NULL) return -1;

	nreactor_event_set(&reactor->events[sockfd], sockfd, acceptor, reactor);
	nreactor_event_add(reactor->epfd, EPOLLIN, &reactor->events[sockfd]);

	return 0;
}
```

## reactor_run

```c
int nreactor_run(struct nreactor *reactor) {
	if (reactor == NULL) return -1;
	if (reactor->epfd < 0) return -1;
	if (reactor->events == NULL) return -1;
	
	struct epoll_event events[MAX_EPOLL_EVENTS+1];
	
	int checkpos = 0, i;

	while (1) {

		int nready = epoll_wait(reactor->epfd, events, MAX_EPOLL_EVENTS, 1000);
		if (nready < 0) {
			printf("epoll_wait error, exit\n");
			continue;
		}

		for (i = 0;i < nready;i ++) {

			struct nevent *ev = (struct nevent*)events[i].data.ptr;

			if ((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)) {
				ev->callback(ev->fd, events[i].events, ev->arg);
			}
			if ((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)) {
				ev->callback(ev->fd, events[i].events, ev->arg);
			}
			
		}

	}
}
```

## reactor_destroy

```c
int nreactor_destory(struct nreactor *reactor) {

	close(reactor->epfd);
	free(reactor->events);

}
```

## reactor的优点

- 响应快，不必为单个同步事件所阻塞；
- 编程相对简单，可以最大程度的避免复杂的多线程及同步问题，并且避免了多线程/进程的切换开销；

- 可扩展性, 可以方便地通过增加reactor实例个数来充分利用CPU资源；
- 可复用性，reactor框架本身与具体事件的处理逻辑无关，具有很高的复用性。

reactor模型开发效率上比直接使用io多路复用要高，reactor通常是单线程的，设计目标是系统单线程一颗CPU的全部资源，所以对与每个事件的处理可以不考虑共享资源的互斥访问。如果要利用多核，可以开启多个reactor，每个reactor一颗CPU核心。

## reactor qps

epoll qps, 在没有数据库或者日志操作的时候，epoll + reactor的qps能达到5k以上，也就是1s能处理5000个请求。

## proactor

proactor，与reactor类似的，是基于iocp的, 使用的是异步io。

boost asio使用proactor。





## 完整代码

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

typedef struct _nreactor {

    int epfd;
    nevent *events;

} nreactor;

int recv_cb(int client_fd, void *arg);
int send_cb(int client_fd, void *arg);
int accept_cb(int listen_fd, void *arg);

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


int nreactor_init(nreactor *reactor) {

    if (reactor == NULL)  return -1;
    memset(reactor, 0, sizeof(nreactor));

    reactor->epfd = epoll_create(1);
    if (reactor->epfd < 0) {
        perror("epoll_create");
        return -2;
    }

    reactor->events = (nevent *)malloc(MAX_EPOLL_EVENTS * sizeof(nevent));
    if (reactor->events == NULL) {

        perror("malloc reactor->events");
        close(reactor->epfd);
        return -3;
    }
    memset(reactor->events, 0, MAX_EPOLL_EVENTS * sizeof(nevent));

    return 0;

}

int nreactor_destroy(nreactor *reactor) {

    close(reactor->epfd);
    free(reactor->events);

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

    nevent *ev = reactor->events + client_fd;

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
        printf("[client_fd=%d] pos[%ld], closed\n", client_fd, ev - reactor->events);

    } else {

        close(client_fd);
        perror("recv");

    }

    return len;

}

int send_cb(int client_fd, void *arg) {

    nreactor *reactor = (nreactor *)arg;
    if (reactor == NULL) return -1;

    nevent *ev = reactor->events + client_fd;

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

    nevent *ev = reactor->events + client_fd;
    nreactor_event_set(ev, client_fd, recv_cb, reactor);
    nreactor_event_add(reactor->epfd, ev, EPOLLIN);

    printf("new connect [%s:%d], client_fd[%d]\n", 
        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);
}


int nreactor_addlistener(nreactor *reactor, int listen_fd, NCALLBACK accept_cb) {

    if (reactor == NULL || reactor->events == NULL) {
        return -1;
    }

    nreactor_event_set(&reactor->events[listen_fd], listen_fd, accept_cb, reactor);
    nreactor_event_add(reactor->epfd, &reactor->events[listen_fd], EPOLLIN);

    return 0;

}

int nreactor_run(nreactor *reactor) {

    if (reactor == NULL) return -1;
    if (reactor->events == NULL) return -1;

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

    int listen_fd = init_sock(port);

    nreactor *reactor = (nreactor *)malloc(sizeof(nreactor));
    nreactor_init(reactor);

    nreactor_addlistener(reactor, listen_fd, accept_cb);
    nreactor_run(reactor);

    nreactor_destroy(reactor);
    close(listen_fd);

    free(reactor);

    return 0;
}


#endif
```