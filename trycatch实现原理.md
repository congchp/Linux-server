# 线程私有空间

线程内部的全局变量

比如一个线程一个reactor，reactor可以作为线程内部的全局变量。

下面代码中，在主线程中定义了一个pthread_key_t key, 传入到子线程，对于这个key，在用pthread_key_create创建后，对于同样的key，每个线程都会有自己的私有空间。每个线程可以设置不同的值。

```c
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define THREAD_COUNT     3

typedef void *(*thread_cb)(void *arg);

void print_thread1_key(pthread_key_t *key) {

    int *p = (int *)pthread_getspecific(*key);

    printf("thread 1: %d\n", *p);

}

void *thread1_proc(void *arg) {

    pthread_key_t *key = (pthread_key_t *)arg;

    int i = 5;
    pthread_setspecific(*key, &i);

    print_thread1_key(key);
}

void print_thread2_key(pthread_key_t *key) {

    char *p = (char *)pthread_getspecific(*key);

    printf("thread 2: %s\n", p);

}

void *thread2_proc(void *arg) {

    pthread_key_t *key = (pthread_key_t *)arg;

    char *ptr = "thread2_proc";
    pthread_setspecific(*key, ptr);

    print_thread2_key(key);
}

struct pair {
    int x;
    int y;
};

void print_thread3_key(pthread_key_t *key) {

    struct pair *p = (struct pair *)pthread_getspecific(*key);

    printf("thread 3 x[%d], y[%d]\n", p->x, p->y);

}

void *thread3_proc(void *arg) {

    pthread_key_t *key = (pthread_key_t *)arg;

    struct pair p = {1, 2};
    pthread_setspecific(*key, &p);

    print_thread3_key(key);
}

int main() {

    pthread_t tid[THREAD_COUNT] = {0};

    thread_cb callback[THREAD_COUNT] = {
        thread1_proc,
        thread2_proc,
        thread3_proc
    };

    pthread_key_t key;
    pthread_key_create(&key, NULL);

    int i = 0;
    for (i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&tid[i], NULL, callback[i], &key);
    }

    for (i = 0; i < THREAD_COUNT; i++) {
        pthread_join(tid[i], NULL);
    }

}
```

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639720739758-9bbeef5c-5a6a-4c14-9b34-c627fb70de14.png)



**线程的私有空间用在哪里呢？**

# try-catch实现原理



越业务的代码，越需要使用捕获异常。

C语言为什么没有try-catch？因为通过返回值可以判断。

接口高度集成的时候，会有异常的存在，很难通过返回值来判断。比如QT里面CreateSocketAndListen。

对于网络io，如果要使用try-catch的话，是下面的格式：

```c
try {
    connect(server); // timeout
    send(); // eagin
    recv(); // disconnect
} catch (timeout) {

} catch (eagin) {

} catch (disconnect) {

} finally {
    close(fd);
}
```

所以我们需要自己实现C语言的try-catch

## try-catch实现的关键点

### setjmp/longjmp

setjmp是设置了一个标签位，后面通过longjmp跳到这个标签位。

longjmp是C语言里面唯一可以破坏栈的，它的执行不需要出栈。没有返回。可以从一个函数跳到另一个函数。

setjmp/longjmp是可重入的，可重入就是函数执行一次，还没有返回结果，再次调用执行，会不会影响结果。setjmp/longjmp本身是线程安全，但是调用的Exception并不是线程安全的，就是count。

setjmp/longjmp是实现try-catch的基础。

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639622704336-743d10dd-d12f-4da5-8a31-f49e04e8f0dc.png)

```c
#include <setjmp.h>
#include <stdio.h>

jmp_buf env;
int count = 0;

// 执行longjmp后，sub_func没有出栈，再次调用sub_func,会覆盖之前的栈
void sub_func(int idx) {
    printf("sub_func: %d\n", idx);
    longjmp(env, idx); // longjmp第二个参数就是跳回setjmp后的返回值
}


int main() {

    int idx = 0;
    count = setjmp(env); // setjmp第一次调用返回0，从longjmp跳回来后的返回值是longjmp指定的
    if (0 == count) {
        sub_func(++idx);
    } else if (1 == count) {
        sub_func(++idx);
    } else if (2 == count) {
        sub_func(++idx);
    } else if (3 == count) {
        sub_func(++idx);
    }

    printf("other item\n");

    return 0;
}
```

