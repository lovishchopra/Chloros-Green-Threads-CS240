// CS240: Advanced Topics in Operating Systems: Lab 1
// Cooperative User Level Threads Library: Chloros
// Lovish Chopra
// lovish@stanford.edu

#include "chloros.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <signal.h>
#include <unistd.h>

// The run queue is a doubly linked list of threads. The front and the back of
// the list are tracked here.
static struct thread* runq_front;
static struct thread* runq_back;
static atomic_int next_id = 0;
static int alarm_rate = 0;

// Push a thread onto the front of the run queue.
static void runq_push_front(struct thread* n) {
    n->next = runq_front;
    n->prev = NULL;
    if (runq_front)
        runq_front->prev = n;
    else
        runq_back = n;
    runq_front = n;
}

// Push a thread onto the back of the run queue.
static void runq_push_back(struct thread* n) {
    n->next = NULL;
    n->prev = runq_back;
    if (runq_back)
        runq_back->next = n;
    else
        runq_front = n;
    runq_back = n;
}

// Remove a thread from the run queue.
static void runq_remove(struct thread* n) {
    if (n->next)
        n->next->prev = n->prev;
    else
        runq_back = n->prev;
    if (n->prev)
        n->prev->next = n->next;
    else
        runq_front = n->next;
}

// Use a 2MB stack.
#define STACK_SIZE (1 << 21)

// Corresponds to thread_start in swtch.S.
void thread_start(void);

// Corresponds to ctxswitch in swtch.S.
void ctxswitch(struct context* old, struct context* new);

// The scheduler thread.
static struct thread* scheduler;

// The currently executing thread (or NULL).
static struct thread* running;

// This is the function that the scheduler runs. It should continuously pop a
// new thread from the run queue and attempt to run it.
static void schedule(void* _) {
    (void) _;

    // While there are still threads in the run queue, pop the next one
    // and run it. Make sure to adjust 'running' before you switch to it. Once
    // it context switches back to the scheduler by calling yield, you can
    // either put it back in the run queue (if it is still runnable), or
    // destroy it (if it exited).
    //
    // Hint: you can implement this how you like, but you might want to think
    // about what happens when the scheduler first starts up. At that point
    // 'running' might be the initial thread, which should first be put back on
    // the run queue before the main scheduler loop begins.
    while(1) {
        // If the previously running thread had exited, we will free the thread,
        // else, we will put it back into the run queue
        if(running->state == STATE_EXITED) {
            free(running->stack);
            free(running);
        }
        else {
            runq_push_back(running);
        }

        // If there are elements in the run queue, we will pop the first
        // element and run it.
        // Else if no more threads are left to run, we break out of the while 
        // loop and exit the scheduler.
        if(runq_front){    
            running = runq_front;
            runq_remove(running);
            // Set a new alarm since we are switching to the new thread.
            ualarm(alarm_rate, 0);
            ctxswitch(&scheduler->ctx, &running->ctx);
        }
        else {
            break;
        }
    }
}

// Creates a new thread that will execute fn(arg) when scheduled. Return the
// allocated thread, or NULL on failure.
static struct thread* thread_new(threadfn_t fn, void* arg) {
    // Allocate a new thread. This should give the thread a stack and
    // set up its context so that when you context switch to it, it will start
    // executing fn(arg).
    //
    // Hint: set up the thread so that when it gets context switched for the
    // first time, it will execute thread_start() with 'fn' at the top of the
    // stack, and arg above 'fn'.
    struct thread* t_new = (struct thread*) malloc(sizeof(struct thread));
    t_new->id = next_id++;
    t_new->state = STATE_RUNNABLE;
    // Use aligned alloc to align the allocation to 16
    t_new->stack = (uint8_t *) aligned_alloc(16, STACK_SIZE);   
    t_new->ctx.mxcsr = 0x1F80;
    t_new->ctx.x87 = 0x037F;

