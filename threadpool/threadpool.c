#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define LL_ADD(item, list) do {                \
    item->next = list;                         \
    item->prev = NULL;                         \
    if (list != NULL) list->prev = item;       \
    list = item;                               \
} while (0)                                    \

#define LL_REMOVE(item, list) do {                            \
    if (item->prev != NULL) item->prev->next = item->next;    \
    if (item->next != NULL) item->next->prev = item->prev;    \
    if (list == item) list = list->next;                      \
    item->prev = NULL;                                        \
    item->next = NULL;                                        \
} while (0)


typedef struct nWorker {
    pthread_t thread_id;
    int terminate;
    struct nManager *pool;

    struct nWorker *next;
    struct nWorker *prev;
} nWorker;

typedef struct nJob {
    void (*func)(void* arg);
    void *user_data;

    struct nJob *next;
    struct nJob *prev;

} nJob;

typedef struct nManager {
    nWorker *workers;
    nJob    *jobs;

    int sum_thread;
    int idle_thread;

    pthread_mutex_t jobs_mutex;
    pthread_cond_t jobs_cond;
} nManager;

typedef nManager nThreadPool;

void *nworker_callback(void *arg) {
    nWorker *worker = (nWorker*)arg;
    while (1) {
        // waiting for job
        pthread_mutex_lock(&worker->pool->jobs_mutex);
        while (worker->pool->jobs == NULL) {
            if (worker->terminate == 1) break;
            pthread_cond_wait(&worker->pool->jobs_cond, &worker->pool->jobs_mutex);
        }
        if (worker->terminate == 1) {
            pthread_mutex_unlock(&worker->pool->jobs_mutex);
            break;
        }

        // remove job from list.
        nJob *job = worker->pool->jobs; // Seems job is last in, first out. ???? first job is not executed first.
        LL_REMOVE(job, worker->pool->jobs);

        pthread_mutex_unlock(&worker->pool->jobs_mutex);

        // execute
        worker->pool->idle_thread--;
        job->func(job);
        worker->pool->idle_thread++;

        free(job);
    }
    LL_REMOVE(worker, worker->pool->workers);
    free(worker);
}

int threadpool_create(nThreadPool *pool, int num_workers) {
    if (pool == NULL) return -1;
    if (num_workers < 1) num_workers = 1;
    memset(pool, 0, sizeof(nThreadPool));

    pool->jobs_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    pool->jobs_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    for (int i = 0; i < num_workers; i++) {
        nWorker *worker = (nWorker*)malloc(sizeof(nWorker));
        if (worker == NULL) {
            perror("malloc");
            return 1;
        }
        memset(worker, 0, sizeof(nWorker));
        worker->pool = pool;

        int ret = pthread_create(&worker->thread_id, NULL, nworker_callback, worker);
        if (ret) {
            perror("pthread_create");
            nWorker *w = pool->workers;
            for (w = pool->workers; w != NULL; w = w->next) {
                w->terminate = 1;
            }
            return 1;
        }
        LL_ADD(worker, pool->workers);
    }
    return 0;
}

void threadpool_destroy(nThreadPool *pool) {
    nWorker *worker = pool->workers;
    for (worker = pool->workers; worker != NULL; worker = worker->next) {
        worker->terminate = 1;
    }

    pthread_mutex_lock(&pool->jobs_mutex);
    pthread_cond_broadcast(&pool->jobs_cond);
    pthread_mutex_unlock(&pool->jobs_mutex);
}

void threadpool_push_job(nThreadPool *pool, nJob *job) {
    pthread_mutex_lock(&pool->jobs_mutex);
    LL_ADD(job, pool->jobs);
    pthread_cond_signal(&pool->jobs_cond);
    pthread_mutex_unlock(&pool->jobs_mutex);
}

#if 1

#define KING_MAX_THREAD			80
#define KING_COUNTER_SIZE		1000

void king_counter(nJob *job) {

	int index = *(int*)job->user_data;

	printf("index : %d, selfid : %lu\n", index, pthread_self());
	
	free(job->user_data);
}


int main(int argc, char *argv[]) {
    int numWorkers = 20;
    nThreadPool *pool = (nThreadPool*)malloc(sizeof(nThreadPool));
    if (pool == NULL) {
        perror("malloc");
    }
    int ret = threadpool_create(pool, numWorkers);
    if (ret) {
        printf("threadpool_create failed");
    }
    int i = 0;
    for (i = 0;i < KING_COUNTER_SIZE;i ++) {
		nJob *job = (nJob*)malloc(sizeof(nJob));
		if (job == NULL) {
			perror("malloc");
			exit(1);
		}
		
		job->func = (void (*)(void*))king_counter;
		job->user_data = malloc(sizeof(int));
		*(int*)job->user_data = i;

		threadpool_push_job(pool, job);
		
	}
    sleep(3);
    threadpool_destroy(pool);
	getchar();
	printf("\n");
}

#endif
