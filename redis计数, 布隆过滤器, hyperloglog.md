# redis计数

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644799023374-9978f663-e55d-4093-9fe2-5dd86898027b.png)

# 布隆过滤器

## redis扩展

redis通过对外提供一套API和一些数据结构，可以供开发者开发自己的模块并加载到redis中。

### 本质

在不侵入redis源码的基础上，提供一种高效的扩展数据结构的方式。

### API及数据结构

参考redismodule.h

## RedisBloom

RedisBloom是redis的一个扩展，我们主要使用了它的布隆过滤器。

关于布隆过滤器的原理，参考[《hash，bloomfilter，分布式一致性hash》](https://blog.csdn.net/congchp/article/details/122882760)

### 加载到redis中的方法

```shell
git clone https://github.com/RedisBloom/RedisBloom.git
cd RedisBloom
git submodule update --init --recursive
make
cp redisbloom.so /path/to

# 命令方式加载
redis-server --loadmodule /path/to/redisbloom.so

#配置文件加载
vi redis.conf
# loadmodules /path/to/redisbloom.so
```

### 命令

redis计数中的使用方法如下：

```shell
# 为 bfkey 所对应的布隆过滤器分配内存，参数是误差率以及预期元素数量，根据运算得出需要多少hash函数
以及所需位图大小
bf.reserve bfkey 0.0000001 10000
# 检测 bfkey 所对应的布隆过滤器是否包含 userid
bf.exists bfkey userid
# 往 key 所对应的布隆过滤器中添加 userid 字段
bf.add bfkey userid
```

# hyperloglog

布隆过滤器提供了一种节省内存的概率型数据结构，它不保存具体数据，存在误差。

hyperloglog也是一种概率型数据结构，相对于布隆过滤器，使用的内存更少。redis提供了hyperloglog。在redis实现中，每个hyperloglog只是用固定的12kb进行计数，使用16384(![img](https://cdn.nlark.com/yuque/__latex/bd2aadec076ac659d03c5fa1c50679e1.svg))个桶子，标准误差为`0.8125%`，可以统计![img](https://cdn.nlark.com/yuque/__latex/5212463e37406b73b693fe832f7bc8c2.svg)个元素。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644757837969-0bb8b54d-ac8b-41d9-98d2-defd001a05a2.png)

## 本质

使用少量内存统计集合的基数的近似值。存在一定的误差。

HLL的误差率：![image](https://user-images.githubusercontent.com/38756973/153783133-ed6e212b-df9c-4213-8f8f-1803cb3dd7da.png), m是桶子的个数；对于redis，误差率就是`0.8125%`。

## 原理

HyperLogLog 原理是通过给定 n 个的元素集合，记录集合中数字的比特串第一个1出现位置的最大值k，也可以理解为统计二进制低位连续为零（前导零）的最大个数。通过k值可以估算集合中不重复元素的数量m，m近似等于 ![img](https://cdn.nlark.com/yuque/__latex/fe401f62231ac24e3399751a415a4eaa.svg)。

但是这种预估方法存在较大误差，为了改善误差情况，HyperLogLog中引入分桶平均的概念，计算 m 个桶的调和平均值。下面公式中的const是一个修正常量。

![img](https://cdn.nlark.com/yuque/__latex/cb99b2b04d14ab037afd98bf9f8cff09.svg)

![img](https://cdn.nlark.com/yuque/__latex/29ee154ebdfef8205456e8f2bc2a3ac0.svg): 每个桶的估算值

![img](https://cdn.nlark.com/yuque/__latex/174a2a7a44ffd5b0da59b01c3ce85a64.svg)：所有桶估值计值的调和平均值。

redis的hyperloglog中有16384(![img](https://cdn.nlark.com/yuque/__latex/bd2aadec076ac659d03c5fa1c50679e1.svg))个桶，每一个桶占6bit；

hash生成64位整数，其中后14位用来索引桶子；前面50位用来统计低位连续为0的最大个数, 最大个数49。6bit对应的是![img](https://cdn.nlark.com/yuque/__latex/71280ad2e4fc24739bbf2e84f1764136.svg)对应的整数值64可以存储49。在设置前，要设置进桶的值是否大于桶中的旧值，如果大于才进行设置，否则不进行设置。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1644758047023-0aab891a-0d85-4179-97e0-4e8c2d446549.png)

## 去重

相同元素通过hash函数会生成相同的 64 位整数，它会索引到相同的桶子中，累计0的个数也会相同，按照上面计算最长累计0的规则，它不会改变该桶子的最长累计0；

## 存储

redis 中的 hyperloglog 存储分为稀疏存储和紧凑存储；

当元素很少的时候，redis采用节约内存的策略，hyperloglog采用稀疏存储方案；当存储大小超过3000 的时候，hyperloglog 会从稀疏存储方案转换为紧凑存储方案；紧凑存储不会转换为稀疏存储，因为hyperloglog数据只会增加不会减少（不存储具体元素，所以没法删除）；

## 命令

```shell
# 往key对应的hyperloglog对象添加元素
pfadd key userid_1 userid_2 userid_3
# 获取key对应hyperloglog中集合的基数
pfcount key
# 往key对应过的hyperloglog中添加重复元素。已存在，并不会添加
pfadd key userid_1
pfadd key1 userid_2 userid_3 userid_4
# 合并key，key1到key2，会去重
pfmerge key2 key key1
```
