# redis io多线程

redis单线程是指logic在单线程中执行。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643265910532-9961c9ca-53cd-4041-8de1-e242f8f512ce.png)

redis io多线程指read、decode、encode、write在io线程池中处理。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643418164802-5e4ffdb3-e69c-4175-b3ef-e8a9e6422a1b.png)

开启多线程的时候，同一个连接的命令还是按顺序处理的吗？

对于多线程，每一个线程都有一个任务队列，redis做了负载均衡，把任务平均分配到每一个线程对应的队列，这里并没有考虑任务是否是同一个连接来的；对于reactor，使用的是request-reply的模式，read后，会将epoll的状态设置为writable，write后，再将状态设置为readable，从io检测层面保证了命令处理的顺序性，不管是否开启多线程。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643416913849-523f8a9f-06c7-482f-8f24-eb67e3c76ad7.png)

```shell
# io-threads 4
# write默认走io线程，因为write需要encode的数据比较大。
# io-threads-do-reads no
```

协议解析，io读写操作，都是在`networking.c`

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643271697774-2183612d-59a6-4b87-81b2-33ff163dc78c.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643271826924-d0ba5504-e279-417f-9eb7-b4e26eb5f98a.png)



线程怎么调度？

通过加互斥锁的方式。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1643272448411-997c8c80-1e60-4be2-a911-93ae9a34b908.png)