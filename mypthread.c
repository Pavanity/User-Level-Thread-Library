// File:	mypthread.c

#include "mypthread.h"

runQueue rq;
runQueue bq;
runQueue dq;
tcb *running = NULL;
ucontext_t sched_context;
ucontext_t main_context;
ucontext_t term_context;
int initialized = 0;


static void schedule(void);
void cleanup();

/* enqueue */
void rq_enqueue(runQueue *rq, tcb * thread){
	rq_node_t *newNode = malloc(sizeof(rq_node_t));
	newNode->thread = thread;
	newNode->next = NULL;
	if (rq->front == NULL) {
		rq->front = newNode;
		rq->rear = newNode;
	}
	else {
		rq->rear->next = newNode;
		rq->rear = newNode;
	}
	rq->size++;
}

/* dequeue */
tcb* rq_dequeue(runQueue *rq) {
	rq_node_t *dequeued = rq->front;
	rq->front = rq->front->next;
	if (rq->front == NULL) {
		rq->rear = NULL;
	}
	tcb *thread = dequeued->thread;
	free(dequeued);
	rq->size--;

	return thread;
}

tcb* getTCB(mypthread_t tid) {
	if (!(running->isMain) && running->tid == tid) return running;
	rq_node_t* current = rq.front;
	while (current != NULL) {
		if (!(current->thread->isMain) && current->thread->tid == tid) return current->thread;
		current = current->next;
	}
	current = bq.front;
	while (current != NULL) {
		if (!(current->thread->isMain) && current->thread->tid == tid) return current->thread;
		current = current->next;
	}
	current = dq.front;
	while (current != NULL) {
		if (!(current->thread->isMain) && current->thread->tid == tid) return current->thread;
		current = current->next;
	}
	return NULL;
}

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr,
                      void *(*function)(void*), void * arg) {
       // create Thread Control Block
       // create and initialize the context of this thread
       // allocate space of stack for this thread to run
       // after everything is all set, push this thread int
		if (!initialized) {
			getcontext(&term_context);
			term_context.uc_stack.ss_sp = malloc(32000);
			term_context.uc_stack.ss_size = 32000;
			term_context.uc_stack.ss_flags = 0;
			makecontext(&term_context, (void (*) (void)) mypthread_exit, 0);

			getcontext(&main_context);
			main_context.uc_link = &term_context;
			tcb *mainTCB = malloc(sizeof(tcb));
			if (!mainTCB) {
				fprintf(stderr, "failed to allocate mainTCB");
				return -1;
			}
			mainTCB->qcounter = 0;
			mainTCB->isMain = 1;
			mainTCB->tstatus = READY;
			mainTCB->tcontext = &main_context;
			rq_enqueue(&rq, mainTCB);

			getcontext(&sched_context);
			sched_context.uc_stack.ss_sp = malloc(32000);
			if (!sched_context.uc_stack.ss_sp) {
				fprintf(stderr, "failed to allocate sched_context stack");
				return -1;
			}
			sched_context.uc_stack.ss_size = 32000;
			sched_context.uc_stack.ss_flags = 0;
			makecontext(&sched_context, schedule, 0);
			swapcontext(&main_context, &sched_context);
		}

		tcb *newTCB = malloc(sizeof(tcb));
		if (!newTCB) {
			fprintf(stderr, "failed to allocate TCB");
			return -1;
		}
		newTCB->qcounter = 0;
		newTCB->isMain = 0;
		newTCB->tid = *thread;
		newTCB->tstatus = READY;
		newTCB->tcontext = malloc(sizeof(ucontext_t));
		if (!newTCB->tcontext) {
			free(newTCB);
			return -1;
		}

		getcontext(newTCB->tcontext);
		newTCB->tcontext->uc_stack.ss_sp = malloc(32000);
		newTCB->tcontext->uc_stack.ss_size = 32000;
		newTCB->tcontext->uc_stack.ss_flags = 0;
		newTCB->tcontext->uc_link = &term_context;
		newTCB->tstack = newTCB->tcontext->uc_stack.ss_sp;
		makecontext(newTCB->tcontext, (void (*) (void)) *function, 1, arg);

		rq_enqueue(&rq, newTCB);

    return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context

	running->qcounter++;
	swapcontext(running->tcontext, &sched_context);

	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	running->tstatus = TERMINATED;
	running->value = value_ptr;
	// free(running->tstack);
	// free(running->tcontext);
	// free(running);

	swapcontext(running->tcontext, &sched_context);
};

