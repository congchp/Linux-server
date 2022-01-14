# 事物

## 目的

事务将数据库从一种一致性状态转换为另一种一致性状态；

## 组成

事务可由一条非常简单的SQL语句组成，也可以由一组复杂的SQL语句组成；

Innodb支持事务，Myisam是不支持事务的。这个是Myisam和Innodb一个主要的区别。

对于一条SQL语句，Innodb默认是加上事务的。

**Myisam为什么不支持事务？**

因为Myisam不支持行锁，只支持表锁。

**Myisam为什么不支持表锁呢？**

因为Myisam B+树叶子结点存储索引+数据在磁盘中的位置，同时有多个B+树，但是并没有聚集索引和辅助索引的概念，可以锁住一颗B+树中的某一个数据，但是无法锁住其他B+树的行数据，所以不能实现行级锁。对于Innodb，查询的时候如果使用辅助索引，之后也要进行一个回表查询，再到聚集索引B+数中找到行数据，所以很容易实现行级锁。

## 特征

在数据库提交事务时，可以确保要么所有修改都已经保存，要么所有修改都不保存；

事务是访问并更新数据库各种数据项的一个程序执行单元。

在 MySQL innodb 下，每一条语句都是事务；可以通过 set autocommit = 0; 设置当前会话手动提交；

## 事务控制语句

```sql
-- 显示开启事务
START TRANSACTION | BEGIN
-- 提交事务，并使得已对数据库做的所有修改持久化
COMMIT
-- 回滚事务，结束用户的事务，并撤销正在进行的所有未提交的修改
ROLLBACK
-- 创建一个保存点，一个事务可以有多个保存点
SAVEPOINT identifier
-- 删除一个保存点
RELEASE SAVEPOINT identifier
-- 事务回滚到保存点
ROLLBACK TO [SAVEPOINT] identifier
```

# ACID特性

## 原子性(A)

事务操作要么都做（提交），要么都不做（回滚）；事务是访问并更新数据库各种数据项的一个程序执行单元，是不可分割的工作单位；

通过 undolog 来实现回滚操作。undolog 记录的是事务每步具体操作，当回滚时，回放事务具体操作的逆运算;

## 隔离性(I)

事务的隔离性要求每个读写事务的对象对其他事务的操作对象能相互分离，并发事务之间不会相互影响，设定了不同程度的隔离级别，通过适度破环一致性，得以提高性能；

对于多个连接是并发处理的。会存在同时执行的事务，怎么保证互相不影响呢？

隔离性是通过MVCC和锁来实现。MVCC 是多版本并发控制，主要解决一致性**非锁定读**，通过记录和获取行版本，而不是使用锁来限制读操作，从而实现高效并发读性能。锁用来处理并发 DML 操作；数据库中提供粒度锁的策略，针对表（聚集索引B+树）、页（聚集索引B+树叶子节点）、行（叶子节点当中某一段记录行）三种粒度加锁；

## 持久性(D)

事务提交后，事务DML操作将会持久化（写入 redolog 磁盘文件 哪一个页 页偏移值 具体数据）；即使发生宕机等故障，数据库也能将数据恢复。redolog 记录的是物理日志；

## 一致性(C)

一致性指事务将数据库从一种一致性状态转变为下一种一致性的状态，在事务执行前后，数据库完整性约束没有被破坏；**一个事务单元需要提交之后才会被其他事务见。**一致性由原子性、隔离性以及持久性共同来维护的。

外键、触发器都支持事务。

# 隔离级别

隔离级别越高，并发性能越低。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642034504464-cdbaadc5-3f3c-498e-a92b-e3ecd37d1315.png)



Mysql默认隔离级别REPEATABLE READ

## READ UNCOMMITTED

读未提交；该级别下读不加锁，写加排他锁，写锁在事务提交或回滚后释放锁；

## READ COMMITTED

读已提交（RC）；从该级别后支持 MVCC (多版本并发控制)，也就是提供一致性非锁定读；此时读操作读取历史快照数据；该隔离级别下读取历史版本的最新数据，所以读取的是已提交的数据；

## REPEATABLE READ

可重复读（RR）；该级别下也支持 MVCC，此时读操作读取事务开始时的版本数据；

## SERIALIZABLE

可串行化；该级别下给读加了共享锁；所以事务都是串行化的执行；此时隔离级别最严苛；

## 命令

