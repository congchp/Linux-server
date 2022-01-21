# redis的网络层

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642569583955-0313cfa1-c63d-46a6-8ae2-43ae111ed3d1.png)

为什么mysql每一条连接对应一个线程？

mysql有网络io，磁盘io，涉及的操作比较多，需要有一个单独的线程来处理。使用单线程或者线程池都不太合适。

redis使用的是单reactor网络模型，因为redis都是网络io，单reactor可以满足。

# redis pipeline

redis pipeline 是由客户端提供的，而不是服务端提供的，是将多条命令一起发送到redis。redis是单线程的，所以会顺序执行pipeline中的命令，但是pipeline不具备事务性，所以并不会保证这些命令会一起执行。

pipeline一般和事务一起使用，让pipeline中的多个命令一起执行，不会被其他命令打断。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642573322410-dc198f60-171f-4c4a-b977-83d8cbca2adb.png)

# redis事务

## MULTI EXEC事务

redis命令事务与pipeline一起使用，做一些简单的工作，让几个命令一起执行。

### MULTI

开启事务

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642571479891-e6acc485-d4b9-4085-8044-910a296c19f0.png)

queued, 表示加入一个队列，并没有执行。

### EXEC

提交事务

### DISCARD

取消事务

### WATCH

事务开始前使用watch监测key的变动，如果事务过程中监测到key有变化，则事务提交的时候会先检测key是否有变动，如果有变动则返回nil，并不会执行事务中的命令。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642572038642-fbeb9fbe-4339-41d6-b75a-4a76d32275de.png)

## lua脚本

### lua脚本解决什么问题？

lua脚本主要是来实现原子性。

redis中加载了一个 lua 虚拟机；用来执行 redis lua 脚本；redis lua 脚本的执行是原子性的；当某个脚本正在执行的时候，不会有其他命令或者脚本被执行。有pipeline的效果，lua中可以写多个语句。

redis lua与mysql存储过程类似。

redis lua脚本具有原子性；mysql存储过程不具备原子性。

mysql存储过程可以写很多sql语句，减少网络传输，可以实现简单的逻辑运算；

lua脚本具备原子性，因为执行lua脚本是一个命令，redis是一个单线程，所以具备原子性，可以通过lua实现逻辑进行回滚。

lua脚本解决了哪些问题？

1. 减少网络传输；
2. 实现原子性。



### lua脚本使用

1. 使用script load将lua脚本传到redis
2. evalsha执行lua脚本

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642665193636-a45e1959-979a-4bfd-bc29-c5fbe9cdf899.png)





# ACID分析

## 原子性

原子性要保证要么全部成功，要么全部失败。

redis事务不支持原子性

在EXEC前，只是入队，并不做检查命令是否能执行，EXEC后，如果有命令执行失败，后面的命令仍然会执行。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642655969593-e7ac50a4-a4ce-4141-a11d-98f1e4d2c515.png)

使用lua脚本，可以通过逻辑判断，自己写回滚保证原子性。

## 一致性

一致性指不破坏完整性约束。redis事务也不满足严格意义的一致性。

## 隔离性

事务的操作不被其他用户操作所打断;redis 是单线程执行，天然具备隔离性。

## 持久性

redis支持4种持久化方式。aof、rdb、aof复写、aof-rdb混用。

只有aof方式是在单线程中进行磁盘io；其他三种是采用fork进程的方式进行持久化。

# redis pub/sub

为了支持消息的多播机制，redis 引入了发布订阅模块。类似一个分布式消息队列。消息不一定可达，如果连接断开了，对于该连接来说，pub的消息就丢失了。

# redis异步连接

同步连接方案采用阻塞io来实现。优点是代码书写是同步的，业务逻辑没有割裂；缺点是阻塞当前线程，直至redis返回结果。通常用多个连接实现连接池来解决效率问题。

异步连接方案采用非阻塞io来实现；优点是没有阻塞当前线程，redis 没有返回，依然可以往redis发送命令；缺点是代码书写是异步的（回调函数），业务逻辑割裂，可以通过协程解决(openresty，skynet)。

## 如何实现异步连接？

hiredis提供了async的方法，但只是读写的操作，实现了redis协议。如果要实现完整的异步连接，需要在hiredis的基础上，结合我们自己实现的网络库(reactor), 一起实现异步连接。

reactor主要实现io检测，当检测到有io可读可写的时候，调用hiredis的异步读写方法。需要实现几个回调函数：addRead，delRead，addWrite ，delWrite等，这几个方法主要是用来检测io的, 通过epoll_ctl设置需要检测的io事件。

hiredis里面提供了一些常用的网络库(如libevent)，实现redis异步连接的example。可以参考它们，实现我们自己的redis异步连接。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642648830382-ed2a51f4-ee92-4d61-aace-eb96f20de2c8.png)

1. read、write注册时的listen事件，还是读写io？
2. 异步请求的reactor中的event，都是对应同一个fd，这个OK吗？

同时发出多个request，操作的其实是reactor同一个event，并且会把这个fd的设置为检测读事件。等到读事件发生，怎么找到response对应的是哪个request呢？因为这是个单线程，request肯定是有顺序的，redis对于同一个连接的命令处理也是顺序的，所以按顺序就能够找到是对应的是哪个命令。



主要的工作就是对hiredis进行适配，用我们自己reactor的io检测方法适配hiredis的方法，需要比较方法，比较参数。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642745441861-fb149928-3188-4cec-b075-c042a23bce4f.png)

比较巧妙地使用struct中成员的地址，得到struct地址的方法：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642744351364-3112d226-6fba-4e64-a9a0-296bdbad584c.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642744727129-b81dbac6-a145-4bc1-9378-62037b4ed596.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642745909696-3611ead8-c8d5-4624-98f4-064cc75d799e.png)