# 如何选择tracker？

当集群中有多个tracker server时，由于tracker之间是对等的关系，客户端在upload文件时可以任意选择一个tracker。

# 如何选择storage？

当选定group后，tracker会在group内选择一个storage server给客户端，支持如下选择storage的规则：

1. Round robin，在group内的所有storage间轮询；
2. First server ordered by ip，ip最小的storage；

1. First server ordered by priority，按优先级排序（优先级在storage上配置）

# 文件名



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646633909547-a9521be5-6c0b-4f78-8138-1ed5cbb359e5.png)

文件名规则：

- storage_id（storage server ip），源storage server ID或IP地址；
- timestamp（文件创建时间戳），是storage server创建文件的时间戳；

- file_size（若原始值为32位则前面加入一个随机值填充，最终为64位）
- crc32（文件内容的检验码）

- 随机数 （引入随机数的目的是防止生成重名文件）

timestamp，用于文件下载的时候，选择storage server；tracker里面会记录每个storage同步到的timestamp。

# 多tracker，多storage的搭建

- 多个tracker是可以部署在同一台服务器上；实际生产环境肯定是每个tracker在一个机器上，只是学习fastdfs集群的话，可以部署到一台机器上；
- 多个storage一定要部署到**不同**的服务器上，在同一台server上改端口是没有用的。

- 对于client和storage，需要把所有的tracker都添加到conf文件中。



通过`/usr/bin/fdfs_monitor /etc/fdfs/storage.conf`查看集群状态

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646635927994-4802c79c-c488-4164-b7e5-6d4f44977b22.png)



tracker中持久化storage的信息

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646639050530-c417c90f-3d37-4534-8afd-90bc8fb117aa.png)



storage每30秒向tracker报告一次状态；每个storage->tracker都有唯一的线程，连接2个tracker就有2个线程

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646635005997-9aca72a0-8dd2-4510-9ccf-a6453bfdade2.png)



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646455719430-a76b5a5c-db85-4d81-b198-05939362c265.png)

是为了持久化一些信息，下次tracker启动可以加载该文件，获取storage信息；

# storage同步机制

## sync目录

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646456668587-b04756b6-fc70-4170-815c-30d6279fab57.png)



storage中有唯一一份bin文件；bin文件记录storage的增删改操作；

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646641981014-803462eb-7069-4837-b359-66506fee30a4.png)



mark文件可以有多份，group里面如果有N个storage，就有N-1个mark文件；每个storage的同步时独立的，都有一个单独的线程。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646642129424-8d033a87-fee3-40cd-bd25-899866f5ecab.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646641906397-54fc6f89-b8d0-45d5-b82f-2dcad9eca069.png)

storage之间数据同步使用推送的方式，比如A上传了一个新文件，保存完更新binlog；A保存完后推送给B、C；



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646642779150-cbdb6d87-f9f5-4281-a8f2-6ed863e75d0c.png)

## 如何避免循环推送？

源、副本信息的操作对binlog的标记是不一样的；

比如大写标记源，要推送给其他storage；

小写标记副本，不用推送给其他storage。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646643181815-f403650e-89bf-4f91-b66f-52fa966676eb.png)

## 增量同步

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646643426724-4b575a86-2610-4762-8eff-06dfcdfa205a.png)

## 同步规则

\1. 只在本组内的storage server之间进行同步；

\2. 源头数据才需要同步，备份数据不需要再次同步，否则就构成环路了; 源数据和备份数据区分是用binlog的操作类型来区分; 操作类型是大写字母，表示源数据，小写字母表示备份数据；

\3. 当新增加一台storage server时，由已有的一台storage server将已有的所有数据（包括源头数据和备份数据）同步给该新增服务器，即进行全量同步。



可以通过增加group，来扩展存储能力；

对于读多写少的场景，可以增加storage来加强读能力。



## 新增节点的流程

group中已存在storage A和storage B，新增加storage C的流程如下图：

storage A和storage B需要申请作为同步源，最终只有一个storage作为同步源，向C进行全量同步

storage\tracker_client_thread.c， tracker_sync_src_req

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646644237609-458eb2d9-ef26-4d42-abc8-c64ef9cba6d2.png)

## 文件同步的流程

fastdfs\storage\storage_sync.c， storage_sync_copy_file

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646644413557-0e5743ef-d881-4eb4-abf5-bf120599ba18.png)

# 关于send、recv的error，如何处理？

send、recv的error，一般出现在客户端；

对于linux系统，返回值都是-1， 错误码和原因如下表：

| 错误码             | send                            | recv                     |
| ------------------ | ------------------------------- | ------------------------ |
| EGAIN或EWOULDBLOCK | TCP窗口太小，数据暂时发送不出去 | 当前内核缓冲区无数据可读 |
| EINTR              | 被信号中断，需要重试            | 被信号中断，需要重试     |
| 其他错误           |                                 |                          |