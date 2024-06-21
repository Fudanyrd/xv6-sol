# Lab 1: System Calls
[handout](https://pdos.csail.mit.edu/6.828/2021/labs/syscall.html)

## Prerequisites
Run the following to setup:
```sh
git fetch && \
git checkout syscall && \
make clean
```

We first read `chapter 2` of riscv-book to learn **how to write a syscall**. Let's consider solution
to the exercise in riscv-book: 
> Add a system call to xv6 that returns the amount of free memory available.

Follow the handout, read [syscall.c](../kernel/syscall.c).

```c
// in kernel/syscall.c, line 107
static uint64 (* syscalls[])(void) = {
[SYS_fork]    sys_fork,
  ...
};

/** system call: fork executor */
extern uint64_t sys_fork(void);
```
This is a mapping from system call id to "system call executor". Hence, we need to add our **free mem syscall**
to it:

```c
/** added in kernel/syscall.h */
#define SYS_fremem 22  // add our own syscall id

/** added in kernel/syscall.c */
static uint64 (*syscalls[])(void) = {
[SYS_close]   sys_close,
[SYS_fremem]  sys_fremem,  // our free mem syscall
};
// our free mem syscall executor
extern uint64_t sys_fremem(void);  

/** added in kernel/sysproc.c */
uint64 sys_fremem(void) {
  // from riscv-book page 26, the maximum address is
  // 0x3fffffffff, just return it now.
  return 0x3fffffffff;
}
```

Wait, will this really work? Look at `kernel/syscall.c` for answer:
```c
// in kernel/syscall.c, line 133
void syscall(void) {
  int num;  // syscall id
  struct proc *p = myproc();  // current process

  num = p->trapframe->a7;  // syscall id is in $a7 register
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // if id is valid: call the executor.
    p->trapframe->a0 = syscalls[num]();
  } else {
    // else fail the syscall, return -1(put return value in $a0)
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

Write a simple user program to test the functionality of `fremem` syscall:
```c
/** create user/fremem.c */
#include "user/user.h"

int main(int argc, char **argv) {
  printf("%l \n", fremem());
  exit(0);
}

/** add in user/user.h */
uint64 fremem(void);
```

But we don't have a method named `fremem` yet?! Let compiler check it:
```
root@fdyrd:/mnt/d/handouts/xv6/code# make clean && make qemu
(output is truncated)...
riscv64-linux-gnu-ld -z max-page-size=4096 -N -e main -Ttext 0 -o user/_fremem user/fremem.o user/ulib.o user/usys.o user/printf.o user/umalloc.oriscv64-linux-gnu-ld: user/fremem.o: in function `main':
/mnt/d/handouts/xv6/code/user/fremem.c:4: undefined reference to `fremem'
```
Notice that user program will not link to kernel, and `user/usys.c` does not exist! Hence we need to 
hack `user/usys.pl` instead:
```pl
# snip
entry("uptime");
entry("fremem");  # add fremem here
```

Then we can run `make qemu` again, and
```
hart 1 starting
hart 2 starting
init: starting sh
$ fremem
4294967295
```
Wait, shouldn't the return value be 0x3fffffffff ?! What went wrong? Then I read `printf` method, 
and noticed that `uint64` is treated as `int`. Need to rewrite part of `printf`. Now the output should be correct:
```
hart 2 starting
hart 1 starting
init: starting sh
$ fremem
274877906943
$ QEMU: Terminated
```

Then modify our `sys_fremem` so that it returns free memory.
```c
uint64_t sys_fremem(void) {
  // sz is size of process memory
  return myproc()->sz;
}
```

And rerun `fremem`... Done!

To sum up, to add a syscall, we should follow the following steps:
<ol>
  <li>Add syscall id in <b>kernel/syscall.h</b>;</li>
  <li>Add syscall id mapping in <b>kernel/syscall.c</b>;</li>
  <li>Add syscall name in <b>user/usys.pl</b>;</li>
  <li>Add syscall definitionn in <b>user/user.h</b>;</li>
</ol>

## Syscall tracing
Simply follow the hints in handout.

1. Add `$U/_trace` to UPROGS in `Makefile`: no more explanation.
2. add a new variable in the `proc` struct, also need to set `trace mask` to 0 when 
  initializing and killing a `proc` struct!
```c
/** in proc.h */
struct proc { 
  // added, mask of traced syscalls of this process
  int trace_mask_;    
};

/** in proc.c */
void procinit(void) {
  struct proc *p;
  ...
  for(p = proc; p < &proc[NPROC]; p++) {
    ...
    // set the trace mask to 0.
    p->trace_mask_ = 0;
  }
}
// other methods with similar change:
// int kill(int pid);
// void freeproc(struct proc *p);
// As for fork, need to make child has the same mask!
```

3. implement `uint64 sys_trace()` in `kernel/sysproc.c`, which is surprisingly trivial:
```c
uint64 sys_trace(void) {
  // fetch trace argument(mask)
  int arg; argint(0, &arg);
  myproc()->trace_mask_ = arg;
  return 0;
}
```

4. Modify the `syscall()` function.
```c
/** name of 24 syscalls, add invalid to avoid NULL */
static char *syscall_names[] = {
  [SYS_invalid] "invalid syscall",
  [SYS_fork]    "syscall fork",
  [SYS_exit]    "syscall exit",
  [SYS_wait]    "syscall wait",
  [SYS_pipe]    "syscall pipe",
  [SYS_read]    "syscall read",
  [SYS_kill]    "syscall kill",
  [SYS_exec]    "syscall exec",
  [SYS_fstat]   "syscall fstat",
  [SYS_chdir]   "syscall chdir",
  [SYS_dup]     "syscall dup",
  [SYS_getpid]  "syscall getpid",
  [SYS_sbrk]    "syscall sbrk",
  [SYS_sleep]   "syscall sleep",
  [SYS_uptime]  "syscall uptime",
  [SYS_open]    "syscall open",
  [SYS_write]   "syscall write",
  [SYS_mknod]   "syscall mknod",
  [SYS_unlink]  "syscall unlink",
  [SYS_link]    "syscall link",
  [SYS_mkdir]   "syscall mkdir",
  [SYS_close]   "syscall close",
  [SYS_fremem]  "syscall freemem",
  [SYS_trace]   "syscall trace",
};

void syscall(void) {
  int ret;  // return value of syscall.
  ...
  // need to take the lock, cause we're reading the pid!
  acquire(&p->lock);
  if ((1 << num) & p->trace_mask_) {
    // the syscall is traced!
    printf("%d: %s -> %d\n", p->pid, syscall_names[num], ret);
  }
  release(&p->lock);
}
```

And now we can run the test script:
```
== Test trace 32 grep == trace 32 grep: OK (1.5s) 
== Test trace all grep == trace all grep: OK (0.9s) 
== Test trace nothing == trace nothing: OK (0.9s) 
== Test trace children == trace children: OK (9.8s)
```

That's it.

## Sysinfo
1. To collect the amount of free memory, add a function to `kernel/kalloc.c`
```c
uint64 availmem() {
  uint64 ret = 0U;
  struct run *r;
  for (r = kmem.freelist; r; r = r->next) {
    ret += PGSIZE;
  }
  return ret;
}
```

2. To collect the number of processes, add a function to `kernel/proc.c`
```c
// return number of unused processes
static int numproc() {
  int ret = 0;
  struct proc *p = 0; 
  for (int i = 0; i < NPROC; ++i) {
    p = proc + i;
    acquire(&p->lock);
    if (p->state != UNUSED) {
      ++ret;
    }
    release(&p->lock);
  }

  return ret;
}
```

3. Implement `sys_sysinfo` in `kernel/sysproc.c`
```c
extern uint64 availmem();
extern int numproc();
uint64 sys_sysinfo(void) {
  // fetch system stat
  struct sysinfo info;
  info.nproc = numproc();
  info.freemem = availmem();

  // fetch destination addr
  uint64 dest;
  argaddr(0, &dest);

  return copyout(myproc()->pagetable, dest, (char *)&info, sizeof(info));
}
```

Now we can compile and run tests:
```
== Test trace 32 grep == trace 32 grep: OK (1.6s) 
== Test trace all grep == trace all grep: OK (0.9s) 
== Test trace nothing == trace nothing: OK (0.9s) 
== Test trace children == trace children: OK (9.7s) 
== Test sysinfotest == sysinfotest: OK (1.4s) 
    (Old xv6.out.sysinfotest failure log removed)
== Test time == 
time: OK 
Score: 35/35
```

That's it.
