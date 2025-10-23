# 目标
1. git相较于svn的主要优势是什么？分布式，集中式
2. git中的四个区域；远端仓库，本地仓库
3. 文件的四种状态
4. git正向操作的常用命令
5. 逆向操作的常用命令
6. 本次commit修改的内容写错，或者commit message如果写错，要如何处理？
7. 一个feature的开发中，正常提交一个merge request；其中如果有多个commit，但是commit比较杂乱，如何整理？
8. 冲突如何解决？

## git的历史
先水一水git产生的背景：

<font style="color:rgb(0, 0, 0);">Linus在1991年创建了开源的Linux，从此，Linux系统不断发展，已经成为最大的服务器系统软件了。</font>

<font style="color:rgb(0, 0, 0);">Linus虽然创建了Linux，但Linux的壮大是靠全世界热心的志愿者参与的，这么多人在世界各地为Linux编写代码，那Linux的代码是如何管理的呢？</font>

<font style="color:rgb(0, 0, 0);">事实是，在2002年以前，世界各地的志愿者把源代码文件通过diff的方式发给Linus，然后由Linus本人通过手工方式合并代码！</font>

<font style="color:rgb(0, 0, 0);">你也许会想，为什么Linus不把Linux代码放到版本控制系统里呢？不是有CVS、SVN这些免费的版本控制系统吗？因为Linus坚定地反对CVS和SVN，这些集中式的版本控制系统不但速度慢，而且必须联网才能使用。有一些商用的版本控制系统，虽然比CVS、SVN好用，但那是付费的，和Linux的开源精神不符。</font>

<font style="color:rgb(0, 0, 0);">不过，到了2002年，Linux系统已经发展了十年了，代码库之大让Linus很难继续通过手工方式管理了，社区的弟兄们也对这种方式表达了强烈不满，于是Linus选择了一个商业的版本控制系统BitKeeper，BitKeeper的东家BitMover公司出于人道主义精神，授权Linux社区免费使用这个版本控制系统。</font>

<font style="color:rgb(0, 0, 0);">安定团结的大好局面在2005年就被打破了，原因是Linux社区牛人聚集，开发Samba的Andrew试图破解BitKeeper的协议，被BitMover公司发现了，于是BitMover公司怒了，要收回Linux社区的免费使用权。</font>

<font style="color:rgb(0, 0, 0);">之后，Linus花了两周时间自己用C写了一个分布式版本控制系统，这就是Git！一个月之内，Linux系统的源码已经由Git管理了！牛是怎么定义的呢？大家可以体会一下。</font>

**<font style="color:rgb(0, 0, 255);">Git迅速成为最流行的分布式版本控制系统，尤其是2008年，GitHub网站上线了，它为开源项目免费提供Git存储，无数开源项目开始使用GitHub托管，包括jQuery，PHP，Ruby，Nginx，Redis，Mysql等等。</font>**

# git原理
## git vs subversion
项目开发中版本管理是必须的；git和svn都是版本管理的工具，git相较于svn有哪些优势？

**svn**

