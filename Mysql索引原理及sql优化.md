# 索引

​    索引分类：主键索引、唯一索引、普通索引、组合索引、全文索引

## 主键索引

​    非空唯一索引，一个表只有一个主键索引；在 innodb 中，主键索引的 B+ 树包含表数据信息；

```sql
PRIMARY KEY(key)
```

## 唯一索引

​    不可以出现相同的值，可以有NULL值；

```sql
UNIQUE(key)
```

## 普通索引

​    允许出现相同的索引内容；

```sql
INDEX(key)
-- OR
KEY(key[,...])
```



## 组合索引

​    对表中的多个列进行索引

```sql
INDEX idx(key1,key2[,...]);
UNIQUE(key1,key2[,...]);
PRIMARY KEY(key1,key2[,...]);
```

## 全文索引

​    将存储在数据库当中的整本书和整篇文章中的任意内容信息查找出来的技术；创建索引关键字 FULLTEXT; 查找时，使用用 match 和 against ；

elesticsearch的全文索引更加强大。

## 主键选择

Innodb为什么一定要有主键？

因为Innodb中，行数据存储在主键的B+树的叶子节点中。

innodb中，每张表有且仅有一个主键。

1. 如果将某列显式设置 PRIMARY KEY ，则该key为该表的主键；
2. 如果没有显式设置，则从非空唯一索引中选择；

- 只有一个非空唯一索引，则选择该索引为主键；
- 有多个非空唯一索引，则选择声明的第一个为主键；

3. 没有非空唯一索引，则自动生成一个 6 字节的 _rowid 作为主键；



# 约束

为了实现数据的完整性，对于innodb，提供了以下几种约束：primary key, unique key, foreign key, default, not null.

## 外键约束

外键用来关联两个表，来保证参照完整性；MyISAM存储引擎本身并不支持外键，只起到注释作用；而innodb完整支持外键；

外键具备**事务性**。

操作父表，子表会联动。

```sql
create table parent (
id int not null,
primary key(id)
) engine=innodb;
create table child (
id int,
parent_id int,
foreign key(parent_id) references parent(id) ON DELETE CASCADE ON UPDATE
CASCADE
) engine=innodb;
-- 被引用的表为父表，引用的表称为子表；
-- 外键定义时，可以设置行为 ON DELETE 和 ON UPDATE，行为发生时的操作可选择：
-- CASCADE 子表做同样的行为
-- SET NULL 更新子表相应字段为 NULL
-- NO ACTION 父类做相应行为报错
-- RESTRICT 同 NO ACTION
INSERT INTO parent VALUES (1);
INSERT INTO parent VALUES (2);
INSERT INTO child VALUES (10, 1);
INSERT INTO child VALUES (20, 2);
DELETE FROM parent WHERE id = 1;
```

最后一条delete语句执行后，也会关联删除child表中的parent_id=1的行数据。

## 约束和索引的区别

约束是逻辑的概念，索引是物理的概念。创建主键索引或者唯一索引的时候，同时创建了相应的约束。每一个索引，都对应一棵B+树。

# 索引实现

## B+树

Mysql中，每个索引对应一棵B+树。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641877219314-a02f8319-f993-4023-8b49-3f8f97ff761f.png)

Mysql为什么要选择B+树呢？为什么不选择红黑树呢？



**红黑树，二叉平衡搜索树**

搜索树，中序遍历有序

平衡：平衡的是树的高度，提供一个稳定搜索时间复杂度。

插入、删除的时候，维持树的高度的平衡。



**B+树，多路平衡排序树**

叶子节点都在同一层，每条链路的高度是一致的。

B+树相对红黑树来说，高度更加平衡，所有叶子节点都在同一层，每条链路的高度是一致的。B+树相对红黑树来说，高度会比红黑树小。**树的高度代表的磁盘io的次数。**更加适合磁盘存储，减少磁盘io次数。叶子节点是一个链表，更加适合范围查询。



红黑树通常用在内存当中，B+树适合用于磁盘存储。