```sql
-- 设置隔离级别
SET [GLOBAL | SESSION] TRANSACTION ISOLATION LEVEL REPEATABLE READ;
-- 或者采用下面的方式设置隔离级别
SET @@tx_isolation = 'REPEATABLE READ';
SET @@global.tx_isolation = 'REPEATABLE READ';
-- 查看全局隔离级别
SELECT @@global.tx_isolation;
-- 查看当前会话隔离级别
SELECT @@session.tx_isolation;
SELECT @@tx_isolation;
-- 手动给读加 S 锁
SELECT ... LOCK IN SHARE MODE;
-- 手动给读加 X 锁
SELECT ... FOR UPDATE;
-- 查看当前锁信息
SELECT * FROM information_schema.innodb_locks;
```

# MVCC

多版本并发控制；用来实现一致性的非锁定读；非锁定读是指不需要等待访问的行上X锁的释放；

**MVCC作用是通过读快照的方式，提升读并发性能**

在 read committed 和 repeatable read下，innodb使用MVCC；然后对于快照数据的定义不同；在 read committed 隔离级别下，对于快照数据总是读取被锁定行的最新一份快照数据；而在repeatable read 隔离级别下，对于快照数据总是读取事务开始时的行数据版本；

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642129946005-f1861488-09cc-4c5d-8068-92ec1f4ca0e1.png)

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642034720518-1fcdb7d1-3929-4afb-9b11-240818211bd9.png)



# 锁

锁机制用于管理对共享资源的并发访问；用来实现事务的隔离性。

## 锁类型

Myisam是表锁

Innodb支持行锁

Mysql当中事务采用的是粒度锁；针对表（B+树）、页（B+树叶子节点）、行（B+树叶子节点当中某一段记录行）三种粒度加锁；

共享锁和排他锁都是行级锁；

意向共享锁和意向排他锁都是表级别的锁；

## 共享锁(S)

事务读操作加的锁；对某一行加锁；

- 在 SERIALIZABLE 隔离级别下，默认帮读操作加共享锁
- 在 REPEATABLE READ 隔离级别下，需手动加共享锁，可解决幻读问题；

- 在 READ COMMITTED 隔离级别下，没必要加共享锁，采用的是 MVCC；注意该级别下不支持gap锁
- 在 READ UNCOMMITTED 隔离级别下，既没有加锁也没有使用 MVCC；

## 排他锁(X)

事务删除或更新加的锁；对某一行加锁；

在4种隔离级别下，都添加了排他锁，事务提交或事务回滚后释放锁；

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642035816868-e6aee49b-3fa7-40b6-b27f-63d9e6120bde.png)



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642037548339-465ca99c-f0a1-4fa2-aca1-d9481f1c9568.png)

已有行数据1，4。事务一插入2，不commit；事务二插入3，commit插入成功。他们插入的都是insert intention lock，并且对不同的行加X锁，insert intention lock是兼容的，索引不会冲突，插入成功。



![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642038818941-0b5d23b2-a9bd-4bf8-aeba-c4cdc2e3d187.png)

auto-increment

事务一插入一行数据，没有commit；事务二插入另一条数据，commit插入成功

auto-inc lock插入后立即释放，不需要等commit，所以插入成功。

## 意向锁

目的：为了告诉其他事务，此时这张表被一个事务访问。其他事务不需要遍历整张表，来看是否有锁。

作用：排除表级别的锁

## 锁的兼容性

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642140693629-cb75871c-4456-4b0f-91b3-a5006741fa61.png)

# 锁算法

## Record Lock

记录锁，单个行记录上的锁；

## Gap Lock(重点)

间隙锁，锁定一个范围，但不包含记录本身；全开区间；REPEATABLE READ级别及以上支持间隙锁；

如果 REPEATABLE READ 修改 innodb_locks_unsafe_for_binlog = 1 ，那么隔离级别相当于

退化为 READ COMMITTED；

```sql
-- 查看是否支持间隙锁，默认支持，也就是 innodb_locks_unsafe_for_binlog = 0;
SELECT @@innodb_locks_unsafe_for_binlog;
```

## Next-Key Lock

记录锁+间隙锁，锁定一个范围，并且锁住记录本身；左开右闭区间；REPEATABLE READ级别及以上支持

## Insert Intention Lock

​    作用：**提高间隙插入的并发性能**

​    插入意向锁，insert操作的时候产生；在多事务同时写入不同数据至同一索引间隙的时候，并不需要等待其他事务完成，不会发生锁等待。

​    假设有一个记录索引包含键值4和7，两个不同的事务分别插入5和6，每个事务都会产生一个加在4-7之间的插入意向锁，获取在插入行上的排它锁，但是不会被互相锁住，因为数据行并不冲突。