![](https://cdn.nlark.com/yuque/0/2024/png/756577/1725242801087-45fc99ee-e8ff-4e1f-bf48-2c793841552d.png)

**git**

![](https://cdn.nlark.com/yuque/0/2024/png/756577/1725243732334-934e9fde-76a4-4099-b044-cbecf2e58037.png)



Git 相较于 SVN（Subversion）有多个优点，主要体现在以下几个方面：



+ <font style="color:#DF2A3F;">分布式</font>版本控制：

Git：每个开发者的本地仓库都是完整的版本库，包括项目的全部历史记录。这使得离线工作变得可行，开发者可以在没有网络连接的情况下进行提交和查看历史。

SVN：是<font style="color:#DF2A3F;">集中式</font>版本控制系统，所有版本历史都存储在中央服务器上，开发者需要连接到服务器才能进行大多数操作。

<font style="color:#DF2A3F;"></font>

+ <font style="color:#DF2A3F;">速度</font>：

Git：由于大多数操作（如提交、查看历史、分支等）在本地进行，因此速度较快。

SVN：许多操作需要与中央服务器交互，可能导致延迟。



+ 分支和合并：

Git：创建和管理分支非常轻松且高效，分支操作几乎是瞬时的。合并操作也相对简单，支持多种合并策略。

SVN：虽然也支持分支和合并，但操作较为复杂，且分支的创建和管理不如 Git 直观。



+ 数据完整性：

Git：使用 SHA-1 哈希算法来确保数据的完整性，任何数据的改变都会导致哈希值的变化，便于检测数据损坏。

SVN：虽然也有数据完整性检查，但相对而言，Git 的机制更为强大。



+ 历史记录：

Git：每次提交都包含完整的历史信息，可以轻松地查看项目的演变过程。

SVN：提交历史可以查看，但不如 Git 直观和灵活。



+ 灵活的工作流：

Git：支持多种工作流（如 Git Flow、GitHub Flow 等），可以根据团队需求灵活选择。

SVN：工作流相对固定，灵活性较低。



+ <font style="color:#DF2A3F;">社区支持与生态系统</font>：

Git：拥有广泛的社区支持和丰富的工具生态系统，如 GitHub、GitLab、Bitbucket 等。

SVN：虽然也有一些工具和平台，但相对较少。



+ <font style="color:#DF2A3F;">冲突解决</font>：

Git：提供强大的工具来处理合并冲突，允许开发者更灵活地解决问题。

SVN：冲突解决相对简单，但功能较少。



总的来说，Git 提供了更高的灵活性、速度和便利性，适合现代软件开发的需求，而 SVN 则更适合某些特定场景，特别是在大型企业中需要集中管理的项目。

## 版本管理的标准流程
![](https://cdn.nlark.com/yuque/0/2024/png/756577/1725246238128-dc5f9b59-f27e-422c-a4cb-e7c4f49bfb8b.png)

<font style="color:#c00000;">Master</font><font style="color:#595959;">： 稳定压倒一切，禁止尚review和测试过的代码提交到这个分支上，Master上的代码是可以随时部署到线上生产环境的。</font>

<font style="color:#c00000;">Develop</font><font style="color:#595959;">：开发分支，我们的持续集成工作在这里，</font><font style="color:#595959;">code review</font><font style="color:#595959;">过的代码合入到这里，我们以下要讲的</font><font style="color:#595959;">BUG fix</font><font style="color:#595959;">和</font><font style="color:#595959;">feature</font><font style="color:#595959;">开发都可以基于</font><font style="color:#595959;">develop</font><font style="color:#595959;">分支拉取，修改完之后合入到</font><font style="color:#595959;">develop</font><font style="color:#595959;">分支。</font>

<font style="color:#c00000;">Feature</font><font style="color:#595959;">：功能开发和</font><font style="color:#595959;">change request</font><font style="color:#595959;">的分支，也即我们每一个</font><font style="color:#595959;">feature</font><font style="color:#595959;">都可以从</font><font style="color:#595959;">devlop</font><font style="color:#595959;">上拉取一个分支，开发、</font><font style="color:#595959;">review</font><font style="color:#595959;">和测试完之后合入</font><font style="color:#595959;">develop</font><font style="color:#595959;">分支。</font>

<font style="color:#c00000;">Hotfix</font><font style="color:#595959;">：紧急修改的分支，在master发布到线上出现某个问题的时候，算作一个紧急布丁。从master分支上拉取代码，修改完之后合入develop和master分支。</font>

<font style="color:#595959;">Release </font><font style="color:#595959;">：预发布分支，比如</font><font style="color:#595959;">0.1</font><font style="color:#595959;">、</font><font style="color:#595959;">0.2</font><font style="color:#595959;">、</font><font style="color:#595959;">1.12</font><font style="color:#595959;">版本，我们一般说的系统测试就是基于这些分支做的，如果出现</font><font style="color:#595959;">bug</font><font style="color:#595959;">，则可以基于该</font><font style="color:#595959;">release</font><font style="color:#595959;">分支拉取一个临时</font><font style="color:#595959;">bug</font><font style="color:#595959;">分支。</font>

<font style="color:#595959;">Bug ： bug fix的分支，当我们定位、解决后合入develop和Release分支，然后让测试人员回归测试，回归测试后由close这个bug</font>

## git的四个区域
![](https://cdn.nlark.com/yuque/0/2024/png/756577/1725241861798-25fd01e6-10c4-472c-9c58-f33b026bc74c.png)

<font style="color:#ff6600;">Workspace</font><font style="color:#4b4b4b;">： 工作区，就是你平时存放项目代码的地方</font>

<font style="color:#ff6600;">Index / Stage</font><font style="color:#4b4b4b;">： 暂存区，用于临时存放你的改动，事实上它只是一个文件，保存即将提交到文件列表信息</font>

<font style="color:#ff6600;">Repository</font><font style="color:#4b4b4b;">： 仓库区（或版本库），就是安全存放数据的位置，这里面有你提交到所有版本的数据。其中</font><font style="color:#4b4b4b;">HEAD</font><font style="color:#4b4b4b;">指向最新放入仓库的版本</font>

<font style="color:#ff6600;">Remote</font><font style="color:#4b4b4b;">： 远程仓库，托管代码的服务器，可以简单的认为是你项目组中的一台电脑用于远程数据交换</font>

## <font style="color:#4b4b4b;">工作流程</font>
![](https://cdn.nlark.com/yuque/0/2024/png/756577/1725248622821-09e9e512-aec2-4c53-9512-3cab8b11dbfc.png)

<font style="color:#4b4b4b;">git的工作流程一般是这样的：</font>

<font style="color:#4b4b4b;">1</font><font style="color:#4b4b4b;">、在工作目录中添加、修改文件；</font>

<font style="color:#4b4b4b;">2</font><font style="color:#4b4b4b;">、将需要进行版本管理的文件</font><font style="color:#4b4b4b;">add</font><font style="color:#4b4b4b;">到暂存区域；</font>

<font style="color:#4b4b4b;">3</font><font style="color:#4b4b4b;">、将暂存区域的文件</font><font style="color:#4b4b4b;">commit</font><font style="color:#4b4b4b;">到</font><font style="color:#4b4b4b;">git</font><font style="color:#4b4b4b;">仓库；</font>

<font style="color:#4b4b4b;">4</font><font style="color:#4b4b4b;">、本地的修改</font><font style="color:#4b4b4b;">push</font><font style="color:#4b4b4b;">到远程仓库，如果失败则执行第</font><font style="color:#4b4b4b;">5</font><font style="color:#4b4b4b;">步</font>

<font style="color:#4b4b4b;">5、git pull将远程仓库的修改拉取到本地，如果有冲突需要解决冲突。回到第三步</font>

<font style="color:#4b4b4b;">因此，git管理的文件有四种状态：</font><font style="color:#ff6600;">未跟踪(Untracked)</font><font style="color:#4b4b4b;">，</font><font style="color:#ff6600;">已修改（modified）</font><font style="color:#4b4b4b;">,</font><font style="color:#ff6600;">已暂存（staged）</font><font style="color:#4b4b4b;">,</font><font style="color:#ff6600;">已提交(committed)</font>

## 文件的四种状态
![](https://cdn.nlark.com/yuque/0/2024/png/756577/1725249032887-baa19238-9de9-45bf-9201-449c5d7cc99d.png)

<font style="color:#ff6600;">Untracked:</font><font style="color:#4b4b4b;">   未跟踪, 此文件在文件夹中，但并没有加入到git库，不参与版本控制，通过git add 状态变为Staged。</font>

<font style="color:#ff6600;">Unmodified:</font><font style="color:#4b4b4b;">   文件已经入库且未修改, 即版本库中的文件快照内容与文件夹中完全一致，这种类型的文件有两种去处，如果它被修改，而变为Modified，如果使用git rm移出版本库, 则成为Untracked文件。</font>

<font style="color:#ff6600;">Modified</font><font style="color:#ff6600;">：</font><font style="color:#4b4b4b;">文件已修改，仅仅是修改，并没有进行其他的操作，这个文件也有两个去处，通过</font><font style="color:#4b4b4b;">git</font><font style="color:#4b4b4b;"> add</font><font style="color:#4b4b4b;">可进入暂存</font><font style="color:#4b4b4b;">staged</font><font style="color:#4b4b4b;">状态，使用</font><font style="color:#4b4b4b;">git</font><font style="color:#4b4b4b;"> checkout </font><font style="color:#4b4b4b;">则丢弃修改，返回到</font><font style="color:#4b4b4b;">unmodify</font><font style="color:#4b4b4b;">状态</font><font style="color:#4b4b4b;">, </font><font style="color:#4b4b4b;">这个</font><font style="color:#4b4b4b;">git</font><font style="color:#4b4b4b;"> checkout</font><font style="color:#4b4b4b;">即从库中取出文件，覆盖当前修改</font>

<font style="color:#ff6600;">Staged：</font><font style="color:#4b4b4b;">暂存状态，执行git commit则将修改同步到库中，这时库中的文件和本地文件又变为一致，文件为Unmodify状态。</font>

<font style="color:#4b4b4b;"></font>

<font style="color:#4b4b4b;">查询状态的命令：</font>

`<font style="color:#4b4b4b;">git status</font>`

# 正向操作
主要命令：

workspace-->staged

`git add`

staged-->repository

`git commit`

repository-->remote

`git push`

# 逆向操作
很多时候，我们add/commit/push操作后，发现操作错误了，怎么办？



逆向操作不是必须，操作错误也有其他方法来解决，但使用逆向操作更快，对git的理解更加清晰；

## staged-->workspace
### case 1
workspace中做了修改，后面我们发现修改有问题，并且修改量比较大；怎么办？

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761113957735-36b39715-b76a-4f28-8fff-6faff0c06442.png)



可以使用staged中的文件来恢复workspace中的文件；

`git restore`

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761114006199-8e1ae073-36c0-482b-b916-f6c8cd5d755e.png)

### case 2
workspace中修改代码，已经使用`git add`添加到了staged；

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761114068333-5d7de5bf-e721-4ae0-b376-fd609b8c447f.png)

使用`git restore --staged`，把staged中的内容回退到workspace中；

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761119868904-baa0efd7-fc7f-4dcd-894d-545d3e7792c1.png)

## repository-->workspace
workspace修改了代码，修改后发现不需要；使用repository来恢复workspace

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761114171064-a9d04a90-a7c8-4a59-b057-27b23400ba26.png)

使用`git checkout`

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761114210998-5e12fd0e-e2d1-4082-ac82-7eb78f3e100f.png)

