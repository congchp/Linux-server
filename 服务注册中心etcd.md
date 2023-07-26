# etcd简介
> etcd官方描述
> A distributed, reliable key-value store for the most critical data of a distributed system.
> etcd is a strongly consistent, distributed key-value store that provides a reliable way to store data that needs to be accessed by a distributed system or cluster of machines. It gracefully handles leader elections during network partitions and can tolerate machine failure, even in the leader node.

> etcd提供一个分布式，高可用、强一致性的kv数据存储服务；
> 用于读多写少的场景，用于存储少量数据；
> 
> etcd诞生于CoreOS公司，基于go语言实现，主要用于共享配置和服务发现；

## etcd v2和v3版本比较
> - v2版本使用http1.1+json通信，v3版本使用grpc(http2.0)+protobuf；v3也支持http+json；
> - v2是简单的kv内存数据库；v3支持事务和多版本并发控制(一致性非锁定读)的磁盘数据库；
> - v3使用lease(租约)替换key ttl自动过期机制；
> - v3是扁平的kv结构；v2是类文件系统的存储结构，是树状的，跟zookeeper类似；

# etcd架构
![image.png](https://cdn.nlark.com/yuque/0/2023/png/756577/1690180496376-cd8cf26c-6865-458d-bcbb-7230dbef7b4f.png#averageHue=%23dcdcdc&clientId=u1bad425f-a672-4&from=paste&height=544&id=u76364d74&originHeight=544&originWidth=1345&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=25373&status=done&style=none&taskId=uddfe1c28-5fc1-4071-b15d-6d3d0ffa64b&title=&width=1345)
> grpc Server：etcd集群节点间以及clent与etcd节点间都是通过grpc进行通信；
> Raft协议：etcd通过raft协议来管理集群；
> wal(write ahead log)：预写式日志，执行写操作前先写wal日志，磁盘顺序写，提升写性能；类似mysql的redo log和rocksdb的wal日志；
> snapshot快照数据：用于其他节点同步主节点数据从而达到一致性的状态；类似redis的rdb，用于主从同步；
> boltdb：是一个单机的支持事务的kv数据库，etcd的事务是基于boltdb的事务实现的；boltdb为每一个key都创建一个B+树，该B+树存储了key所对应的版本数据；


# etcd操作命令
## 设置
```shell
# Puts the given key into the store
PUT key val
--ignore-lease[=false] #updates the key using its current lease
--ignore-value[=false] #updates the key using its current value
--lease="0" #lease ID (in hexadecimal) to attach to the key
--prev-kv[=false] #return the previous key-value pair before modification
```
## 删除
```shell
# Removes the specified key or range of keys [key, range_end)
DEL key
DEL keyfrom keyend
--from-key[=false] #delete keys that are greater than or equal to the given key using byte compare
--prefix[=false] #delete keys with matching prefix
--prev-kv[=false] #return deleted key-value pairs
```

## 获取
```shell
# Gets the key or a range of keys
GET key
GET keyfrom keyend
--consistency="l" #Linearizable(l) or Serializable(s)
--count-only[=false] #Get only the count
--from-key[=false] #Get keys that are greater than or equal to the given key using byte compare
--keys-only[=false] #Get only the keys
--limit=0 #Maximum number of results
--order="" #Order of results; ASCEND or DESCEND(ASCEND by default)
--prefix[=false] #Get keys with matching prefix
--print-value-only[=false] #Only write values when using the "simple" output format
--rev=0 #Specify the kv revision
--sort-by="" #Sort target; CREATE, KEY, MODIFY, VALUE, or VERSION
-w json
```
## 监听
```shell
# Watches events stream on keys or prefixes
WATCH key
-i, --interactive[=false] #Interactive mode
--prefix[=false] #Watch on a prefix if prefix is set
--prev-kv[=false] #get the previous key-value pair before the event happens
--progress-notify[=false] #get periodic watch progress notification from server
--rev=0 #Revision to start watching
```
## 事务
保证多个操作的原子性；
```shell
# Txn processes all the requests in one transaction
TXN if/ then/ else ops
-i, --interactive[=false] #Input transaction in interactive mode
```
![image.png](https://cdn.nlark.com/yuque/0/2023/png/756577/1690254798124-4364445c-9d06-4da7-b733-73e68e05602e.png#averageHue=%23022557&clientId=u1bad425f-a672-4&from=paste&height=246&id=u4905a60b&originHeight=369&originWidth=1351&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=29485&status=done&style=none&taskId=u781bffe4-87bc-4622-9796-07d83781a5c&title=&width=900.6666666666666)
## 租约
```shell
lease grant # 创建一个租约
lease keep-alive # 续约
lease list # 枚举所有的租约
lease revoke # 销毁租约
lease timetolive # 获取租约信息
```
# raft共识算法
> raft是一种分布式一致性算法；
> 
> tikv server通过raft算法保证数据的强一致性；pd server也是通过raft来保证数据的一致性；
> 
> Raft下读写是如何工作的？
> 读写都是通过leader；follower只有选举和备份的作用，读写都不经过follower；learner只有复制的作用，没有选举权；


## leader选举(leader election)
> 两个超时控制选举：
> 1. election timeout, 选举超时，就是follower等待成为candidate的时间，是150ms到300ms的随机值；
> 2. heartbeat timeout，心跳超时，要小于150ms，即小于election timeout；
> 
> 选举超时后，follower成为candidate，开始新一轮选举，election term(任期)加一；follower给自己投上一票，然后向其它节点拉票；如果其他节点还没有投票，那么就投给这个candidate；收到拉票请求后，其它节点重置它的选举超时；candidate获得大多数的选票的时候，就成为leader。leader在心跳超时(heartbeat timeout)内，向其它节点同步自身的变化，follower节点收到通知后给leader回复；当前选举周期一直持续到某个follower节点收不到heartbeat，从而成为candidate。
> 
> follower成为candidate后，需要做以下事情：
> 1. election term加一；
> 2. 给自己投一票；
> 3. 重置election timeout；
> 
follower收到拉票请求后，需要做以下事情：
> 1. 重置election timeout；
> 2. election term加一
> 3. 只会给第一个拉票的candidate投票；
> 
收到heartbeat也需要重置election timeout；
> 

## 日志复制(log replication)
> 所有的写操作，都需要通过leader节点；leader先将写操作写入log(WAL日志)，并没有提交；之后将log发送给follower节点，follower节点会写入自己的WAL日志，之后回复leader节点；leader节点获得大多数节点的应答后，再进行提交，之后将结果回复给client；最后再通知follwer节点落盘；

## 网络分区(network partitions, 脑裂)
> 比如集群一共有5个节点，由于网络故障，分成两个分区A, B；A有3个节点，B有2个节点，原先的leader节点在B中，A会重新选举出一个新的leader；对于A的写操作，由于大多数原则，最终会commit；对于B，因为不满足大多数原则，所以不会commit。
> 
> 网络恢复后，raft如何恢复一致性？
> 网络恢复后，B会回滚未提交的日志，并且会同步A的数据；主要是因为A的election term比B大，最终A中的leader会成为整个集群的leader；
> 
> election term的作用？
> 解决网络分区问题；

# etcd存储原理
## 数据版本号机制
> - raft_term:
> 
leader任期，没进行一轮选举term加一；全局递增，64bits;
> - revision:
> 
etcd 全局key空间版本号，只要有key发生变更，则revision加一；
> revision机制是etcd实现MVCC的基础；
> - kv中的revision：
>    - create_revision
> 
创建key数据时，对应的revision；
>    - mod_revision
> 
修改key的value数据时，对应的revision；
>    - version
> 
key的当前版本号；表示该key的value被修改了多少次；

## etcd存储
![image.png](https://cdn.nlark.com/yuque/0/2023/png/756577/1690271314879-a8c1d309-0310-4a0c-8146-4d75b99c0971.png#averageHue=%23f8f8f8&clientId=u1bad425f-a672-4&from=paste&height=365&id=ubd71ddfb&originHeight=548&originWidth=1259&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=59478&status=done&style=none&taskId=u00814c91-55c1-4c6b-aa9e-0950f2ac78c&title=&width=839.3333333333334)
> etcd为每个key创建一个索引；在磁盘中，使用B+树存储，一个索引对应一棵B+树；B+树的key为revision，value为key对应的值；B+树存储key的revision，从而实现了etcd的MVCC；etcd不会存储所有的revision，会通过定期compaction来清理历史revision，只保留较新的部分revision；
> 
> 为了提高索引的效率，etcd在内存中维持一棵B树；为什么使用Ｂ树，而不是红黑树呢？从磁盘B+树中，可以一次copy一页（16k）到内存Ｂ树中，插入效率更快；
> 
> mysql通过undolog实现MVCC；mysql内存使用自适应hash


# etcd应用
## 服务发现
> etcd作为注册中心；服务提供者向etcd进行注册，即put key value；服务使用者watch对应的key；

![image.png](https://cdn.nlark.com/yuque/0/2023/png/756577/1690272876531-7f58434c-af77-400c-ac08-7171cbfc222b.png#averageHue=%23dec9c0&clientId=u1bad425f-a672-4&from=paste&height=381&id=ueb58d4b9&originHeight=572&originWidth=710&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=436220&status=done&style=none&taskId=uf70c27f5-cf1e-4119-b2d4-ae8362dbca2&title=&width=473.3333333333333)
## 负载均衡
> 多个服务提供者向etcd进行注册，put不同的key，使用同样的prefix；etcd通过心跳检测，确定各个服务提供者的状态，进行负载均衡；

![image.png](https://cdn.nlark.com/yuque/0/2023/png/756577/1690273133496-6a4a9f1d-fac9-4681-90e9-53f484e6225a.png#averageHue=%23dcc5bc&clientId=u1bad425f-a672-4&from=paste&height=349&id=u995ec6ae&originHeight=524&originWidth=696&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=423498&status=done&style=none&taskId=uc312b16a-fbea-4e26-b782-bb5801c403e&title=&width=464)
## 分布式锁
> redis可以通过setnx实现非公平分布式锁；etcd可以实现平台锁；
> 第一个创建key的客户端先获取锁；其他客户端监听版本号刚好小于自己的对象的删除信息；

![image.png](https://cdn.nlark.com/yuque/0/2023/png/756577/1690274613716-126a9b18-f16e-4d33-ab32-fef7b2c04625.png#averageHue=%23f1f1f1&clientId=u1bad425f-a672-4&from=paste&height=320&id=u4ea6e43d&originHeight=480&originWidth=730&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=94138&status=done&style=none&taskId=u4ea774b6-da0d-4c5a-87be-bba0c7f0012&title=&width=486.6666666666667)