​    如果没有插入意向锁，就需要使用间隙锁，第二个事务就会被阻塞，并发性能无法提升。有了意向锁，5和6就可以并发插入。

## AUTO-INC Lock(AI锁)

​    **较低概率造成B+树的分裂, 提升并发性能。**

​    自增锁，是一种特殊的表级锁，发生在 AUTO_INCREMENT 约束下的插入操作；采用的一种**特殊的表锁**机制；完成对自增长值插入的SQL语句后**立即释放(而不是等commit后释放)**；在大数据量的插入会影响插入性能，因为另一个事务中的插入会被阻塞；从MySQL 5.1.22开始提供一种轻量级互斥量的自增长实现机制，该机制提高了自增长值插入的性能；

## 锁兼容

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642042640868-435b487b-5426-4b45-80f3-4e84799beea0.png)

横向：表示已经持有的锁；纵向：表示正在请求的锁；

一个事务已经获取了插入意向锁，对其他事务是没有任何影响的；

一个事务想要获取插入意向锁，如果有其他事务已经加了 gap lock 或 Next-key lock 则会阻塞；**这个是重点，死锁之源；**

# 锁的对象

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642043411442-4f1ba52b-f7a6-4d11-b62f-1c045e0eb6c3.png)

行级锁是针对表的索引加锁；索引包括聚集索引和辅助索引；

表级锁是针对页或表进行加锁；

重点考虑 InnoDB 在 read committed 和 repeatable read 级别下锁的情况；

如下图 students 表作为实例，其中 id 为主键，no（学号）为辅助唯一索引，name（姓名）和age（年龄）为二级非唯一索引，score（学分）无索引。

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142499044-7eb143f3-bea6-414e-9084-44f33c5229db.png)

**分别讨论**

- 聚集索引，查询命中： UPDATE students SET score = 100 WHERE id = 15;

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142545196-ee8805ce-380d-4c38-beb4-0b5da5f24f7a.png)

- 聚集索引，查询未命中： UPDATE students SET score = 100 WHERE id = 16;

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142592575-4fa88225-45d4-40c5-8bc8-0efa0a9a9e40.png)

- 辅助唯一索引，查询命中： UPDATE students SET score = 100 WHERE no = 'S0003';

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142627867-90df0b42-30d0-4956-94e9-60807e45e2bd.png)

- 辅助唯一索引，查询未命中： UPDATE students SET score = 100 WHERE no = 'S0008';

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142669689-1070db08-55fb-4cb2-9a3c-38de6e537607.png)

- 辅助非唯一索引，查询命中： UPDATE students SET score = 100 WHERE name = 'Tom';

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142709164-aa3dd944-c4c9-48df-90af-161964673eec.png)

- 辅助非唯一索引，查询未命中： UPDATE students SET score = 100 WHERE name = 'John';

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142732364-107ce52b-bcd2-44f3-97d5-e1d62b7e4268.png)

- 无索引： UPDATE students SET score = 100 WHERE score = 22;

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142769431-f12e9ba3-78ab-47da-893b-6826d456fb08.png)

- 聚集索引，范围查询： UPDATE students SET score = 100 WHERE id <= 20;

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142802290-544d30a6-111d-4621-8427-de8f3e458c1f.png)

- 辅助索引，范围查询： UPDATE students SET score = 100 WHERE age <= 23;

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142828782-50eab618-848c-4c87-af63-7ad42e38856d.png)

- 修改索引值： UPDATE students SET name = 'John' WHERE id = 15;

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642142867007-10dd68a2-bf64-4344-aa1c-22fe95692d7d.png)

# redo log

redo 日志用来实现事务的持久性；内存中包含 redo log buffer，磁盘中包含 redo log file；

当事务提交时，必须先将该事务的所有日志写入到重做日志文件进行持久化，待事务的commit操作完成才完成了事务的提交；

redo log 顺序写，记录的是对每个页的修改（页、页偏移量、以及修改的内容）；在数据库运行时不需要对 redo log 的文件进行读取操作；只有发生宕机的时候，才会拿redo log进行恢复；

# undo log

undo 日志用来帮助事务回滚以及 MVCC 的功能；存储在共享表空间中；undo 是逻辑日志，回滚时将数据库逻辑地恢复到原来的样子，根据 undo log 的记录，做之前的逆运算；比如事务中有insert 操作，那么执行 delete 操作；对于 update 操作执行相反的 update 操作；同时 undo 日志记录行的版本信息，用于处理 MVCC 功能；

