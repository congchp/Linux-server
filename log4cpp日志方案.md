# read/write与fread/fwrite

read/write:

- linux底层操作；
- 内核调用，将数据copy到内核，涉及到进程上线文的切换，即用户态到内核态的转换，这是个比较耗性能的操作。

fread/fwrite：

- C语言标准规定的io流操作，建立在write之上
- 在用户层，增加了一层缓冲机制，用于减少内核调用次数，但是增加了一次内存拷贝。

两者之间的关系，见下图：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641362228622-a46e348d-2bd2-488a-a48a-0f287c7ba3fa.png)

对于输入设备，调用fsync/fflush将清空相应的缓冲区，其内数据将被丢弃；

对于输出设备或磁盘文件，fflush只是将数据copy到内核缓冲区，并不能保证数据到达物理设备，因此应该在调用fflush后，调用fsync，确保数据存入磁盘。

对于日志磁盘操作，直接使用write，实时性更高，但是因为每次都需要将数据copy进内核，效率肯定不高；使用fwrite的话，在用户空间增加了一个缓冲区，之后再调用fflush批量写入内核，实时性不如直接使用write，但是如果数据量大的话，整体性能要更好。



# log4cpp原理

## log4cpp日志框架

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641368828930-0533cd5a-f8cd-4d55-a3f5-bae3ec363ac6.png)

log4cpp中最重要的概念有Category、Appender、Layout、Priority

## Category

​    Category翻译为"种类"，具有树型结构。

​    log4cpp有且只有一个root Category，可以有多个子Category。可以通过不同的Category来记录不同模块的日志。

​    Priority、Additivity、Appender、Layout等属性都是跟Category相关的。

## Priority

Priority表示日志的级别，比如设置为`INFO`，表示`INFO`以上级别的log才输出。

## Appender

Appender负责将日志写入相应的设备，比如Console，File，RemoteSyslog等。一个Category可以同时有多个Appender。

## Additivity

每个Category都有一个additivity属性，该属性默认值是true。

- 如果值为true，则该Category的Appender包含了父Category的Appender；
- 如果值为false，则该Category的Appender取代了父Category的Appender。

也可以这么理解：

additivity, 表示sub category的日志，是否要输出到上一级category日志中。

优点：

- 可以分模块打印不同的日志文件；
- 也可以把所有模块的日志也打印到同一个文件。

## Layout

Layout控制输出日志的显示样式。可以自定义日志格式。



log4cpp支持3种风格日志， C， C++

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641277859526-a05ba9a6-35dc-4536-a5b0-0c8d48fb9954.png)

## rollOver

RollingFileAppender, 就是滚动日志，防止单个日志文件过大。

可以设置MaxFileSize和maxBackupIndex。

比如maxBackupIndex设置为5，则最多会存储6个log文件，log以及log1-log5。先写log，之后是log1。如果写满6个文件，就会丢弃最旧的日志。如果log5写满了，则进行以下操作：

1. 删除log5
2. 将其他log文件rename

​     log4->log5

​     log3->log4

​     log2->log3

​     log1->log2

​     log->log1

3. 新建一个log文件

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641374193271-6efc49c6-d3a1-4c15-a3ea-e74a2df114f3.png)

RollingFileAppender::_append调用的也是FileAppender::_apend，只是每次都会去check文件大小，如果超过规定大小，则进行rollOver。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641428248523-9b00eb11-3803-4280-8595-fd7674ae248c.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641432958755-76b64df1-be09-43c4-aae6-25a8ecb85056.png)

## 日志性能分析

​    使用RollingFileAppender，性能要比FileAppender高一些。因为RollingFileAppender，每次都要通过系统调用去check文件大小，所以性能会差一些。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641429191245-495ab67b-15f3-4048-ad61-db7a9a17a89e.png)



​    无论FileAppender，还是RollingFileAppender，记录日志到磁盘，最终使用的都是FileAppender::_apend, 最终调用的是write。每条日志都调用write，性能肯定不会太高。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641429939621-0f3fdac9-07a8-4e50-959b-898e510450fa.png)



​    上面的日志方式，都是同步日志方式，每条日志都调用write。

​    log4cpp提供了一个StringQueueAppender, 可以把日志记录到字符串队列中，之后程序员可以使用另一个线程从该字符串队列中取出日志，将日志进行落盘。可以fwrite和fflush，批量写入磁盘。这就是一种异步日志的方式。