![img](https://cdn.nlark.com/yuque/0/2021/png/756577/1639722949686-0dbdf20f-4749-4685-b15e-253153422354.png)

上面的代码，形式上已经很像try-catch了，可以用宏定义，定义成try catch

```c
#include <setjmp.h>
#include <stdio.h>

jmp_buf env;
int count = 0;

#define try    count = setjmp(env); if (0 == count)
#define catch(type) else if (type == count)
#define throw(type) longjmp(env, type)


void sub_func(int idx) {
    printf("sub_func: %d\n", idx);
    throw(idx);
}


int main() {

    try {
        sub_func(++count);
    } catch (1) {
        sub_func(++count);
    } catch (2) {
        sub_func(++count);
    } catch (3) {
        sub_func(++count);
    }

    printf("other item\n");

    return 0;
}
```

### 需要解决的问题

1. 怎么处理嵌套？

​    try中又有try。

​    try-catch是一个典型栈的概念，怎么去构建这个栈呢？节点定义如下。

```c
struct ExceptionFrame {

    jmp_buf env;
    int count;

    struct ExceptionFrame *next;

};
```

​    每一层对应自己的栈节点，都有自己的env，try找到自己的栈节点，也就找到了对应的env，就能知道是由那一层进行catch。



try-catch在运行的时候，栈节点是一个单例, 同一层try-catch用一个节点就可以了。

```c
try {
    
} catch () {
    
}

try {

} catch () {

}
```





**这个栈的当前节点保存在哪里？**

**保存到线程的私有空间，这样可以保证线程安全。**

`**线程a的异常，线程b来捕获**`**。这种可以做，但是没必要这么做。**

**我们做成每个线程一个栈，per stack per thread.**



三个catch有可能捕获同样的异常，需要通过当前栈节点确定由那个catch捕获。

```c
func() {
    struct ExceptionFrame *frame = pthread_getspecific(key);
    longjmp(frame->env, count);
}


// jmp_buf env
try {
    // jmp_buf env
    try {
        // jmp_buf env
        try {
            func();
        } catch (1) {

        }
    } catch (1) {

    }

} catch (1) {

}
```

1. 怎么保证线程安全？

​    避免跨线程跳，比如在线程a中setjmp，在线程b中longjmp。

​    将栈的当前节点保存到**线程的私有空间**。

### 代码实现

```c
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <setjmp.h>

#include <pthread.h>

#define ThreadLocalData pthread_key_t
#define ThreadLocalDataSet(key, value) pthread_setspecific((key), (value))
#define ThreadLocalDataGet(key) pthread_getspecific((key))
#define ThreadLocalDataCreate(key) pthread_key_create(&(key), NULL)

#define EXCEPTION_MESSAGE_LENGTH   512

ThreadLocalData exception_stack;

typedef struct _Exception {

    const char *name;

} Exception;

typedef struct _ExceptionFrame {

    jmp_buf env;

    Exception *exception;

    const char *file;
    const char *func;
    int line;

    struct _ExceptionFrame *next;
    
    char msg[EXCEPTION_MESSAGE_LENGTH];

} ExceptionFrame;

enum {
    ExceptionEntered = 0,
    ExceptionThrown,
    ExceptionHandled,
    ExceptionFinalized
};

#define ExceptionPopStack  \
    ThreadLocalDataSet(exception_stack, ((ExceptionFrame *)ThreadLocalDataGet(exception_stack))->next)

#define Try do { \
            ExceptionFrame frame; \
            frame.next = (ExceptionFrame *)ThreadLocalDataGet(exception_stack); \
            ThreadLocalDataSet(exception_stack, &frame); \
            int exception_flag = setjmp(frame.env); \
            if (exception_flag == ExceptionEntered) {

#define ReThrow					ExceptionThrow(frame.exception, frame.func, frame.file, frame.line, NULL)
#define Throw(e, cause, ...) 	ExceptionThrow(&(e), __func__, __FILE__, __LINE__, cause, ##__VA_ARGS__, NULL)

#define Catch(e) \
                if (exception_flag == ExceptionEntered) ExceptionPopStack; \
            } else if (frame.exception == &(e)) { \
                exception_flag = ExceptionHandled;

#define EndTry \
                if (exception_flag == ExceptionEntered) ExceptionPopStack; \
            } \
            if (exception_flag == ExceptionThrown) { \
                ExceptionThrow(frame.exception, frame.func, frame.file, frame.line, NULL); \
            } \
        } while (0)


void ExceptionThrow(Exception *e, const char *func, const char *file, int line, const char *cause, ...) {

    ExceptionFrame *frame = (ExceptionFrame *)ThreadLocalDataGet(exception_stack);

    if (frame) {
        frame->exception = e;
        frame->func = func;
        frame->file = file;
        frame->line = line;

        if (cause) {
            va_list ap;
            va_start(ap, cause);
            vsnprintf(frame->msg, EXCEPTION_MESSAGE_LENGTH, cause, ap); 
            va_end(ap);
        }

        ExceptionPopStack;
        longjmp(frame->env, ExceptionThrown);

    } else if (cause) {

        char message[EXCEPTION_MESSAGE_LENGTH];

        va_list ap;
        va_start(ap, cause);
        vsnprintf(message, EXCEPTION_MESSAGE_LENGTH, cause, ap);
        va_end(ap);

        printf("%s: %s\n raised in %s at %s:%d\n", e->name, message, func ? func : "?", file ? file : "?", line);
        
    } else {

        printf("%s: %p\n raised in %s at %s:%d\n", e->name, e, func ? func : "?", file ? file : "?", line);
        
    }


}


#if 1

Exception A = {"AException"};
Exception B = {"BException"};
Exception C = {"CException"};
Exception D = {"DException"};

void *thread(void *args) {

    long selfid = (long)pthread_self();

    Try {

        Throw(A, "A");
        
    } Catch (A) {

        printf("catch A : %ld\n", selfid);
        
    } EndTry;

    Try {

        Throw(B, "B");
        
    } Catch (B) {

        printf("catch B : %ld\n", selfid);
        
    } EndTry;

    Try {

        Throw(C, "C");
        
    } Catch (C) {

        printf("catch C : %ld\n", selfid);
        
    } EndTry;

    Try {

        Throw(D, "D");
        
    } Catch (D) {

        printf("catch D : %ld\n", selfid);
        
    } EndTry;

    Try {

        Throw(A, "A Again");
        Throw(B, "B Again");
        Throw(C, "C Again");
        Throw(D, "D Again");

    } Catch (A) {

        printf("catch A again : %ld\n", selfid);
    
    } Catch (B) {

        printf("catch B again : %ld\n", selfid);

    } Catch (C) {

        printf("catch C again : %ld\n", selfid);
        
    } Catch (D) {
    
        printf("catch B again : %ld\n", selfid);
        
    } EndTry;

    return NULL;
    
}


#define THREADS		50

int main(void) {

    ThreadLocalDataCreate(exception_stack);

    Throw(D, NULL);

    Throw(C, "null C");

    printf("\n\n=> Test1: Try-Catch\n");

    Try {

        Try {
            Throw(B, "recall B");
        } Catch (B) {
            printf("recall B \n");
        } EndTry;
        
        printf("Recall A\n");
        Throw(A, NULL);

    } Catch(A) {

        printf("\tResult: Ok\n");
        
    } EndTry;

    printf("=> Test1: Ok\n\n");

    printf("=> Test2: Test Thread-safeness\n");
#if 1
    int i = 0;
    pthread_t threads[THREADS];
    
    for (i = 0;i < THREADS;i ++) {
        pthread_create(&threads[i], NULL, thread, NULL);
    }

    for (i = 0;i < THREADS;i ++) {
        pthread_join(threads[i], NULL);
    }
#endif
    printf("=> Test2: Ok\n\n");

} 


#endif
```