# 并发读异常

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642051098419-28f52707-1dce-4c8a-9b17-773a739f4e8c.png)

## 脏读

问题：事务读到其他事务未提交的数据。

原因：因为读没有做任何处理。

解决：提升隔离级别。

## 不可重复读

问题：事务两次查询同一行的数据，结果不一致。

原因：MVCC读取最新版本数据, 是另一个事务新提交的修改。

解决：提升隔离级别到READ COMMITED。

## 幻读

问题：

- 事务一以主键id为条件，查询数据，结果不存在；
- 事务二插入该主键id的行数据；

- 事务一插入同样的数据，会被阻塞，因为事务一会获取x锁；
- 事务二commit，事务一会报'Duplicate entry xxx for key Primary'的错误。

原因：读没有加锁

解决：REPEATABLE READ下，读加共享锁。



## 丢失更新

### 回滚覆盖

数据库已经解决回滚覆盖

### 提交覆盖

![img](https://cdn.nlark.com/yuque/0/2022/png/756577/1642055991991-cc0dad35-acee-4141-af16-c145f803d2ad.png)

解决方案：读加锁 lock in share mode

### 并发异常sql

```sql
Session A

drop table  if exists `dirty_read_t`;

create table `dirty_read_t` (
	id int not null,
	name varchar(20),
	sex varchar(10),
	age int,
	primary key(id)
)
ENGINE=InnoDB;

INSERT INTO `transaction`.`dirty_read_t` (`id`, `name`, `sex`, `age`) 
VALUES ('1', 'changping', 'male', '21'),('2', 'bingshuang', 'male', '22'),
(7, 'yanjia', 'male', 23), (10, 'dongning', 'male', 24);


-- 查看全局隔离级别
SELECT @@global.tx_isolation;
-- 查看当前会话隔离级别
SELECT @@session.tx_isolation;
SELECT @@tx_isolation;


-- 脏读
select * from dirty_read_t;


set session transaction isolation level read uncommitted;
BEGIN;
SELECT * FROM dirty_read_t WHERE id > 3;

-- 事务B插入数据后还未COMMIT，事务A中就能够读到那条数据了，之后事务B的事务rollback了。
-- 读到脏数据。
SELECT * FROM dirty_read_t WHERE id > 3;

rollback;


-- 不可重复读
set session transaction isolation level read committed;
BEGIN;

SELECT * FROM dirty_read_t WHERE id > 3;

-- 事务B insert数据，commit后，事务A读取
SELECT * FROM dirty_read_t WHERE id > 3;

ROLLBACK;

-- 幻读
set session transaction isolation level repeatable read;
BEGIN;
SELECT * FROM `dirty_read_t` WHERE id = 5;

-- 事务B已经插入了相同的一条记录
insert into `dirty_read_t` (id, name, sex, age) values (5, 'fengbo', 'male', 25);

ROLLBACK;

-- 幻读问题解决
set session transaction isolation level repeatable read;
BEGIN;
SELECT * FROM `dirty_read_t` WHERE id = 5 lock in share mode; -- 加共享锁
-- 事务B想要插入一条id = 5的记录，但是事务A已经加了共享锁，事务B获取不到锁。
insert into `dirty_read_t` (id, name, sex, age) values (5, 'fengbo', 'male', 25);

COMMIT;
Session B

-- 查看全局隔离级别
SELECT @@global.tx_isolation;
-- 查看当前会话隔离级别
SELECT @@session.tx_isolation;
SELECT @@tx_isolation;

-- 脏读

set session transaction isolation level read uncommitted;

BEGIN;
INSERT INTO  `dirty_read_t` (id, name, sex, age)VALUES(5, 'fengbo', 'male', 25);
-- insert后，事务A去读，读到UNCOMMITED的数据

-- 之后rollback了
rollback;



-- 不可重复读
set session transaction isolation level read committed;
BEGIN;

INSERT INTO  `dirty_read_t` (id, name, sex, age)VALUES(5, 'fengbo', 'male', 25);
COMMIT;

-- 幻读
set session transaction isolation level repeatable read;
BEGIN;
insert into `dirty_read_t` (id, name, sex, age) values (5, 'fengbo', 'male', 25);
ROLLBACK;
```

# 并发死锁

Mysql两种原因产生死锁：

- 多个线程对于锁的资源依赖成环；
- 线程间锁不兼容。

死锁原因分析，需要先分析出走的什么索引，再分析加的什么锁，找出死锁原因。

解决方法：

- 分析加锁顺序，调整加锁顺序；
- 分析加的是什么锁，解决冲突。