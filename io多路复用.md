**io多路复用(select/poll/epoll)**

# 什么是io多路复用？为什么要有io多路复用？

原始的server，处理多个连接的方法，有两种：
1）一个while循环，不断去轮询，检测每一个socket是否有消息，并处理；
2）主线程accept，之后一个连接一个线程

第二种方法要优于第一种。方法2优点是逻辑简单；缺点是不适合大量客户端。
上述两种方法，效率肯定不高。很多时候，都是做的无效的recv，并且线程是占用资源的，为每一个socket创建一个线程肯定是不合算的。

一请求一线程，posix的thread一个线程的栈空间是8M，所以处理不了大量的连接，会有C10K的问题。
因此，linux内核提供了select/poll，实现了io多路复用。内核为不断轮询检测Io，然后告诉用户进程哪些io可读/可写。将检测io可读/可写的状态，与实际的recv/send进行分离。



# **为什么要有epoll？epoll比select/poll强在哪里？**

select/poll, 调用的时候，每次都需要将关心的socket传入内核，从用户空间copy进内核；返回的时候，又将所有socket状态都返回，需要进行遍历操作，所以效率不高。
select性能最优是连接数在1024(FD_SETSIZE)左右, poll与select原理一样，所以性能也差不多。但是可以多开几个select，能够突破C10K，但达不到C1000K
linux2.6开始，提供了epoll来解决C10K的问题。epoll使用rbtree+链表，rbtree存储待检测的socket，epoll_wait检测后，将链表(就绪队列)传入用户空间，就绪队列里面存的都是可读/可写的socket。效率比select/poll提升很多。单台server使用epoll，可以达到百万并发。



# **listen参数backlog的含义**



```plain
int listen(int sockfd, int backlog) // backlog是TCP三次握手中，sync队列和accept队列元素之和
```



# **epoll中什么是ET?LT?**

Level Triggered (LT)
Edge Triggered(ET)
后面再epoll中介绍
大块数据用LT, 小块数据用ET。



# **一连接一进程，select/poll/epoll基础的使用，直接参考下面的代码**



