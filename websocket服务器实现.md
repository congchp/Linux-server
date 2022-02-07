# websocket是什么，用在哪里？

websocket与http都属于tcp/ip协议簇中的应用层协议，基于tcp协议，主要用于浏览器和服务器通信。

# websocket与http

http是request-reply模式，只能由client先发数据；而websocket中，server可以主动发送数据给client。

http也是可以做长连接的, 只是一般不这么用；websocket一般用作长连接。



对于传输数据不大的情况下，http包头所占字节比较多，消息利用率不高，需要自定义协议。

自定义协议由两部分组成：

1. tcp包本身的信息，比如包长，新建连接，断开连接等，网络相关；
2. 业务协议，json、xml等。

websocket就是解决这个问题的，websocket只定义了tcp包本身的信息，业务协议可以自己去定义，类似自定义协议。

# websocket应用场景

微博即时聊天，qq、微信网页版即时聊天，比分网页版实时，股票行情，可以用websocket来做。

websocket，普遍的使用场景，用于服务器主动推送给浏览器, b/s模式的实时通信。websocket并不仅限于浏览器和服务器之间的实时通信。

# websocket协议格式

1. tcp连接建立后，客户端发送的第一个包是符合http协议头的，handshake。为了兼容http。

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1637135395206-fb051980-ba34-4414-9735-e9aa843cc9fc.png)

2. handshake完成后，再发送的包就不符合http协议了，是websocket协议。

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1637135552371-c92383ad-ef42-45ef-b016-93aaa8e3a44e.png)

websocket协议，参考[rfc6455](https://www.rfc-editor.org/rfc/pdfrfc/rfc6455.txt.pdf)。

# websocket客户端如何验证服务器合法

靠handshake来验证合法性

client发送的第一个包：

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1637135996764-ff80fd1b-d43d-41a4-bfab-d89bd2cb6d7c.png)

`Upgrade`说明是websocket协议

`Sec-WebSocket-Key`是用来进行验证的，具体方法如下：

```plain
// Sec-WebSocket-Key + GUID
str = "nwnCsQI3P/SqZ6I2dyyfFA==258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

sha = SHA1(str);

sec_websocket_accept = base64_encode(sha);
```

之后发送respose给client, 客户端对`Sec_Websocket_Accept`进行验证。

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1637139473384-2610e77b-1ca5-4a15-9194-9499fd2c60e5.png)

# websocket协议

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644130540533-0da73e38-d204-4b8c-a734-9f2d9d234b5d.png)

handshake后，再发送的包就符合websocket协议。包分为两部分，一部分是header，另一部分是payload。http使用特殊字符`\r\n`来界定包；而websocket则是在header中包含`payload length`指定包的长度。具体协议参考[rfc6455](https://www.rfc-editor.org/rfc/pdfrfc/rfc6455.txt.pdf)。



FIN: 1 bit

​      Indicates that this is **the final fragment in a message**.  The first fragment MAY also be the final fragment. 



Opcode:  4 bits 

​      Defines the interpretation of the "Payload data". If an unknown opcode is received, the receiving endpoint MUST _Fail the WebSocket Connection_.  The following values are defined.

- %x0 denotes a continuation frame 
- %x1 denotes a text frame

- %x2 denotes a binary frame
- %x3-7 are reserved for further non-control frames

- %x8 **denotes a connection close**
- %x9 denotes a ping

- %xA denotes a pong
- %xB-F are reserved for further control frames

**0x8表示关闭连接。**



Mask: 1 bit

​      Defines whether the "Payload data" is masked.  If set to 1, a masking key is present in masking-key. All frames sent from client to server have this bit set to 1. 



Payload length:  7 bits, 7+16 bits, or 7+64 bits 

​      The length of the "Payload data", in bytes: if 0-125, that is the payload length. If 126, the following 2 bytes interpreted as a 16-bit unsigned integer are the payload length.  If 127, the following 8 bytes interpreted as a 64-bit unsigned integer (the       most significant bit MUST be 0) are the payload length. 



Masking-key:  0 or 4 bytes 

​      All frames sent from the client to the server are masked by a 32-bit value that is contained within the frame.  This field is present if the mask bit is set to 1 and **is absent if the mask bit is set to 0.** 

 

# 明文与密文如何传输

`mask = 0`，传明文

`mask = 1`，传密文

如果`mask=1`，则websocket的包头中就会有4个字节的`masking-key`

```plain
// transformed-octet-i = original-octet-i XOR masking-key-octet-j
j = i%4 // i MOD 4， masking-key是4个字节
payload[i] ^= masking_key[j]; // 发送端进行一次异或；接收端再次进行异或操作，得到原始内容
```

# websocket如何断开

自定义协议为什么要进行close？

既然TCP有close操作，为什么websocket还要留一个opcode来断开？

这样做可以先在应用层把业务处理完成，然后再调用tcp的close，能做到更好的回收。

# websocket状态机

websocket的状态机，只有时间顺序，没有状态迁移。

handshake->tranmission->end

```c
enum {
	WS_HANDSHAKE = 0,
	WS_TRANSMISSION = 1,
	WS_END = 2,
};
```

# websocket handshake与tcp三次握手关系

tcp三次握手完成后，websocket协议还有一次握手。

# 总结

websocket是一个应用层协议，主要用于服务器主动推送消息给浏览器。websocket要比http简单很多。