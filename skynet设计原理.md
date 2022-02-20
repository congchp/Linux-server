skynet是一个轻量级的游戏服务器框架；实现了actor的并发模型；可以基于skynet框架去实现业务。

# 多核并发编程

## 多线程

在一个进程中开启多线程，为了充分利用多核，一般设置工作线程的个数为 cpu 的核心数；

memcached 就是采用这种方式；

多线程在一个进程当中，所以数据共享来自进程当中的内存；这里会涉及到很多临界资源的访问，所以需要考虑加锁；

## 多进程

在一台机器当中，开启多个进程充分利用多核，一般设置工作进程的个数为 cpu 的核心数；

nginx 就是采用这种方式；

## CSP

以 go 语言为代表，并发实体是协程（用户态线程、轻量级线程）；内部也是采用多少个核心开启多少个内核线程来充分利用多核；

## actor

erlang 从语言层面支持 actor 并发模型，并发实体是 actor(用户态进程)；skynet采用 c + lua来实现 actor 并发模型, skynet中的actor也叫服务；底层也是通过采用多少个核心开启多少个内核线程来充分利用多核；

## 总结

不要通过共享内存来通信，而应该通过通信来共享内存。

# actor定义

为什么要抽象进程？

每一个进程就是一个运行实体，这些运行实体提供了相互隔离的运行环境。

```c
// 服务的定义，就是actor
struct skynet_context {
	void * instance; // 隔离的运行环境，一块内存或者lua虚拟机
	struct skynet_module * mod; // 服务的启动文件
	void * cb_ud;
	skynet_cb cb; // callback，通过调用cb来运行actor
	struct message_queue *queue; // 消息队列，按照消息到达的先后顺序执行它
	ATOM_POINTER logfile;
	uint64_t cpu_cost;	// in microsec
	uint64_t cpu_start;	// in microsec
	char result[32];
	uint32_t handle; // actor的id
	int session_id;
	ATOM_INT ref; // 引用计数
	int message_count; // actor处理了多少个消息
	bool init; // 是否初始化
	bool endless; // 是否陷入死循环
	bool profile; // 是否调试状态

	CHECKCALLING_DECL
};
```



# actor组成部分

1. 隔离环境，主要是lua虚拟机，也有可能是一块内存
2. 回调函数，帮助我们运行actor；在C语言层面，只有一个回调函数；在lua层面，有多个回调函数，不同类型的消息调用不同的回调函数，比如网络消息、actor之间的消息；

1. **消息队列**

# actor消息

消息的类型：actor之间的消息，网络消息、定时消息。

## 网络消息的事件如何与actor绑定？

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645162987813-b1fc2f9c-a89d-4bca-831a-bb107a401ec1.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645163191630-c5e4e05e-47e4-4bc7-b50a-79ae5ca5cf0a.png)

skynet以消息的方式运行，所有的任务都以消息的方式进行传递。

skynet 通过 socket.start(fd, func) 来完成 actor 与 fd 的绑定；

底层epoll中，是通过`epoll_ctl `设置` struct epoll_event `中 `data.ptr = (struct socket *)ud`; 来完成fd 与 actor绑定；

## actor之间的消息

消息怎么一一对应上？

一个actor要往另一个actor中发消息，只是将消息放入目的actor的消息队列中，是一个move的操作。

## 定时消息

skynet中有一个单独线程来处理定时任务，使用时间轮算法。

当定时任务被触发，会将定时任务包装成消息，传递给产生这个任务的actor，从而驱动actor的运行。

# actor的调度

actor的运行是由线程池驱动的。

## 线程池的调度



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645323278396-2e29ecc9-0f00-4ae3-b194-5b46477c3a18.png)

线程调度方式，是指队列中的任务**从无到有**，是怎么处理的, **消费线程如何调度**? 需要怎么唤醒消费者线程？如果任务队列中的任务**从有到无**，需要**怎么让消费者线程休眠**？



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645325406780-9e4bcacc-1d65-4ca0-9c91-7d1c21f126b4.png)

消息生产者：网络线程、定时线程、工作线程

消息消费者：工作线程



### 工作线程

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645325652167-acb46b83-a17b-4d75-8390-e0003098a129.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645326818405-08dee8ee-87cd-456b-bbf8-7962682955ee.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645327493020-8e105401-3043-4d0c-859c-123ac004e86d.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645328082037-eead7760-1ab0-41d8-9691-0e77d4d0c562.png)

memcached使用的自旋锁

nginx解决惊群，使用自旋锁(cas实现自旋锁)，锁存放在共享内存中。



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645328517835-2be3a628-2ef8-45c8-894a-85e2e0ef1d2d.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645328607122-b315a904-cb80-44a2-b1b4-cecceb35c4f9.png)

#### 线程调度方式

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645329170933-b935e99f-a236-4ce5-968a-317474ec988a.png)

什么时候调用pthread_cond_signal?

有消息产生的时候，需要signal。



什么时候调用pthread_cond_broadcast?

线程池要退出的时候，需要broadcast。



signal在哪里调用？

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645333114160-750612a5-c193-4728-bbda-aad07f8d45f1.png)



网络线程产生消息，需要wakeup

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645171864037-3f7eb610-c9de-43e9-93c7-970232378ba8.png)



定时线程产生消息，需要wakeup

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645171913305-b66f4a21-2e2f-4849-b607-7768311c6acf.png)



actor之间的消息，不需要wakeup

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645333699826-d7383c42-358b-4275-b69b-058f370b6b61.png)



## actor的调度

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645334139269-97e5dc4f-4590-445f-9892-83ec94e073a0.png)

worker线程池针对的是全局消息队列，全局消息队列组织的是**活跃的actor的消息队列**；活跃的actor是指actor的消息队列中有消息

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645334543787-cc1bfedd-224a-4304-9f83-755ba357c54a.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1645175182741-cc865523-f6e7-4dac-bd3e-1b2c759bcc2e.png)



# 协程

为什么要引入协程？

需要用协程来消除回调。



skynet，一个消息对应一个协程；openresty，一个请求对应一个协程。