磁盘跟内存： 一次磁盘io大约10ms， 一次内存操作大约100ns，速度相差10万倍。



B+树怎么映射磁盘？

Mysql B+树，默认1页16k， 每一页最少存储两行。如果超过16k怎么办？

存储两行数据，每一行存储6~7k，其他的数据存到共享表空间

一个节点为什么最少要存储两行数据？



B+树的数据只存储在叶子节点中，非叶子节点只存储索引。



支持范围查询，叶子节点是双向链表(磁盘物理地址)，保证快速范围查询。

## B+树层高问题

B+树的一个节点对应一个数据页；B+树的层越高，那么要读取到内存的数据页越多，io次数越多；

innodb一个节点16kB；

假设：

key为10byte且指针大小6byte，假设一行记录的大小为1kB；

那么一个非叶子节点可存下16kB/16byte=1024个（key+point）；每个叶子节点可存储16kB/1kB=16行数据；

结论：

2层B+树叶子节点1024个，可容纳最大行数为： 1024 * 16 = 16384；

3层B+树叶子节点1024 * 1024，可容纳最大行数为：1024 * 1024 * 16 = 16777216；

4层B+数叶子节点1024 * 1024 * 1024，可容纳最大行数为：1024 * 1024 * 1024 * 16 =

17179869184;



有上面数据看出，正常一个表的主键索引B+树，一般是2~4层。

阿里的对于数据库表的建议，一个表超过500万行数据，就要考虑分表分库。

## 聚集索引

Myisam

frm 表信息文件

myd 数据文件 堆表

myi 索引文件 B+树，叶子节点存储索引+行所在数据文件的地址

Myisam每次查询都需要回表查询, 即通过索引查找行数据在数据文件中的地址，之后在根据地址到数据文件中查询行数据。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641817521751-c5567e7d-5a79-483b-a17c-cbd23b7d51eb.png)



Innodb

frm 表信息文件

idb 数据文件 B+树， 聚集索引，叶子节点存储主键id+具体行数据

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641818196428-a72856ff-fba9-41a8-b299-561bc7f45318.png)



聚集索引，是主键创建的索引，使用聚集索引可以查到行数据;

辅助索引，其他索引;

聚集索引、辅助索引是innodb中的概念，Myisam不分聚集索引和辅助索引。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641877185856-2d2b8fdf-1bb3-4bcf-94fd-b788cb3ddca7.png)

## 辅助索引

辅助索引B+树，叶子节点存储索引+主键索引，然后通过主键索引查找行数据。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641877282500-004400e8-cf3c-4343-a23f-e1be1aef7020.png)

## Innodb缓存

下图显示了组成InnoDB存储引擎体系结构的内存中和磁盘上的结构。

内存中包括Buffer Pool、Change Buffer和Log Buffer。

Buffer Pool缓存表和索引数据；

Change Buffer缓存非唯一索引的数据变更（DML操作）；

Log Buffer缓存log数据。



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641878508013-fef1a877-701c-4916-9e0d-d1f4b213476b.png)

### Buffer pool

Buffer pool 缓存表和索引数据；采用 LRU 算法（原理如下图）让 Buffer pool 只缓存比较热的数据 ；

当需要空间以将新页面添加到Buffer Pool时，将淘汰最近使用最少的页面，并将新页面添加到Buffer Pool的中间。此中点插入策略将列 Buffer Pool 视为两个子列 Sublist：

- 最前面是最近访问过的新(“年轻”)页面的Sublist
- 在末尾，是最近访问过的旧页面的Sublist

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641878602787-aff16357-9d4e-434a-bf51-fe8f135a25e3.png)

当InnoDB将页面读入Buffer pool时，它最初将其插入中点(旧Sublistg 的头部)。访问旧Sublist中的页面会使其“年轻”，并将其移至新Sublist 的开头。

