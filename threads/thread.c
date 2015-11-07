#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"


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
    interrupts_on();
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
    int enabled;
    /* disable signals, store the previous signal state in "enabled" */
    enabled = interrupts_off();
    assert(!interrupts_enabled());

    return runningThread->threadID;
    interrupts_set(enabled);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    int enabled;
    /* disable signals, store the previous signal state in "enabled" */
    enabled = interrupts_off();
    assert(!interrupts_enabled());

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
        newThreadContext->uc_stack.ss_sp = malloc(THREAD_MIN_STACK);
        if(!(newThreadContext->uc_stack.ss_sp)) {
            free(newThread);
            free(newThread->threadContext);
            
            interrupts_set(enabled);
            return THREAD_NOMEMORY;
        }

        newThreadContext->uc_stack.ss_size = THREAD_MIN_STACK;
        newThreadContext->uc_stack.ss_flags = 0;

        newThreadContext->uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;
        newThreadContext->uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;
        newThreadContext->uc_mcontext.gregs[REG_RIP] = (unsigned long)thread_stub;
        newThreadContext->uc_mcontext.gregs[REG_RSP] = (unsigned long)(THREAD_MIN_STACK + (newThreadContext->uc_stack.ss_sp) - 8);
    
        int index = 0;
        while(traceThreadID[index])
            index++;

        traceThreadID[index] = true;
        newThread->threadID = ++numThread;
        newThread->raiseExitFlag = false;
        add_to_ready_queue(newThread);

        Tid ret = newThread->threadID;
        
        interrupts_set(enabled);
        return ret;
    }
    interrupts_set(enabled);
    return THREAD_NOMORE;
}

Tid
thread_switch(Tid tid, bool isAny)
{
    int enabled;
    /* disable signals, store the previous signal state in "enabled" */
    enabled = interrupts_off();
    assert(!interrupts_enabled());

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
    Tid ret = next->threadID;
    interrupts_set(enabled);
    return ret;
}

void 
destroyThread(void)
{
    int enabled;
    /* disable signals, store the previous signal state in "enabled" */
    enabled = interrupts_off();

    traceThreadID[runningThread->threadID - 1] = false;
    free(runningThread->threadContext->uc_stack.ss_sp);
    free(runningThread->threadContext);
    free(runningThread);
    numThread--;

    interrupts_set(enabled);
}

Tid
thread_yield(Tid want_tid)
{
    int enabled;
    /* disable signals, store the previous signal state in "enabled" */
    enabled = interrupts_off();
    assert(!interrupts_enabled());

    if(want_tid == THREAD_SELF || want_tid == 0) {
        // Returns thread ID of the running thread
        Tid ret = runningThread->threadID;

        interrupts_set(enabled);
        return ret;
    }
        
    else if(want_tid == THREAD_ANY) {

        if(readyQueue) {
            // Switch to next thread
            Tid ret = thread_switch(want_tid, true);
            
            interrupts_set(enabled);
            return ret;
        }
        interrupts_set(enabled);
        return THREAD_NONE;
    }

    else { 
        
        if(searchThread(want_tid)) {
            // Switch to next thread
            Tid ret = thread_switch(want_tid, false);
            
            interrupts_set(enabled);
            return ret;
        }
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
}

Tid
thread_exit(Tid tid)
{
    int enabled;
    /* disable signals, store the previous signal state in "enabled" */
    enabled = interrupts_off();

    if(tid == THREAD_ANY) {
        if(readyQueue) {
            readyQueue->raiseExitFlag = true;
            Tid ret = readyQueue->threadID;

            interrupts_set(enabled);
            return ret;
        }
        interrupts_set(enabled);
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
        
            Tid ret = runningThread->threadID;
            
            interrupts_set(enabled);
            return ret;
        }

        else {
            destroyThread();
            interrupts_set(enabled);
            return THREAD_NONE;
        }
    }

    else {
        if(searchThread(tid)) {
            searchThread(tid)->raiseExitFlag = true;
            
            interrupts_set(enabled);
            return tid;
        }
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* This is the wait queue structure */
struct wait_queue {
    struct thread *blockedQueue;
    struct wait_queue *next;
};

struct wait_queue *
wait_queue_create()
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());	

    struct wait_queue *wq;

    wq = malloc(sizeof(struct wait_queue));
    assert(wq);

    wq->blockedQueue = NULL;
    wq->next = NULL;

    interrupts_set(enabled);
    return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());

    if(!wq->blockedQueue)
        free(wq);

    interrupts_set(enabled);
}

