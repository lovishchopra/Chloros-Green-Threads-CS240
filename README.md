# Overview

In this lab, you will write a cooperative user-level threads library, chloros
(a Greek word meaning "green"). You will be programming in C and x86-64
assembly on the myth cluster. If you're not familiar with myth cluster, see
[Stanford's Shared Computing
Environment](https://uit.stanford.edu/service/sharedcomputing).

User-level threads are similar to the type of threads you might be familiar
with, OS threads, but are implemented entirely in user-level code. They are
typically speedier than OS threads since there is no context switching into/out
of the kernel. Cooperative usel-level threads are user-level threads that
release control to the scheduler deliberately, usually through a
`thread_yield()` call. That is, they execute for as long as they want.

A user of the thread library would calls `thread_spawn(fn, arg)` to create a
new thread that executes `fn` with `arg`. The user calls `thread_yield()`
inside a thread to yield execution to another thread, including the initial
one. Finally, the user might call `thread_wait()` to wait for all of the
spawned threads to finish executing before ending the process.

A full program using your library might look like this:

```
#include "chloros.h"

#include <stdio.h>
#include <stdlib.h>

void worker(void* arg) {
    int num = *((int*) arg);

    for (int i = 0; i < 10; i++) {
        printf("hello from worker %d\n", num);
        thread_yield();
    }

    free(arg);
}

int main() {
    thread_init();
    for (int i = 0; i < 4; i++) {
        int* num = malloc(sizeof(int));
        *num = i;
        thread_spawn(&worker, num);
    }

    thread_wait();

    return 0;
}
```

# Phase 0: Getting Started

First, ensure that you are working on the lab using a machine meeting the following requirements:

* Runs a modern Unix on x86-64: Linux, BSD, or macOS.
* Runs a 64-bit variant of the OS (check with arch).
* Has GCC or Clang and GNU Make or [Knit](https://github.com/zyedidia/knit)
  installed.

If you don't have a machine that meets these requirements, you can work on the
lab through `myth.stanford.edu`.

## Skeleton code

The following files make up the codebase:

* `chloros.h`: defines types and function signatures for the Chloros library.
* `chloros.c`: defines the function implementations for Chloros. This is where
  the majority of your implementation will be.
* `swtch.S`: an assembly file with routines for context switching (which you
  will write), and thread entry.
* `test1.c`: a test program. We may release more test programs as well.
* `example.c`: an example program which you can play with to test your
  implementation.

## Build targets

The project has a Makefile and a Knitfile. To use the Knitfile, you must
install the [Knit](https://github.com/zyedidia/knit/releases/tag/v1.1.1) build
tool, but you can also just use Make (which might require more use of `make
-B`).

Both build tools have the following targets:

* `all`: compiles all targets.
* `test`: compiles and runs the test program.
* `test1`: compiles the test program (called `test1`).
* `example`: compiles the example program (called `example`).
* `clean`: removes temporary files.
* `format`: auto-formats using clang-format (optional).
* `submit`: packages your code for submission.

Run `make` or `knit` with `SAN=1` to enable the sanitizers. Your code should
pass the tests with sanitizers enabled.

# Phase 1: allocating threads

In phase 1 you will implement the `thread_new` function. See the `FIXME`
comment for details. This should allocate a new thread and stack and set it up
to execute `fn(arg)`. Remember that the stack pointer *must* be 16-byte
aligned. **This is important**. Make sure all stacks are unique (do not reuse
the same stack for multiple threads). You should also assign unique IDs for
threads (useful for debugging).

# Phase 2: context switching

In phase 2 you will implement the `ctxswitch` function in `swtch.S`.

The `ctxswitch` function will be called on behalf of a usel-level thread during
its execution to context switch from one thread to another. For a context
switch to be successful, all state of the currently executing thread must be
saved in some structure and the state of the thread being switched into must be
restored. A thread's execution state is also referred to as its context.

A thread's context is already defined in the `struct context` structure. All of
these registers are known as callee saved, which means that the callee of a
function guarantees that those registers will have the same value when the
function returns as they did when the function was called. In other words, a
function A calling another function B can expect those registers to contain the
same values after function B returns as they had when function B was called
because the callee (B) saves and restore those values. These registers are
defined this way by the [System V X64
ABI](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf)
calling convention, which is followed on most Unix systems running on 64-bit
machines. You can read more at the ABI reference link.

Apart from these registers, a thread's full context also includes the values in
its stack. Saving and restoring a thread's stack on each context switch would
be a very expensive operation, so instead of doing this, we simply give each
thread its own unique stack. Then, as long as each thread's stack pointer
points to its own unique stack, saving and restoring the stack pointer suffices
to save and restore the thread's stack.

You're ready to implement the context switching assembly function in
`swtch.S`. You can find the function's specification above its
definition. Keep in mind that according to the calling convention, the first
two parameters to a function are passed in the `%rdi` and `%rsi` registers. Also
note that GCC calls the GNU Assembler implicitly, which uses the GAS syntax for
assembly. You may wish to consult the calling convention in the [ABI reference](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf)
or the [X86 instruction reference](https://en.wikibooks.org/wiki/X86_Assembly).

This phase is not meant to grill your knowledge of assembly. You can implement
the necessary additions to `ctxswitch` using only two different assembly
instructions: `movq` and `ret`.

# Phase 3: yield and spawn

In phase 3 you will implement `thread_init`, `schedule`, `thread_yield`,
and `thread_spawn`. See the `FIXME` comments for details.

`thread_init` should set up a scheduler thread to run the `schedule` function
and should register the current execution context as a thread (no need to
allocate a stack for it though).

`schedule` should do round robin scheduling, where it pops the next thread off
the front of run queue, runs it (by context switching to it), and then either
pushes it onto the back of the run queue, or destroys it if it exited.

`thread_yield` should switch to the scheduler if there are threads to run.
Otherwise it should return `false`.

`thread_spawn` should create a new thread that executes `fn(arg)` and
immediately switch to it.

Once you implement these functions, you should make sure your implementation
passes the test in `test1`. You may also want to experiment by using the
`example` program. We also recommend writing some tests of your own. We may
release additional tests, or may use additional tests for grading.

# Submission

Once you've completed the tasks above, you're done and ready to submit. Ensure
that your lab and tests run as you expect them to on `myth.stanford.edu`. We
will grade your lab on the myth machines.

Don't forget to test your code with the sanitizers!

Run `make submit` and proceed to Gradescope to upload your submission.

# Extra credit

For extra credit, you can adapt your library to have some additional bells and
whistles:

* Multiple kernel threads. You can schedule the user threads using multiple
  kernel threads. Once you do this, you have to be careful about
  synchronization. Make sure you use atomic operations for shared memory
  (`_Atomic` in C). You will also want to use `-fsanitize=thread` to find data
  races.
* Preemptive threads: you may have heard that "user-level threads are not
  preemptive." While that's true for the base chloros library you have written
  so far, there's nothing fundamental stopping us from implementing preemption
  in chloros, even on a single kernel thread. In this task, you will first
  modify your user-level threading library to be preemptively scheduled. To do
  this, when the client program enables preemption, use `ualarm` to ask the
  operating system to send you a `SIGALRM` at some user-controlled rate. In your
  signal handler (registered with `sigaction`) you can change which thread is
  scheduled. The third `ucontext_t*` parameter to the signal handler contains the
  state of the current thread right when it was preempted by the signal. For
  this EC, you're welcome to use, e.g., the `swapcontext` methods to operate with
  these, so long as your code does not use such methods when preemption has not
  been enabled (e.g., in the provided test cases). 

If you do extra credit, write up a short report about what you did in
`extra_credit.txt` and a small test case.