/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	tcb* joined = getTCB(thread);
	if (joined == NULL) {
		fprintf(stderr, "TCB doesn't exist");
		exit(EXIT_FAILURE);
	}
	do {

	} while(joined->tstatus != TERMINATED);

	if(value_ptr != NULL) {
		*value_ptr = joined->value;
	}

	// free(joined->tstack);
	// free(joined->tcontext);
	// free(joined);

	// swapcontext(running->tcontext, &sched_context);


	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex,
                          const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	mutex->init = 1;
	mutex->owner = 0;
	mutex->a = 0;
	return 0;
};

/* acquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // if the mutex is acquired successfully, enter the critical section
        // if acquiring mutex fails, push current thread into block list and //
        // context switch to the scheduler thread

		if (mutex->init == 0) return -1;

		while (__sync_lock_test_and_set(&mutex->a, 1)) {
			running->tstatus = BLOCKED;
			swapcontext(running->tcontext, &sched_context);
		}
		mutex->owner = running->tid;
		

        return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	// Release mutex and make it available again.
	// Put threads in block list to run queue
	// so that they could compete for mutex later.


	mutex->owner = 0;
	__sync_lock_release(&mutex->a) ;

	while(bq.front != NULL) {
		tcb *tmp = rq_dequeue(&bq);
		tmp->tstatus = READY;
		rq_enqueue(&rq, tmp);
	}
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init
	mutex->init = 0;
	return 0;
};

/* scheduler */
static void schedule(void) {

	// Invoke different actual scheduling algorithms
	// according to policy (STCF or MLFQ)

	// if (sched == STCF)
	//		sched_stcf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	if (!initialized) {
		initialized = 1;
	}

// schedule policy
#ifndef MLFQ
	// Choose STCF
	while (rq.front || bq.front || running) {
		sched_stcf();
	}
#else
	// Choose MLFQ
	sched_mlfq();
#endif
	cleanup();
}

/* Preemptive SJF (STCF) scheduling algorithm */
// TODO: Implement priority queue structure instead of looping through queue
void sched_stcf() {
	if (running != NULL) {
		if (running->tstatus == SCHEDULED || running->tstatus == READY) {
			running->tstatus = READY;
			rq_enqueue(&rq, running);
		} else if (running->tstatus == TERMINATED) {
			rq_enqueue(&dq, running);
		} else if (running->tstatus == BLOCKED) {
			rq_enqueue(&bq, running);
		}
		running = NULL;
	}

	if (rq.front == NULL)
		return;


	tcb *current = rq_dequeue(&rq);
	for (int i = 0; i < rq.size; ++i) {
		if(current->qcounter > rq.front->thread->qcounter) {
			rq_enqueue(&rq, current);
			current = rq_dequeue(&rq);
		}
	}
	running = current;
	running->tstatus = SCHEDULED;

	// struct itimerval it_val;
	if (signal(SIGALRM, (void (*)(int)) mypthread_yield) == SIG_ERR) {
		perror("SIGALRM");
		exit(EXIT_FAILURE);
	}
	// it_val.it_value.tv_sec = (QUANTUM/1000);
	// it_val.it_value.tv_usec = (QUANTUM*1000) % 1000000;
	// it_val.it_interval = it_val.it_value;

	// if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
    // 	perror("itimer setting");
    // 	exit(EXIT_FAILURE);
  	// }

	ualarm(QUANTUM*1000, 0);
	swapcontext(&sched_context, running->tcontext);

}


/* TODO: Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	

}



void cleanup() {
	while (dq.front) {
		tcb * tmp = rq_dequeue(&dq);
		if (!(tmp->isMain)) {
			free(tmp->tstack);
			free(tmp->tcontext);
		}
		free(tmp);
	}
	free(term_context.uc_stack.ss_sp);
	free(sched_context.uc_stack.ss_sp);
	// free(main_context.uc_stack.ss_sp);
}
