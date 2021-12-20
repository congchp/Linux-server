# CPU亲缘性，affinity

使用进程或者线程都是可以的。

CPU粘合的作用，更大程度的利用CPU的性能。如果开4个进程去处理网络io，进行CPU粘合后，进程只在绑定的CPU上调度，每个CPU都有一个调度队列，进行只会出现在绑定的CPU的调度队列中，不会切换到其他CPU中调度，一定意义上节省了CPU进程切换的代价。

```c
// 进程粘合

#include <stdio.h>

#define __USE_GNU

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>

void process_affinity(int num) {

    // pid_t selfid = syscall(__NR_gettid);
    pid_t selfid = getpid();
    printf("pid = %d\n", selfid);

    cpu_set_t mask;
    CPU_ZERO(&mask);

    CPU_SET(selfid % num, &mask);

    //第一个参数传selfid 和 0，效果是一样的
    sched_setaffinity(selfid, sizeof(mask), &mask);
    // sched_setaffinity(0, sizeof(mask), &mask);

    while (1);

}



int main() {

    int num = sysconf(_SC_NPROCESSORS_CONF);

    int i = 0;
    pid_t pid = 0;
    for (i = 0; i < num/2; i++) {
        pid = fork();
        if (pid <= (pid_t)0) {
            break;
        }
    }

    if (pid == 0) {
        process_affinity(num);
    }

    while (1) usleep(1);
}
// 线程粘合

#include <stdio.h>

#define __USE_GNU

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_THREAD_COUNT    10


int num = 0;

void *process_affinity(void *arg) {
    int *tid = (int *)arg;

    printf("thread [%d]\n", *tid);

    cpu_set_t mask;
    CPU_ZERO(&mask);

    CPU_SET(*tid % num, &mask);

    //第一个参数传selfid 和 0，效果是一样的
    sched_setaffinity(0, sizeof(mask), &mask);
    // sched_setaffinity(0, sizeof(mask), &mask);

    while (1);

}

int main() {

    num = sysconf(_SC_NPROCESSORS_CONF);

    int i = 0;

    pthread_t thread_id[MAX_THREAD_COUNT] = {0};
    int tid[MAX_THREAD_COUNT] = {0};

    for (i = 0; i < num; i++) {
        tid[i] = i;
        pthread_create(&thread_id[i], NULL, process_affinity, &tid[i]);
    }

    while (1) usleep(1);
}
```