随着数据库的运行，通过移至list 的末尾，Buffer pool中未被访问的页面“变旧”。新的和旧的Sublist中的页面都会随着其他页面的更新而老化。随着将页面插入中点，旧Sublist 中的页面也会老化。最终，未使用的页面到达旧Sublist 的尾部并被淘汰。

### Change Buffer

Change Buffer, 缓存对非唯一索引的DML操作，再由io线程异步刷到磁盘。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641880264767-0835f8d7-4917-4b52-b95e-b6c8657c36a9.png)

唯一索引的DML操作，先写日志，再异步刷盘。日志先写到log buffer中，再写日志文件

为什么先写日志，再刷盘呢？

随机io和顺序io的差别。写redo log，是顺序写，1次io；B+树刷盘，大概需要3次磁盘io。

## 最左匹配原则

对应组合索引，如(k1, k2)。比较规则，先比较k1，k1相同才比较k2.

## 覆盖索引

从辅助索引中就能够找到数据，而不需要通过聚集索引查找。不需要回表查询。覆盖索引较大概率有较少磁盘io。



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641861195776-bccf86dd-56ce-4109-84ea-0b55ca2645f5.png)

# 索引失效

- 索引字段参与运算，则索引失效；例如： from_unixtime(idx) = '2021-04-30';
- 索引字段发生隐式转换，则索引失效；例如： '1' 隐式转换为 1 ；

- LIKE 模糊查询，如果使用通配符 % 开头，则索引失效；例如： select * from user where name like '%Mark';
- 在索引字段上使用 NOT <> != 索引失效；如果判断 id <> 0， 可以修改为idx > 0 or idx < 0；

- 组合索引中，没使用第一列索引，则索引失效；
- in + or 索引失效；单独的in 是不会失效的；not in 肯定失效的；

# 索引原则

- 查询频次较高且数据量大的表建立索引；索引选择使用频次较高，过滤效果好的列或者组合；
- 使用短索引；节点包含的信息多，较少磁盘io操作；比如：smallint，tinyint；

- 对于很长的动态字符串，考虑使用前缀索引；

```sql
select count(distinct left(name,3))/count(*) as sel3,
count(distinct left(name,4))/count(*) as sel4,
count(distinct left(name,5))/count(*) as sel5,
count(distinct left(name,6))/count(*) as sel6,
from user;
alter table user add key(name(4));
-- 注意：前缀索引不能做 order by 和 group by
```

- 对于组合索引，考虑最左侧匹配原则和覆盖索引；
- 尽量选择区分度高的列作为索引；该列的值相同的越少越好；

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1641881810574-c10d3b9c-d5e4-49a2-b3c6-3a2d0b51c16c.png)

```sql
select count(distinct idx) / count(*) from table_name;
-- 或者
show index from student;
*************************** 1. row ***************************
Table: student
Non_unique: 0
Key_name: PRIMARY
Seq_in_index: 1
Column_name: id
Collation: A
Cardinality: 7
Sub_part: NULL
Packed: NULL
Null:
Index_type: BTREE
Comment:
Index_comment:
1 row in set (0.00 sec)
-- Cardinality 这个值代表 select count(distinct idx) / count(*) from
table_name;
-- 该值决定了优化器的执行计划的选择；
-- 立马更新 Cardinality 值
analyze table student;
-- 在非高峰时间段，对数据库中几张核心表做 analyze table 操作，这能使优化器和索引更好的为你工作；
```

- 尽量扩展索引，在现有索引的基础上，添加复合索引, 而不是新增索引；最多6个索引
- 不要 select * ； 尽量只列出需要的列字段；方便使用覆盖索引；

- 索引列，列尽量设置为非空；
- 可选：开启自适应 hash 索引或者调整 change buffer；

```sql
select @@innodb_adaptive_hash_index;
set global innodb_adaptive_hash_index=1; -- 默认是开启的
select @@innodb_change_buffer_max_size;
-- 默认值为25 表示最多使用1/4的缓冲池内存空间 最大值为50
set global innodb_change_buffer_max_size=30；
```

为什么主键需要设置成自增？

这样在插入的时候，会有较少的分裂次数。