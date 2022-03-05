// File:	mypthread_t.h

#ifndef MYTHREAD_T_H
#define MYTHREAD_T_H

#define _GNU_SOURCE

/* To use POSIX pthread Library in Benchmark, comment this USE_MYTHREAD macro */
#define USE_MYTHREAD 1


#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>
#include <signal.h>

typedef uint mypthread_t;

#define READY 0
#define SCHEDULED 1
#define BLOCKED 2
#define TERMINATED 3

#define QUANTUM 5

typedef struct threadControlBlock {
	mypthread_t tid; // thread Id
	int tstatus; // thread status
	ucontext_t *tcontext; // thread context
	stack_t *tstack; // thread stack
	int qcounter; //timer count
	int isMain;
	void * value; //join arg value
	// thread priority
	// And more ...

	
} tcb;

/* mutex struct definition */
typedef struct mypthread_mutex_t {
	int init;
	mypthread_t owner;
	int a;
} mypthread_mutex_t;


typedef struct rq_node_t {
	tcb *thread;
	struct rq_node_t *next;
} rq_node_t;

typedef struct runQueue {
	rq_node_t *front;
	rq_node_t *rear;
	int size;
} runQueue;

/* Function Declarations: */

void sched_stcf();

/* enqueue */
void rq_enqueue(runQueue *rq, tcb * thread);

/* dequeue */
tcb* rq_dequeue(runQueue *rq);

/* get TCB from queues*/
tcb* getTCB(mypthread_t tid);

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int mypthread_yield();

/* terminate a thread */
void mypthread_exit(void *value_ptr);

/* wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr);

/* initial the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex);

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex);

/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex);

#ifdef USE_MYTHREAD
#define pthread_t mypthread_t
#define pthread_mutex_t mypthread_mutex_t
#define pthread_create mypthread_create
#define pthread_exit mypthread_exit
#define pthread_join mypthread_join
#define pthread_mutex_init mypthread_mutex_init
#define pthread_mutex_lock mypthread_mutex_lock
#define pthread_mutex_unlock mypthread_mutex_unlock
#define pthread_mutex_destroy mypthread_mutex_destroy
#endif

#endif
