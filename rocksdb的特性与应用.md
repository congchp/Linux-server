# 列族(column family)

列族相当于mysql中的table；多个列族共享一个WAL文件，但有独立的memtable和sst文件；WAL是预写日志，对rocksdb的写操作，都是记录WAL，之后才会写磁盘，当数据写入磁盘后，才会删除WAL中对应的记录；



列族的删除非常快，为什么？

因为它是顺序写的；可以解决redis中bigkey的删除；

# 使用rocksdb的哪些特性？

主要使用rocksdb的squence number、snapshot、磁盘顺序写、列族等特性来实现一些应用；可以实现kv数据库、关系型数据库以及Nosql数据库。

# pika

数据量很大的时候，redis有以下问题：

1. 启动的时候，数据从磁盘加载到内存，数据量大的情况下会很慢；50G数据的话，启动时加载到内存需要大约1.5小时；
2. 主从复制，全量同步，会很慢。



pika就是解决上面的问题；pika实现了redis的协议；pika基于rocksdb，数据都存在磁盘中；50G以上使用pika；pika的效率是redis的80%；pika不支持lua; redis是单线程，pika是多线程；

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1648108862450-d7b71f6b-8722-4050-981b-0470312c305a.png)

## 特点

- 容量大，支持百G数据量的存储
- 兼容redis，不用修改代码即可平滑从redis迁移到pika
- 支持主从(slaveof)
- 完善的运维命令

## Blackwidow

pika是基于rocksdb进行存储，需要支持redis的数据类型，包括string，list，set，hash，zset；但是rocksdb本身只支持kv存储，就需要进行一个映射，将redis的数据类型转换成rocksdb的kv进行存储；blackwidow主要就是进行数据转换的；



对于每一种redis的数据类型，使用一个db进行存储；key使用一个列族，value使用一个列族；

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1648110707253-49272d69-69ac-429b-a05a-100ebf258eff.png)



# myrocks

facebook维护的mysql版本，存储引擎使用rocksdb；

myrocks适合用于大数据量业务或者写密集业务。

## 应用场景

### 大数据量业务

相比innodb，**rocksdb占用更少的存储空间**，具备更高的压缩效率，非常适合大数据量的业务；在大数据量的情况下，rocksdb所占空间比innodb少一半以上；如果使用innodb，需要频繁的进行分表分库，使用rocksdb就可以减少分表分库的次数；

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1648168466599-4ddfc72c-4d77-4fe8-945d-4e37cff88d6f.png)

#### myrocks为什么占磁盘空间比innodb少？

innodb使用B+树进行存储；一旦离散写，B+树会大量分裂，就会造成每一个页存储不满的情况；距统计innodb存储空间，大约有50%的空间浪费；

myrocks使用rocksdb进行存储，LSM-Tree，顺序存储，所以比innodb节省磁盘空间；

### 写密集业务

rocksdb采用追加的方式记录DML操作，**将随机写变成顺序写**；非常适合用在批量插入和更新频繁的业务场景；

## myrocks事务实现

### myrocks和innodb事务比较

|            | myrocks                      | innodb                                                  |
| ---------- | ---------------------------- | ------------------------------------------------------- |
| 锁级别     | 行锁                         | 行锁                                                    |
| 隔离级别   | read commitedrepeatable read | read uncommitedread commitedrepeatable readserializable |
| 并发读异常 | 没有解决幻读问题             |                                                         |

myrocks没有解决幻读问题；幻读问题需要靠gap锁来解决，myrocks主键支持gap锁，辅助索引不支持gap锁，所以没有解决幻读问题；



mysql的事务，是基于MVCC和锁实现的；myrocks也是类似的；

### sequence number

rocksdb中的每一条记录都有一个sequence number，这个sequence number存储在记录的key中；

```
InternalKey:| User key (string) | sequence number (7 bytes) | value type (1 byte) |
```

对于同样的User Key记录，在rocksdb中可能存在多条，但它们的sequence number不同；sequence number是实现事务处理的关键，是MVCC的基础。

### snapshot

snapshot是rocksdb的快照信息，每个snapshot对应一个sequence number；假设snapshot的sequence number为Sa，那么对于此snapshot来说，只能看到`sequence number <= Sa`的记录，`sequence number > Sa`的记录是不可见的。



rocksdb的compact操作会根据已有snapshot信息即全局双向链表来判断哪些记录在compact时可以清理。



判断的大体原则是，从全局双向链表取出最小的snapshot sequence number Sn；如果已删除的老记录的`sequence number <= Sn`, 那么这些老记录在compact时可以清除。

### MVCC

sequence number提供了多版本信息；每次查询时，不需要加锁；而是根据当前的sequence number Sn创建一个snapshot，查询过程中只取小于或等于Sn的最大的sequendce number的记录；查询结束后释放snapshot；这样就实现了MVCC，一致性非锁定读。



**rocksdb的MVCC效率要远高于mysql，为什么？**

mysql的MVCC是基于undolog的，它存在共享表空间，也是使用B+树存储；假设事务begin之后，有100万次update，那么在对该记录进行备份时，需要执行100万次版本回溯，每次都是基于记录上的undo指针对undo页进行随机读，效率很低。



rocksdb对该问题进行了优化，假设原始记录的sequence number为2，该版本即为备份事务可见版本；对于比它更大的版本，在将memtable dump为sst文件，或者对sst文件进行compaction时会删除中间版本，仅保留当前活跃事务可见版本和记录最新的版本；这样提高了快照读的效率，也减少了需占用的存储空间。