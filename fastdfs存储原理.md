本文主要介绍fastdfs客户端的上传下载原理以及以及服务端的网络io模型；主要介绍storage，不涉及tracker；storage和tracker使用的网络io模型是一样的。

# 协议格式

FastDFS采用二进制TCP通信协议。一个数据包由 包头（header）和包体（body）组成。client、tacker、storage之间通信的消息格式，都是这样的。

包头只有10个字节，格式如下：

@ pkg_len：8字节整数，body长度，不包含header，只是body的长度

@ cmd：1字节整数，命令码; 比如上传，下载等；不同的命令，对应的body内容不同

@ status：1字节整数，状态码，0表示成功，非0失败（UNIX错误码）

```c
// tracker\tracker_proto.h TrackerHeader

typedef struct
{
	char pkg_len[FDFS_PROTO_PKG_LEN_SIZE];  //body length, not including header
	char cmd;    //command code
	char status; //status code for response
} TrackerHeader;
```



以STORAGE_PROTO_CMD_UPLOAD_FILE，上传普通文件为例，数据包定义如下：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646286295401-6140b7f6-2a0d-49d6-8a47-b273b6b2aa1f.png)

# 客户端上传原理

fastdfs提供命令进行上传文件操作：

```
fdfs_upload_file <config_file> <local_filename> [storage_ip:port] [store_path_index]
```



客户端上传文件流程如下：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646288701089-72007f19-a2e9-4d13-ba0e-819e55724942.png)

**stat获取文件的状态、大小等**

上传文件，肯定是要判断是否是一个常规文件；并且需要获取文件的大小的。通过linux系统提供的stat函数就可以得到，跟使用stat命令是一样的。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646288507836-afdc57c9-f360-49e0-9c8f-c5185b14016a.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646289677898-87086677-4b47-4312-9982-d7ddff20b448.png)



**storage_do_upload_file**

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646289643244-5b02671b-2e49-45b9-9cf9-289eed414d4a.png)

发送静态资源文件的时候，需要先将文件读入内存，再将内存中的数据send到相应的网络fd。通过使用sendfile完成文件的发送，不再需要两步操作。

sendfile使用mmap，实现零拷贝；

零拷贝，使用的是mmap方式，本质是DMA的方式，不需要CPU参与。普通copy，从磁盘copy数据到内存，需要CPU的move指令。

在进程中有一块区域叫内存分配区，当调用mmap的时候，会把文件映射到对应的区域，操作文件就跟操作内存一样。



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646290132215-bdd5c4d1-074a-4132-973b-f6f04205e83b.png)

fastdfs提供的客户端fdfs_upload_file是通过文件的方式上传，其实fastdfs也可以支持内存方式上传；我们在云盘项目中，就自己参考fastdfs的协议，实现了内存方式上传，减少了保存本地磁盘文件的过程。



发送完文件后，等待服务端返回响应；包含group_name, remote_file_name。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646289309724-c914c148-e294-44eb-b1a0-668c33a7a739.png)

## 断点续传

fastdfs支持断点续传



先使用命令操作

```bash
echo hello > test1.txt
echo world > test2.txt
echo cong > test3.txt

# 先使用fdfs_upload_appender上传 test1.txt
fdfs_upload_appender /etc/fdfs/client.conf test1.txt
得到：group1/M00/00/00/CqgWMGIgcUiEPsHJAAAAADY6MCA314.txt ，在fdfs_append_file的时候需要

# 接着续传 test2.txt
fdfs_append_file /etc/fdfs/client.conf group1/M00/00/00/CqgWMGIgcUiEPsHJAAAAADY6MCA314.txt test2.txt

# 接着续传 test3.txt
fdfs_append_file /etc/fdfs/client.conf group1/M00/00/00/CqgWMGIgcUiEPsHJAAAAADY6MCA314.txt test3.txt

# 在服务器相应的目录下查找对应的文件，用cat读取文件内容。
root@4af22fda6f4b:/home/fastdfs/storage/data/00/00# cat CqgWMGIgcUiEPsHJAAAAADY6MCA314.txt
hello
world
cong
```



断点续传文件分为两个阶段：

1. fdfs_upload_appender 上传第一部分文件，STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE命令；
2. fdfs_append_file 上传其他部分的文件，以STORAGE_PROTO_CMD_APPEND_FILE命令。



需要注意：

- 注意断点续传的顺序性；
- 支持断点续传，但fastdfs并不支持多线程分片上传同一个文件。

# 客户端下载原理

fastdfs下载协议如下：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646294648704-f2f9c1f4-30dd-4976-a591-e6a6c8fe8874.png)



客户端下载流程如下：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646295084575-ec457b34-0a08-4d32-89c7-d3805b0e23ae.png)

下载的时候也支持三种接收方式：

- FDFS_DOWNLOAD_TO_FILE：storage_do_download_file1_ex
- FDFS_DOWNLOAD_TO_BUFF：storage_download_file1

- FDFS_DOWNLOAD_TO_CALLBACK：storage_download_file_ex



fastdfs支持多线程下载， 因为协议支持file_offset和download_bytes; 可以指定每一个线程现在的起始位置和大小。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646295895156-dd77de30-c19f-4541-b8ef-c080639bb048.png)

如果服务端是对单个连接进行限速，那么客户端使用多线程下载可以提升下载速度；如果服务端是对用户名或者ip进行限速，客户端多线程下载效果也不明显。

# 网络io模型

fastdfs网络io使用的是多reactor的模型；分为accept线程，work线程，dio线程；这样设计的优点：

- 将网络io和磁盘io解耦；
- 扩充磁盘的时候，方便定义文件读写线程数量



accept线程接收网络连接，并将连接分配给work线程处理；通过pipe将连接发送给work线程；

work线程处理io请求；每个work线程都有一个epoll；如果需要进行磁盘io操作，则将任务push到队列中，交给dio线程处理；

dio线程进行磁盘io操作，从队列里面取任务；并通过pipe向work线程发送消息。



网络io简单处理流程如下图：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646298057912-8b56710c-07a1-45f1-8d30-d11e71664ab4.png)



fastdfs具体的io处理如下：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646298142601-f59f9374-6c23-4c78-96f4-8fc53c4d81f9.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1646283315385-08e06533-5df7-429a-a0a7-4b5222857395.png)