    /*
    Look of stack in memory:
    ------------
   |   arg      | 
    ------------
   |   fn       |
    ------------
   |thread_start|
    ------------ <- final rsp
   |            |
   |            |
   |  rest of   |
   |  the stack |
   |  is empty  |
   |            |
   |            |
   |            |
   |            |
    ------------- <- pointer to stack allocation (t_new->stack)
    */
    uint64_t rsp = (uint64_t) t_new->stack + STACK_SIZE;
    *(void **)(rsp - 16) = arg;    
    *(void **)(rsp - 24) = fn;
    *(void **)(rsp - 32) = thread_start;
    t_new->ctx.rsp = rsp - 32;

    return t_new;
}

// Initializes the threading library. This should create the scheduler (a
// thread that executes scheduler(NULL)), and register the caller as a thread.
// Returns true on success and false on failure.
bool thread_init(void) {
    // Create the scheduler by allocating a new thread for it
    scheduler = thread_new(schedule, NULL);

    // Register the initial thread (the currently executing context) as
    // a thread. It just needs a thread object but doesn't need a stack or any
    // initial context as it is already running. Make sure to update 'running'
    // since it is already running.
    struct thread* initial_thread = (struct thread *) malloc(sizeof(struct thread));
    initial_thread->id = next_id++;
    initial_thread->state = STATE_RUNNABLE;
    running = initial_thread;

    return true;
}

// Spawn a new thread. This should create a new thread for executing fn(arg),
// and switch to it immediately (push it on the front of the run queue, and
// switch to the scheduler).
bool thread_spawn(threadfn_t fn, void* arg) {
    // Thread spawn should disable any existing alarm because it is a scheduling activity.
    // The thread is yielding voluntarily and creating a new thread. The new alarm should 
    // go off only alarm_rate microseconds after the new thread is scheduled and running. 
    // This design choice is carefully made to avoid potential starvation and has been 
    // explained in the extra_credit.txt file.
    ualarm(0, 0);

    // Create a new thread, push it to the front of the run queue and context switch
    struct thread* t_new = thread_new(fn, arg);
    runq_push_front(t_new);
    ctxswitch(&running->ctx, &scheduler->ctx);
    return true;
}

// Wait until there are no more other threads.
void thread_wait(void) {
    while (thread_yield()) {}
}

// Yield the currently executing thread. Returns true if it yielded to some
// other thread, and false if there were no other threads to yield to. If
// there are other threads, this should yield to the scheduler.
bool thread_yield(void) {
    // Thread yield should disable any existing alarm because it is a scheduling activity.
    // The thread is yielding voluntarily and the new alarm should go off only alarm_rate
    // microseconds after the new thread is scheduled. This design choice is carefully made 
    // to avoid potential starvation and has been explained in the extra_credit.txt file.
    ualarm(0, 0);

    assert(running != NULL);
    // if there are no threads in the run queue, return false. Otherwise
    // switch to the scheduler so that we can run one of them.
    if(!runq_front){
        return false;
    }

    // Context switch to scheduler
    ctxswitch(&running->ctx, &scheduler->ctx);
    return true;
}

// The entrypoint for a new thread. This should call the requested function
// with its argument, and then do any necessary cleanup after the function
// completes (to cause the thread to exit).
void thread_entry(threadfn_t fn, void* arg) {
    fn(arg);
    running->state = STATE_EXITED;
    thread_yield();
    // this should never happen
    assert(!"exited thread resumed");
}
void handler(int signum){
    // Printing a small statement just for representation purposes.
    // This will not be a part of a standard production code.
    printf("SIGALRM causing preemption.\n");
    // Simply call thread_yield
    thread_yield();
}

void register_alarm(useconds_t rate) {
    // Register signal handler for SIGALRM
    struct sigaction action = {0};
    action.sa_sigaction = (void *)handler;
    action.sa_flags = SA_NODEFER;
    if (sigaction(SIGALRM, &action, NULL) == -1) {
        perror("sigaction");
        exit(0);
    }
    // Register the alarm rate in a static variable
    alarm_rate = rate;
    // Set an initial alarm
    ualarm(alarm_rate, 0);
}