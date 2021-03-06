#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#include <config.h>
#include <primitives.h>
#include <tvec.h>
#include <fastrand.h>
#include <pool.h>
#include <threadtools.h>
#include <simqueue.h>
#include <barrier.h>

SimQueueStruct *queue;
int64_t d1, d2;
Barrier bar;

inline static void *Execute(void* Arg) {
    SimQueueThreadState *th_state;
    long i = 0;
    long id = (long) Arg;
    long rnum;
    volatile int j = 0;

    fastRandomSetSeed(id + 1);
    th_state = getAlignedMemory(CACHE_LINE_SIZE, sizeof(SimQueueThreadState));
    SimQueueThreadStateInit(queue, th_state, id);

    BarrierWait(&bar);
    if (id == 0) {
        d1 = getTimeMillis();
    }

    for (i = 0; i < RUNS; i++) {
        SimQueueEnqueue(queue, th_state, id, id);
        rnum = fastRandomRange(1, MAX_WORK);
        for (j = 0; j < rnum; j++)
            ;
        SimQueueDequeue(queue, th_state, id);
        rnum = fastRandomRange(1, MAX_WORK);
        for (j = 0; j < rnum; j++)
            ;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int backoff;

    if (argc != 2) {
        fprintf(stderr, "ERROR: Please set an upper bound for the backoff!\n");
        exit(EXIT_SUCCESS);
    } else {
        sscanf(argv[1], "%d", &backoff);
    }
    queue = getAlignedMemory(CACHE_LINE_SIZE, sizeof(SimQueueStruct));
    SimQueueInit(queue, backoff);

    BarrierInit(&bar, N_THREADS);
    StartThreadsN(N_THREADS, Execute, _USE_UTHREADS_);
    JoinThreadsN(N_THREADS);
    d2 = getTimeMillis();

    printf("time: %d (ms)\tthroughput: %.2f (millions ops/sec)\t", (int) (d2 - d1), 2.0*RUNS*N_THREADS/(1000.0*(d2 - d1)));
    printStats(N_THREADS);

#ifdef DEBUG
    Node *link_a = queue->enq_pool[queue->enq_sp.struct_data.index].link_a;
    Node *link_b = queue->enq_pool[queue->enq_sp.struct_data.index].link_b;
    CASPTR(&link_a->next, null, link_b);
    fprintf(stderr, "State value: %ld\n", (long)queue->enq_pool[queue->enq_sp.struct_data.index].counter);
    volatile Node *cur = queue->deq_pool[queue->deq_sp.struct_data.index].ptr;
    long counter = 0;
    while (cur != null) {
        cur = cur->next;
        counter++;
    }
    fprintf(stderr, "%ld nodes were left in the queue\n", counter - 1); // Do not count queue->guard node 
#endif

    return 0;
}