void
add_to_wait_queue(struct thread *Thread, struct wait_queue *queue)
{
    if(!queue->blockedQueue)
    {
        queue->blockedQueue = Thread;
        Thread->next = NULL;
        return;
    }

    struct thread *curr = queue->blockedQueue;
    while (curr->next)
        curr = curr->next;
     
    curr->next = Thread;
    Thread->next = NULL;
    return;
}

struct thread *
remove_block_thread(struct wait_queue *queue, int all)
{
    if (queue->blockedQueue) {

        struct thread *curr = queue->blockedQueue;

        if(all) {
            queue->blockedQueue = NULL;
            return curr;
        }
        else {
            queue->blockedQueue = curr->next;
            curr->next = NULL;
            return curr;
        }
    }
    return NULL;
}

Tid
thread_sleep(struct wait_queue *queue)
{
    int enabled;
    /* disable signals, store the previous signal state in "enabled" */
    enabled = interrupts_off();
    assert(!interrupts_enabled());

    if(!queue) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    if(readyQueue) {

        // Checks if in the context of currntly running thread.
        // resolves the issue when setcontext() returns form and execution started
        // right after the getcontext() because of saved PC value at that point.
        bool inCurrentThreadContext = true;
    
        // choose next thread to run, remove it from ready queue
        struct thread *next = remove_want_tid_thread(THREAD_ANY, true);

        // enqueue current in wait queue
        add_to_wait_queue(runningThread, queue);
    
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
        Tid ret = next->threadID;
        interrupts_set(enabled);
        return ret;
    }
    interrupts_set(enabled);
    return THREAD_NONE;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
    int enabled;
    /* disable signals, store the previous signal state in "enabled" */
    enabled = interrupts_off();
    assert(!interrupts_enabled());

    if(!queue || !queue->blockedQueue) {
        interrupts_set(enabled);
        return 0;
    }

    else if(all == 0) {
        add_to_ready_queue(remove_block_thread(queue, 0));
        interrupts_set(enabled);
        return 1;
    }
    else {
        int count = 0;
        struct thread *ret = remove_block_thread(queue, 1);
        while(ret) {
            struct thread *curr = ret;
            ret = ret->next;
            add_to_ready_queue(curr);
            count++;
        }
        interrupts_set(enabled);
        return count;
    }
}

struct lock {
    bool isAcquired;
    struct wait_queue *wq;
};

struct lock *
lock_create()
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());

    struct lock *lock;

    lock = malloc(sizeof(struct lock));
    assert(lock);

    lock->isAcquired = false;
    lock->wq = wait_queue_create();
    
    interrupts_set(enabled);
    return lock;
}

void
lock_destroy(struct lock *lock)
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());	

    assert(lock != NULL);

    if(!lock->isAcquired) {
        free(lock->wq);
        free(lock);
    }

    interrupts_set(enabled);
}

bool 
tset(struct lock *lock)
{
    bool old = lock->isAcquired;
    lock->isAcquired = true;
    return old;
}

void
lock_acquire(struct lock *lock)
{
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());

    assert(lock != NULL);
    
    while(tset(lock))
        thread_sleep(lock->wq);
    
    interrupts_set(enabled);
}

void
lock_release(struct lock *lock)
{
    int enabled;
    enabled = interrupts_off();
    assert(!interrupts_enabled());
	
    assert(lock != NULL);
    
    if(lock->isAcquired) {
        lock->isAcquired = false;
        thread_wakeup(lock->wq, 1);
    }
    interrupts_set(enabled);
}

struct cv {
    struct wait_queue *wq;
};

struct cv *
cv_create()
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());

    struct cv *cv;

    cv = malloc(sizeof(struct cv));
    assert(cv);
	
    cv->wq = wait_queue_create();

    interrupts_set(enabled);
    return cv;
}

void
cv_destroy(struct cv *cv)
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());
	
    assert(cv != NULL);

    if(!cv->wq)
        free(cv);

    interrupts_set(enabled);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());

    assert(cv != NULL);
    assert(lock != NULL);

    if(lock->isAcquired){
        lock_release(lock);
        thread_sleep(cv->wq);
        lock_acquire(lock);
    }

    interrupts_set(enabled);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());
	
    assert(cv != NULL);
    assert(lock != NULL);

    if(lock->isAcquired)
        thread_wakeup(cv->wq, 0);

    interrupts_set(enabled);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_off();
    assert(!interrupts_enabled());

    assert(cv != NULL);
    assert(lock != NULL);

    if(lock->isAcquired)
        thread_wakeup(cv->wq, 1);

    interrupts_set(enabled);
}