## Repository-->workspace(Head~N)
### reset --mixed
HEAD指针指向最近一个commit；HEAD~2，HEAD往前2个commit；

如果最近两个commit有问题；repository和staged想恢复到HEAD~2，但是workspace仍然想要使用HEAD的内容；则使用`git reset --mixed HEAD~2`；--mixed不加也可以，默认就是--mixed；

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761114296040-2180d70b-79cd-4a0a-8193-8978d13c8882.png)

### reset --soft
`git reset --soft HEAD~2`

将repository回退到HEAD~2；staged仍然是HEAD

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761114380499-a3a1f085-4b34-424b-988c-afbe85f92789.png)

### reset --hard
`git reset --hard HEAD~2`

repository，staged，workspace都使用HEAD~2

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761114464348-01a79f60-4f4d-4d0f-a33d-b249fdf4bc56.png)



# 优化commit
每个commit都要build/test通过；对于不规范的commit，我们如何进行优化？

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761114594319-02ac4317-9fc1-4cb9-955a-8c7e8090b7be.png)

## git commit --amend
`git commit --amend` 用于修改或更新上一次提交的命令。它允许你在不创建新的提交的情况下，直接修改上一次提交的commit message或内容。这对于修复commit message中的错误、添加遗漏的文件或修改已经提交的内容非常有用。

