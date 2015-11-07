#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"

#define STACK_SIZE 32

/* This is the thread control block */
struct thread {
	Tid threadID;
        ucontext_t *threadContext;
        bool raiseExitFlag;
        struct thread *next;
};

int numThread = 0;
struct thread *runningThread;
struct thread *readyQueue;
bool traceThreadID[THREAD_MAX_THREADS];


struct thread *
remove_want_tid_thread(Tid tid, bool isAnyThread)
{
    if (readyQueue) {

        struct thread *curr = readyQueue;
        struct thread *prev = NULL;

        if(curr->threadID == tid || isAnyThread) {
            readyQueue = curr->next;
            curr->next = NULL;
            return curr;
        }

        else {
            
            while(curr) {
                prev = curr;
                curr = curr->next;

                if(curr->threadID == tid)
                {
                    prev->next=curr->next;
                    curr->next = NULL;
                    //numThread--;
                    return curr;
                }
            }
            return NULL;
        }
    }
    return NULL;
}

void
add_to_ready_queue(struct thread *Thread)
{
    if(!readyQueue)
    {
        readyQueue = Thread;
        Thread->next = NULL;
        return;
    }

    struct thread *curr = readyQueue;
    while (curr->next)
        curr = curr->next;
     
    curr->next = Thread;
    Thread->next = NULL;
    return;
}

struct thread *
searchThread(Tid tid)
{
    struct thread *curr = readyQueue;

    while(curr) {
        if(curr->threadID == tid)
            return curr;

        curr = curr->next;
    }

    return NULL;
}

/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. 
 */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
    Tid ret;

    thread_main(arg); // call thread_main() function with arg
    ret = thread_exit(THREAD_SELF);
    // we should only get here if we are the last thread. 
    assert(ret == THREAD_NONE);
    // all threads are done, so process should exit
    exit(0);
}

void
thread_init(void)
{
	/* your optional code here */
    // allocates space for the new thread
    runningThread = (struct thread *)malloc(sizeof(struct thread));
    assert(runningThread);

    // set 'running' state for the initial thread and tid to 0
    runningThread->threadID = 0;
    runningThread->raiseExitFlag = false;
    runningThread->next = NULL;

    // allocates space for thread context
    runningThread->threadContext = (ucontext_t *)malloc(sizeof(ucontext_t));
    assert(runningThread->threadContext);
    
    /* initializes the structure pointed at runningQueue head to the 
     * currently active context ("custom" init thread that creates the 
     * first user thread)
     */
    getcontext(runningThread->threadContext);
    return;
}

Tid
thread_id()
{
	return runningThread->threadID;
	//return THREAD_INVALID;
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    if(numThread < THREAD_MAX_THREADS - 1) {
        
        // allocates space for the new thread
        struct thread *newThread = (struct thread *)malloc(sizeof(struct thread));
        assert(newThread);

        // allocates space for thread context
        newThread->threadContext = (ucontext_t*)malloc(sizeof(ucontext_t));
        assert(newThread->threadContext);
    
        ucontext_t *newThreadContext = newThread->threadContext;
        getcontext(newThreadContext);

        newThreadContext->uc_link = 0;
        newThreadContext->uc_stack.ss_sp = malloc(THREAD_MIN_STACK + STACK_SIZE);
        if(!(newThreadContext->uc_stack.ss_sp))
            return THREAD_NOMEMORY;

        newThreadContext->uc_stack.ss_size = STACK_SIZE;
        newThreadContext->uc_stack.ss_flags = 0;

        newThreadContext->uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;
        newThreadContext->uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;
        newThreadContext->uc_mcontext.gregs[REG_RIP] = (unsigned long)&thread_stub;
        newThreadContext->uc_mcontext.gregs[REG_RSP] = (unsigned long)((newThreadContext->uc_stack.ss_sp) + THREAD_MIN_STACK + STACK_SIZE);
    
        int index = 0;
        while(traceThreadID[index])
            index++;

        traceThreadID[index] = true;
        newThread->threadID = ++numThread;
        newThread->raiseExitFlag = false;
        add_to_ready_queue(newThread);
    
        return newThread->threadID;
    }
    return THREAD_NOMORE;
}

Tid
thread_switch(Tid tid, bool isAny)
{
    // Checks if in the context of currntly running thread.
    // resolves the issue when setcontext() returns form and execution started
    // right after the getcontext() because of saved PC value at that point.
    bool inCurrentThreadContext = true;
    
    // choose next thread to run, remove it from ready queue
    struct thread *next = remove_want_tid_thread(tid, isAny);

    // enqueue current in ready queue
    add_to_ready_queue(runningThread);
    
    // save currently running thread's CPU state
    getcontext(runningThread->threadContext);
    
    if(inCurrentThreadContext) {
        inCurrentThreadContext = false;
        runningThread = next;
        
        if(next->raiseExitFlag)
            thread_exit(THREAD_SELF);

        // restore next thread's CPU state
        setcontext(next->threadContext);
    }
    return tid;
}

void destroyThread(void)
{
    traceThreadID[runningThread->threadID - 1] = false;
    free(runningThread->threadContext->uc_stack.ss_sp);
    free(runningThread->threadContext);
    free(runningThread);
    numThread--;
}

Tid
thread_yield(Tid want_tid)
{
    if(want_tid == THREAD_SELF || want_tid == 0)
        return runningThread->threadID;
        
    else if(want_tid == THREAD_ANY) {

        if(readyQueue) {
            // switch to next thread
            return thread_switch(want_tid, true);
        }
        return THREAD_NONE;
    }

    else { 
        
        if(searchThread(want_tid)) {
            // switch to next thread
            return thread_switch(want_tid, false);
        }
        return THREAD_INVALID;
    }
}

Tid
thread_exit(Tid tid)
{
    if(tid == THREAD_ANY) {
        if(readyQueue) {
            readyQueue->raiseExitFlag = true;
            return readyQueue->threadID;
        }
        return THREAD_NONE;
    }

    else if(tid == THREAD_SELF || tid == runningThread->threadID) {
        if(readyQueue) {
            
            // choose next thread to run, remove it from ready queue
            struct thread *next = remove_want_tid_thread(tid, true);

            destroyThread();
            runningThread = next;

            if(runningThread->raiseExitFlag) {
                thread_exit(THREAD_SELF);
            }
            else
                // restore next thread's CPU state
                setcontext(runningThread->threadContext);
        
            return runningThread->threadID;
        }

        else {
            free(runningThread->threadContext);
            free(runningThread);

            return THREAD_NONE;
        }
    }

    else {
        if(searchThread(tid)) {
            searchThread(tid)->raiseExitFlag = true;
            return tid;
        }
        return THREAD_INVALID;
    }
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in ... */
};

struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}
