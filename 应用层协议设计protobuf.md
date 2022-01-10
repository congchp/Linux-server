# 为什么需要协议设计？

## 什么是协议？

​    协议就是双方的一种约定，通过约定，通信的双方可以对一段数据有相同的理解，从而可以相互协作。

​    我们自定义的协议，一般是在TCP/IP模型的应用层。

## 什么是序列化？

- 序列化： 将数据结构或对象转换成二进制串的过程
- 反序列化：将在序列化过程中所生成的二进制串转换成数据结构或者对象的过程

为什么不能直接使用结构体进行通信呢？主要因为结构体无法跨平台、跨语言。协议中会使用序列化和反序列化。

# 消息帧的完整性判断

为了让对端知道如何给包分界，一般有以下做法：

- 以特定符号来分界，如每个包都以特定字符来结尾，如`\r\n`；
- header+body结构，这种结构一般header部分是一个固定字节长度的结构，并且header中会有一个length字段制定body的大小。收包时，先接收固定长度字节数的header，解析出body的大小，按此长度接收body;

- 在序列化后的buffer前面增加一个header，其中有个字段存储包的总长度，根据特殊字符(比如\n或者\0)判断header的完整性。http和redis协议采用的是这种方式。收到包的时候，先判断已收到的数据中是否包含结束符，收到结束符后解析包头，解出这个包完整长度，按此长度接收包体。

# 协议设计

## 协议设计范例

### 范例1-IM即时通讯

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641694056157-196ce8be-5429-41a8-bcaf-fa80d21add9c.png)

### 范例2-云平台节点服务器

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641694259421-6faa1985-ce07-412b-92fe-eca7665e8e6d.png)

### 常见协议

如Nginx，http，redis等等。



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641694704050-e7dd5d2b-00ca-484e-9650-3188a10caaad.png)



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641694723998-02f66ab8-aa5b-49f2-b7c5-6f4dae8a3125.png)

## 序列化方法

### 常见序列化方法

常见的序列化方法有xml、json、protobuf等

- XML可扩展标记语言(e**X**tensible **M**arkup **L**anguage)。是一种通用和重量级的序列化方法。以文本方式存储。
- JSON(JavaScript Object Notation, JS对象简谱)是一种通用和轻量级的序列化方法。以文本方式存储。

- protobuf是google的私有协议，是一种独立和轻量级的序列化方法。以二进制方式进行存储。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641698431409-d52b185a-752c-42e6-823f-7b69615b9618.png)

### 序列号、反序列化速度对比

序列化、反序列化速度

protobuf  > json > xml



序列化后字节大小

xml > json > protobuf



可读性

xml > json > protobuf

### 常用场景

xml用于本地配置

json，可读性好，用于websocket，http等协议

protobuf，google私有协议，业务内部使用，各个服务器之间rpc调用。

# protobuf使用

protobuf是一种跨平台，跨语言的序列化方法，可用于通信协议，数据存储等。

## protobuf工作流程

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641781403235-b8a1ff5b-4236-4be1-be05-bb62f4ed923a.png)

IDL(Interface Description Language)，接口描述语言。

对于序列化协议来说，使用者只需要关心业务对象本身，即使用IDL定义好数据结构proto文件，可以通过工具生成序列化和反序列化代码。

## protobuf工程经验

proto文件命名：IM.Login.proto, 项目.模块.proto

最后生成C++代码的时候，对应生成namespace IM::Login



字段是通过编号来标识的。字段位置变更，只要编号不变，就不会有影响。



怎样设置repeated成员？自定义类型和内置类型有什么差别？

内置类型的repeated成员的使用：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641782500819-67abc715-ca6e-4a82-9a1f-9fe21870ceb5.png)

自定义类型的repeated成员的使用：

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641782953472-75a1414a-8527-4cda-9eda-45464d6f1d1b.png)



对于单个自定义类型的成员，怎么去设置？

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641783215176-21057dad-df0c-42dc-82ef-19318a148564.png)



设置完结构体中成员的值后，怎么进行序列化？

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641783587172-d466c117-1c75-49a1-a7e2-223f79203ec9.png)



反序列化的时候，怎么知道使用哪个结构来承接二进制数据？

在协议设计的时候，协议头中有字段指明type。接收方需要先解析header，根据header中的type决定使用哪个结构来进行反序列化。



自定义类型成员并且不是设置为repeated则有has_函数，也就是对象可以不包含这个成员。



可以内置timestamp，不需要我们自己去设置

google.protobuf.Timestamp last_updated = 5;



# protobuf的编码原理

## varints编码



​    使用Base128编码。每个字节的最高位不能用来表示value，而是用来表示是否结束。1代表没有结束，0代表结束。

​    每个字节的低7bit表示value，最高位表示是否结束。使用的是小端形式。

比如数字666，以标准的整型存储，其二进制表示为

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641792758527-de81e961-a2e5-4325-a823-888bf8056ef0.png)

而采用varints编码，其二进制形式为，只需要2个字节

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641792812889-cb00143f-173c-4c39-b3eb-90bcf65680c7.png)

编码过程

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641793304866-ff5996b2-5a16-4ea7-968d-d3975a6176c8.png)

解码过程

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641793773392-2e749c73-3175-4a2f-af3f-291f98c39aaa.png)



## zigzag编码

​    负数如果直接使用varint，所占字节会比较大。zigzag对负数进行编码优化。负数推荐使用sint32类型，sint32编码的时候，通过zigzag+varints进行编码，减少所需字节数。

​    如果都是正数，用int32

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641794510981-78534ea0-9841-4640-95d4-7a7fe48b39f6.png)

## protobuf数据组织

protobuf数据由3部分组成：

field_num, wire_type, value

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641795251998-da361a34-b2b1-4a77-bdbe-ac0445928504.png)

field_num, proto文件中的编号, 实际是4bit，最高位标识下一字节是否有效。filed_num也是base128的表示方式。4bit最大值15，如果filed_num大于15，就需要额外的字节来表示，第一个字节的最高为就为1。

wire_type, protobuf的编码类型

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641795216812-1cb18d23-d2f5-4d59-a17f-c3158a374c71.png)



field_num最好不要超过15，超过15，field_num就需要占更多字节。



- 当wire_type等于0的时候整个二进制结构为：

Tag-Value， tag就是（filed_num << 3 | wire_type)

value的编码采用Varints编码方式，故不需要额外的位来表示整个value的长度。因为Varint的msb位标识下一个字节是否是有效的就起到了指示长度的作用。

- 当wire_type等于1、5的时候整个二进制结构也为：

Tag-Value

因为都是取固定32位或者64位，因此也不需要额外的位来表示整个value的长度。

- 当wire_type等于2的时候整个二进制结构为：

Tag-[Length]-Value

因为表示的是可变长度的值，需要有额外的位来指示长度。