### **修改最后一次提交的提交信息**
![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761119566769-b5c772b5-bf82-401c-a13d-ffa7322a89d6.png)

###  添加或删除文件到上一次提交中  
+ 假设你在上次提交后，发现有文件遗漏或需要调整文件内容，你可以进行修改并将这些文件重新添加到staged，然后使用 `--amend` 将这些更改包括在上一次提交中。
+ 例如，你刚刚修改了 `main.c` 并想将这些更改添加到上一次提交中：

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761119604187-88f3cb57-98de-4df8-8103-140d25e6755c.png)

## git rebase -i
`git rebase -i`可以对一个MR中的多个commit进行修改；使得commit之间 有清晰的逻辑关系；

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761119641652-84531c07-75ac-4656-b18f-800a3b63d7fc.png)

![](https://cdn.nlark.com/yuque/0/2024/png/756577/1725272910014-950581dc-2d21-4c27-85b5-41f9c23adcbc.png)

# 解决冲突
冲突场景有多种，比较常见的场景：

+ 基于develop branch开发，有多个feature branch；待某个feature要合入develop时，发现冲突；
+ git rebase优化多个commit时，多个commit可能会有冲突；



手动解决冲突

![](https://cdn.nlark.com/yuque/0/2025/png/756577/1761119728998-642748c1-6bd5-4a77-b5d3-1ee54a2043db.png)

# 结束
开发过程中，掌握本文中的git命令，就可以了；命令一定要多练习使用；建议通过命令行使用git，而不是可视化工具的方式；

其他命令，如`git merge`，`git fetch`等等，暂时忘记它们吧；如果一定要使用，参考git官网的手册；

## 参考文献
[git官网](https://git-scm.com/)

