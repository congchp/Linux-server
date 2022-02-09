# 网络编程相关的posix api包括那些？

posix api, Portable operating system interface

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644285399386-97ac30c8-ea52-4d77-abb8-1d01258d1d51.png)

# socket

**插     座，**由两部分组成

fd， tcb(tcp control block)

调用`socket()`后，得到一个fd和tcb，tcb的状态是close的。

fd是我们操作的；tcb是协议栈的，和tcp连接的生命周期是一致的。



进程第一次创建fd的时候，值为3，因为0-2是标准输入输出。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644286048350-70703320-b9a4-4d1b-9edd-48c8c64cfa40.png)

```plain
[root@4af22fda6f4b webserver]# ll /dev/std*
lrwxrwxrwx 1 root root 15 Jan 30 05:00 /dev/stderr -> /proc/self/fd/2
lrwxrwxrwx 1 root root 15 Jan 30 05:00 /dev/stdin -> /proc/self/fd/0
lrwxrwxrwx 1 root root 15 Jan 30 05:00 /dev/stdout -> /proc/self/fd/1
```

# bind

`bind()`, 绑定本地的ip和端口，确定从哪个地方接收数据。

接收或者发送数据，用来填充本机的ip地址和端口。

# 5元组

`(source ip，source port，dest ip，dest port，proto)`，确定一个连接。

# 建立连接的过程

三次握手发生在哪个posix api里面？过程是怎么样的？

三次握手是发生在协议栈和协议栈之间的。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644366635912-99ef3c18-9810-4835-8f4e-5f054eddad7e.png)

connect是阻塞还是非阻塞？

取决于fd设置为阻塞还是非阻塞。connect是把数据copy到内核中，之后内核协议栈会发起TCP三次握手。

如果connect是阻塞的，阻塞到收到服务器发送的ack，也就是第二次握手的包。

如果是非阻塞的，如何判断连接成功？非阻塞的connect返回成功后，这个fd是可写的。



服务器收到三次握手的第一个包的时候，会做两件事情。

1. 创建一个tcb，放入syn队列；
2. 返回ack给客户端。



服务器收到三次握手的第三个包的时候，需要通过5元组check syn队列，之后加入到accept队列中。如果服务器超时没有收到第三个包，则会重新发送第二次握手的包。

# accept

accept函数做两件事情：

1. 从accept队列中取出一个节点；
2. 为这个节点分配一个fd，返回。

```c
int clientfd = accept();
```

# listen

```c
listen(fd, backlog)
```

unix系统中，backlog 是`syn队列+accept队列`的最大值；

linux系统中，backlog是`accept队列`的最大值。



什么是DDOS攻击？

客户端不断模拟发送三次握手的第一次包，收到服务器的ack包后，也不再发送第三次握手的包，目的是将syn队列占满，使得服务器无法建立正常的连接。

增加backlog对DDOS攻击有用吗？

如果backlog是形容`accept队列`总数的话，没有作用；如果是形容`syn队列+accept队列`总数的话，会有一定作用。

怎样防止DDOS攻击？

前面放一个堡垒机，反向代理。



服务器端口只有65535，为什么能够做到100万连接呢？

因为连接时跟fd->tcb有关系, tcb的唯一性由什么决定呢？是由5元组确定的。

端口是可以复用的，端口只是5元组中的一个元素。

# 数据发送

send只是把用户空间的数据copy到协议栈里面，就是tcb的sendbuffer里面。

TCP中的PSH，表示立刻通知应用程序读取数据。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644302888221-3634733e-5b8c-4fda-b3c8-f450136e41ad.png)

应用程序调用3次send，内核协议栈有可能发送两个包到对端，这就需要在接收方进行拆包粘包。

有两种界定包的完整性的方法：

- 协议头上面加上长度；
- 用特殊字符来对包进行分隔。

这两种方法之所以可行，有一个很大的前提，是包是顺序的，先发的先到。

## 怎么保证顺序？

延迟ack，解决包的有序的问题。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644309187589-b8d4e506-777c-4de4-9b18-5944102d0b58.png)



TCP延迟ack，确认时间长；超时重传，重传的数据较多(**因为之前收到的5号包也进行重传**)，重传费带宽。

弱网环境下，tcp不是那么合适，可以选择udp。

实时性强的场景，可以选择udp。



TCP延迟ack是可以关闭的。



udp使用场景：

1. 游戏，利用UDP的实时性；
2. 迅雷下载，使用UDP传输，抢占带宽；TCP有拥塞控制，对带宽进行控制。



TCP有几个定时器？参考TCP/IP详解

超时重传、TIME_WAIT、坚持定时器、keep-alive定时器。



# 断开连接的过程

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644326783208-caf13a7b-651e-4426-8d2d-ce474deaf694.png)



双方同时close的情况

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644327072047-8b3a2792-d7d8-4673-9e3f-3994a371b2b8.png)

客户端出现大量fin_wait_1，怎么解？

在fin_wait_1不会待太长时间，因为会有超时重传，超时会再次发送fin。



**大部分的情况，客户端可能会出现fin_wait_2，同时服务器出现大量close_wait，是什么原因？**

是因为服务器没有及时调用close。服务器收到client的close，之后处理其他耗时的操作，没有进行close。是业务逻辑的问题，可以先进行close操作，或者把耗时的业务抛到任务队列里面，交给线程池去处理，先把网络这一层处理好。

这也是为什么应用层自定义协议要设计fin位，就是要在TCP close前对业务层进行回收，防止调用收到对端的fin包后，不能及时调用close。



如果出现了last_ack的状态，在这个状态不会很久，只是时间问题，因为有超时重传，不需要解。



如果连接一直卡在fin_wait_2的状态，有没有办法终止它？

理论上只有停掉进程。因为从fin_wait_2的状态只能经过time_wait, 再到close状态。



服务器在close_wait的状态直接宕机了，没有close，那么客户端就是fin_wait_2的状态，怎么解决？

设置keepalived，服务器没有响应，客户端的fin_wait_2会终止掉，会经过time_wait状态，最后会进入close状态。

可以看下内核源码，跟下fin_wait_2状态。



tcb和fd都是什么时候被回收的？

被动方调用close后，fd就回收了，可以分配给其他socket使用；被动方收到最后一个ack，回收tcb；主动方time_wait时间到了之后，回收fd和tcb。



**time_wait状态存在的原因？**

为了避免最后一次ack的丢失。如果最后一次ack丢失，被动端会重新发送fin，如果主动端没有time_wait的状态，而是直接close状态，则重新发送的fin不知道发送给谁。