​    异步日志的性能会高一些。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641431573449-27400196-308c-479b-8105-9ff9ee086da7.png)

​    

## log4cpp总结

​    虽然log4cpp性能不是很好，但是log4cpp中的很多设计还是值得我们借鉴的，比如Category、Appender、layout、rollover等等。



- DailyRollingFileAppender

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641359812283-b13cfb3e-5869-4f07-8f21-79b2168c7aa9.png)

- 日志回滚， 文件改名

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641432958755-76b64df1-be09-43c4-aae6-25a8ecb85056.png)

- category，additivity机制
- 多个appender输出机制



### log4cpp性能问题

- 每条日志都调用write，时写入磁盘，这样会影响效率；

可以多行日志累积再写入, 比如100行。可以分为两个线程，一个是日志记录线程，将日志记录到buffer，写入100行notify一次；另一个是日志刷盘线程，使用condition_wait， 带上一个wait_timeout, 比如1秒，唤醒后将日志数据刷盘。

- rollover的时候，每次都读取日志文件大小。

对于读取日志文件大小，可以在FileAppender::_append中记录文件大小，这样就可以不用每次都进行系统调用。

# muduo日志

## 异步日志机制

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641437673481-98fa4a8a-e48a-4882-aa36-3754560b24c9.png)

- 怎么唤醒日志落盘线程读取日志写入磁盘？

使用mutex+condition

- 每次发notify对性能有没有影响？

累积多行发一次notify，wait_condition加上timeout。

- 日志写入磁盘时时批量写入还是单条写入？

批量写入

## 双缓存双队列机制

**双缓存**

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641438361412-48bc4320-81f9-48fb-ab8a-f931f6dc844d.png)

**日志notify问题**

(1) 日志api调用线程，写满一个buffer才发一次notify，插入日志

(2) 日志落盘线程，通过wait_timeout去读取日志，然后写入磁盘。



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641438762951-2b1ef121-6ea8-4c95-8227-25549dbe1792.png)

如果是超时唤醒，如果bufferA此时只有1M数据，并没有达到4M，日志落盘线程是否将它读走呢？肯定是要读走的，进行落盘，不然延迟会有点大。



双缓存，减少buffer分配的次数。bufferA和bufferB不断交换，避免buffer不断分配。



buffer默认是4M一个，写满4M才notify一次。



如果bufferA和bufferB都满了怎么办？

会有一个buffer queue, 就是buffers。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641439066724-7a490138-73ec-4fa9-a541-59279cfd0405.png)

**日志写入线程**

​    写日志时，先check currentBuffer剩余大小是否够存日志，如果够则直接写入buffer；如果不够，将currentBuffer move到buffers队列中，之后check nextBuffer是否为空，如果不为空则将它交换为currentBuffer，如果nextBuffer为空则新分配一个buffer作为currentBuffer，之后将日志写入buffer，最后notify，唤醒日志落盘线程。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641446882138-ce9dafde-bce6-49b1-9ed7-1f9ab1b0104c.png)



**日志落盘线程**

**双队列**buffers，buffersToWrite。

buffersToWrite里面的数据全部落盘

把buffers里面的buffer转移到buffersToWrite里面，转移后就释放锁

如果没有buffersToWrite，落盘的时候需要遍历buffers，会一直锁着buffers，影响日志记录线程写日志。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641447309835-be60ec01-c9f2-446a-88b4-fb21d3ee30ce.png)



**数据落盘的时候不需要加锁，因为只有日志落盘线程去操作buffersToWrite。**

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641448252030-0e8e8f78-036d-4bff-8fbf-11efb754fdfa.png)

如果bufferToWrite缓存太多了，则只保留2个buffer，其他buffer drop掉。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641448348615-e62a6be6-1856-4b9c-883b-c2ecf2370e4a.png)



为什么用fwrite_unlocked? 相比fwrite更快。

因为只有一个线程去操作这个文件fd，用unlock可以提升性能。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641344541772-21d03822-611b-4441-bce3-77286ca08204.png)



## rollover

rollFile的时候，获取文件大小，不需要系统调用。在之前写日志append的时候，就记录了文件的大小，不需要系统调用去读文件大小。相比log4cpp要更好。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641344932965-06dea446-3a06-4fc1-88ed-38c48fc28a31.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641449458856-6282bfb3-990c-4b1b-ae7d-c7f56eecd863.png)

## muduo日志总结

muduo使用的是异步日志方式，双缓存、双队列机制，锁的粒度，批量写入机制，都是可以参考的。