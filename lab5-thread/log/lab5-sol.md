# Lab 5: multithreading
[handout](https://pdos.csail.mit.edu/6.828/2021/labs/thread.html)

## Prerequisites
### use threads
This section is a summary of this [manual](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_create.html).
```c
#include <pthread.h>
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_routine)(void*), void *arg);
```
If nullptr is passed to `attr`, the method just do the default behavior. Return 0 on success.

```c
#include <pthread.h>
int pthread_join(pthread_t thread, void **value_ptr);
```

Note that `pthread_join` will suspend execution of the calling thread until the target thread terminates.

### use locks
As we will see in future tasks, threads may concurrently visit and crash global data structures. Lock mechanism
is here to prevent such condition.
```c
#include <pthread.h>

pthread_mutex_t lock;            // declare a lock
pthread_mutex_init(&lock, NULL); // initialize the lock
pthread_mutex_lock(&lock);       // acquire lock
pthread_mutex_unlock(&lock);     // release lock
```

It is your task to determine when to acquire and release locks. You should neven acquire the same lock 
in a single thread, which will lead to deadlock.

## uthread
Our task is to write a simple user thread library.

### <a href="../user/uthread_switch.S"> uthread_switch.S</a>
Simply store and then load all general-purpose registers. A good reference is 
<a href="../kernel/swtch.S">kernel/swtch.S</a>. This task is a bit tedious though.

```s
	.globl thread_switch
thread_switch:
	## YOUR CODE HERE ##
	# store registers of prev thread
  sd ra, 0(a0)
  sd sp, 8(a0)
  sd gp, 16(a0)
  # skipped code for t, s, a regs

	# load registers of thread to execute 
	add t0, a1, x0 # why? (hint: this is on purpose) 
  ld ra, 0(t0)
  ld sp, 8(t0)
  ld gp, 16(t0)
  # skipped code for t, s, a regs
  ld t0, 32(t0) # so as not to overwrite t0 so early!

	ret    # return to ra 
```

### <a href="../user/uthread.c">uthread.c </a>
```c
void thread_create(void (*func)()) {
  struct thread *t;

  // find and allocate a thread
  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  
  // set the value of sp.
  uint64 x = (uint64)t->stack + STACK_SIZE;
  uint64 *st = (uint64 *)t->stack;
  uint64 y;
  asm volatile("mv %0, ra" : "=r" (y));
  st[0] = y;  // ra
  st[1] = x;  // sp

  // call the function to start the thread.
  func();
}
```
One **pitfall** here is that you should set `ra, sp` registers since `t->stack` is just junk now!

If you almost forget inline assembly(in my case I did forget about it), read my previous
[log](../../lab3-trap/log/lab3-sol.md).

And modification to scheduler is adding function call to `thread_switch`:
```c
void thread_schedule(void) {
  struct thread *t, *next_thread;
  /** skipped a lot of code */

  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE */
    thread_switch((uint64)t, (uint64)current_thread);
  } else
    next_thread = 0;
}
```

Results:
```
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-thread "uthread"
make: 'kernel/kernel' is up to date.
== Test uthread == uthread: OK (1.4s) 
    (Old xv6.out.uthread failure log removed)
```

## ph
Trivial lock-unlock practice. My <a href="../notxv6/ph.c">implementation</a> first initialize 5 latches
(one for each bucket), and acquire them at each access, release them when finished.

Results:
```
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-thread "ph_fast"
make: 'kernel/kernel' is up to date.
== Test ph_fast == gcc -o ph -g -O2 -DSOL_THREAD -DLAB_THREAD notxv6/ph.c -pthread
ph_fast: OK (19.0s) 
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-thread "ph_safe"
make: 'kernel/kernel' is up to date.
== Test ph_safe == make: 'ph' is up to date.
ph_safe: OK (9.0s) 
```

## barrier
I think my <a href="../notxv6/barrier.c">implementation </a> is very clear, hence no more explanation provided.

```c
// go to sleep on cond, releasing lock mutex, acquiring upon wake up
pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *lock);
// wake up every thread sleeping on cond 
pthread_cond_broadcast(pthread_cond_t *cond);

static void barrier() {
  // acquire the lock
  pthread_mutex_lock(&bstate.barrier_mutex); 
  ++bstate.nthread;

  if (bstate.nthread == nthread) {
    // clear thread count
    bstate.nthread = 0;
    ++bstate.round;
    // wakeup all threads
    pthread_cond_broadcast(&bstate.barrier_cond);
  } else // suspend the thread
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  // release the lock(upon finishing, the lock must not be taken!)
  pthread_mutex_unlock(&bstate.barrier_mutex); 
}
```

Results:
```
make: 'kernel/kernel' is up to date.
== Test barrier == make: 'barrier' is up to date.
barrier: OK (2.8s) 
```

## run all
```
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-thread
make: 'kernel/kernel' is up to date.
== Test uthread == uthread: OK (1.1s) 
== Test answers-thread.txt == answers-thread.txt: OK 
== Test ph_safe == make: 'ph' is up to date.
ph_safe: OK (8.8s)
== Test ph_fast == make: 'ph' is up to date.
ph_fast: OK (19.9s)
== Test barrier == gcc -o barrier -g -O2 -DSOL_THREAD -DLAB_THREAD notxv6/barrier.c -pthread
barrier: OK (3.2s)
== Test time ==
time: OK
Score: 60/60
```
This completes lab 5.