```plain
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <pthread.h>

#define BUFFER_LENGTH 1024
#define POLL_SIZE 1024
#define EPOLL_SIZE 1024

void* client_proc(void *arg) {
    int client_fd = *(int*)arg;

    while (1) {
        char buffer[BUFFER_LENGTH] = {0};
        int ret = recv(client_fd, buffer, BUFFER_LENGTH, 0);
        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("data has already been read by another thread\n");
            }
            return NULL;
        } else if (ret == 0) {
            printf("disconnect \n");
            return NULL;
        } else {
            printf("Recv: %s, %d Bytes\n", buffer, ret);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Parameter error\n");
        return -1;
    }
    int port = atoi(argv[1]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        return 2;
    }
    
    if (listen(listen_fd, 10)) {
        perror("listen");
        return 3;
    }

#if 1
    while (1) {
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        int client_len = sizeof(struct sockaddr_in);

        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd <= 0) {
            continue;
        }

        pthread_t thread_id;
        int ret = pthread_create(&thread_id, NULL, client_proc, &client_fd);
        if (ret < 0) {
            perror("pthread_create");
            exit(1);
        }
    }
#elif 1
    fd_set rset_w, rset_c;

    FD_ZERO(&rset_w);
    FD_SET(listen_fd, &rset_w);

    int max_fd = listen_fd;

    while (1) {
        rset_c = rset_w;

        int nready = select(max_fd + 1, &rset_c, NULL, NULL, NULL);
        if (nready < 0) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listen_fd, &rset_c)) {
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(struct sockaddr_in));
            socklen_t client_len = sizeof(struct sockaddr_in);

            int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd <= 0) {
                continue;
            }

            char str[INET_ADDRSTRLEN] = {0};
            printf("received from %s at port %d, listen_fd:%d, client_fd:%d\n", inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)),
                ntohs(client_addr.sin_port), listen_fd, client_fd);
            
            if (max_fd == FD_SETSIZE) {
                printf("client_fd --> out of range\n");
                break;
            }
            FD_SET(client_fd, &rset_w);

            if (client_fd > max_fd) {
                max_fd = client_fd;
            }

            printf("listen_fd: %d, max_fd:%d, clietn_fd: %d\n", listen_fd, max_fd, client_fd);

            if (--nready == 0) {
                continue;
            }

        }

        for (int i = listen_fd + 1; i <= max_fd; i++) {
            if (FD_ISSET(i, &rset_c)) {
                char buffer[BUFFER_LENGTH] = {0};
                int ret = recv(i, buffer, BUFFER_LENGTH, 0);
                if (ret < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("data has alread beed read by another thread\n");
                    }
                    FD_CLR(i, &rset_w);
                    close(i);
                } else if (ret == 0) {
                    printf("disconnect %d\n", i);
                    FD_CLR(i, &rset_w);
                    close(i);
                } else {
                    printf("Recv: %s, %d Bytes\n", buffer, ret);
                }
            }
            if (--nready == 0) {
                break;
            }
        }
    }
#elif 1
    struct pollfd fds[POLL_SIZE] = {0};
    int max_fd = listen_fd;

    int i = 0;
    for (i = 0; i < POLL_SIZE; i++) {
        fds[i].fd = -1;
    }
    fds[listen_fd].fd = listen_fd;
    fds[listen_fd].events = POLLIN;

    while (1) {
        int nready = poll(fds, max_fd + 1, -1);

        if (nready < 0) {
            perror("poll");
            continue;
        }

        if ((fds[listen_fd].revents & POLLIN) == POLLIN)  {
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(struct sockaddr_in));
            socklen_t client_len = sizeof(struct sockaddr_in);

            int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd <= 0) {
                continue;
            }

            char str[INET_ADDRSTRLEN] = {0};
            printf("received from %s at port %d, listen_fd:%d, client_fd:%d\n", inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)),
                ntohs(client_addr.sin_port), listen_fd, client_fd);
            
            if (max_fd == POLL_SIZE) {
                printf("client_fd --> out of range\n");
                break;
            }
            fds[client_fd].fd = client_fd;
            fds[client_fd].events = POLLIN;

            if (client_fd > max_fd) {
                max_fd = client_fd;
            }

            printf("listen_fd: %d, max_fd:%d, clietn_fd: %d\n", listen_fd, max_fd, client_fd);

            if (--nready == 0) {
                continue;
            }

        }

        for (int i = listen_fd + 1; i <= max_fd; i++) {
            if (fds[i].revents & POLLIN) {
                char buffer[BUFFER_LENGTH] = {0};
                int ret = recv(i, buffer, BUFFER_LENGTH, 0);
                if (ret < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("data has alread beed read by another thread\n");
                    }
                    fds[i].fd = -1;
                    close(i);
                } else if (ret == 0) {
                    printf("disconnect %d\n", i);
                    fds[i].fd = -1;
                    close(i);
                } else {
                    printf("Recv: %s, %d Bytes\n", buffer, ret);
                }
            }
            if (--nready == 0) {
                break;
            }
        }
    }
#else
    int epfd = epoll_create(1);
    struct epoll_event ev, events[EPOLL_SIZE] = {0};

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    while (1) {
        int nready = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (nready == -1) {
            perror("epoll_wait");
            break;
        }
        int i = 0;
        for (i = 0; i < nready; i++) {
            if (events[i].data.fd == listen_fd) {
                struct sockaddr_in client_addr;
                memset(&client_addr, 0, sizeof(struct sockaddr_in));
                socklen_t client_len = sizeof(client_addr);

                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd <= 0) {
                    continue;
                }

                char str[INET_ADDRSTRLEN] = {0};
                printf("received from %s at port %d, listen_fd:%d, client_fd:%d\n", inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)),
                    ntohs(client_addr.sin_port), listen_fd, client_fd);
                
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                int client_fd = events[i].data.fd;

                char buffer[BUFFER_LENGTH] = {0};
                int ret = recv(client_fd, buffer, BUFFER_LENGTH, 0);
                if (ret < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("data has alread beed read by another thread\n");
                    }
                    close(client_fd);

                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &ev);
                } else if (ret == 0) {
                    printf("disconnect %d\n", client_fd);
                    close(client_fd);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &ev);
                } else {
                    printf("Recv: %s, %d Bytes\n", buffer, ret);
                }
            }
        }
    }

#endif
    return 0;
}
```