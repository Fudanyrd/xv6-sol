# Lab 3: Traps
[handout](https://pdos.csail.mit.edu/6.828/2021/labs/traps.html)  

## Prerequisites

### inline assembly
Inline assembly has the form:
```c
asm ("asm code" : input : output : clobbers);
// certain optimization is disabled, safer
asm volatile("asm code" : input : output : clobbers);
```

Input can be one of:
<table>
  <tr>
    <td>"r" </td>
    <td>specifies a register</td>
  </tr>
  <tr>
    <td>"m" </td>
    <td>specifies a mem location</td>
  </tr>
  <tr>
    <td>"rm" </td>
    <td>specifies either a register or mem loc</td>
  </tr>
</table>

Real example from `kernel/riscv.h`:
```c
static inline void w_stvec(uint64 x) {
  asm volatile("csrw stvec, %0" : : "r" (x));
}
```

Output can be one of:
<table>
  <tr>
    <td>"=r" </td>
    <td>specifies a register</td>
  </tr>
  <tr>
    <td>"=m" </td>
    <td>specifies a mem location</td>
  </tr>
  <tr>
    <td>"=rm" </td>
    <td>specifies either a register or mem loc</td>
  </tr>
</table>

Real example from `kernel/riscv.h`:
```c
static inline uint64 r_stvec() {
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}
```
In these examples `%0` specifies 0th argument in input/output.

### priv. riscv features

Priviledged registers:
<table>
  <tr>
    <td>satp</td>
    <td>physical address of page table root</td>
  </tr>
  <tr>
    <td>stvec</td>
    <td>ecall jumps here, points to trampoline</td>
  </tr>
  <tr>
    <td>sepc</td>
    <td>ecall saves user's pc here </td>
  </tr>
  <tr>
    <td>sscratch</td>
     <td>scratch space, used to store temporary data</td>
  </tr>
</table>

Priviledged instructions:
`csr` family: `csrr`(read), `csrw`(write), `csrrw`(swap).  
`sret`: return to user space

### kernel transition
Syscall/interrupt/faults enter kernel:
<ol>
  <li>Switch to supervisor mode</li>
  <li>Save pc and 32 regs</li>
  <li>Switch to kernel pagetable/stack</li>
  <li>Exec kernel code</li>
</ol>

### stack frame
The `gcc` compiler organize the stack frame in this way
([source](https://pdos.csail.mit.edu/6.828/2021/lec/l-riscv-slides.pdf)):
```c
/*
+----------------+  <- high addr
| Return Address |
+----------------+
| to Prev. fp    |
+----------------+
| saved register |
+----------------+  <- low addr
| local variable |
+----------------+  <- $sp
*/
```
The saved frame pointer lives at fixed offset (-16) from the frame pointer.
Also note that `gcc` stores current frame pointer in `s0` register.

## riscv asm
Skipped.

## Backtrace
First need some way to retrive s0 register(i.e. current frame pointer)
```c
// in riscv.h, get s0 register
static inline uint64 r_s0() {
  uint64 x;
  asm volatile ("addi %0, s0, 0" : "=r" (x));
  return x;
}
```

Then we write the `backtrace()` function. Output return address, and iterate by frame pointer.
```c
void backtrace() {
  uint64 fptr = r_s0();

  // overflow adr
  const uint64 overflow = PGROUNDUP(fptr);

  printf("backtrace:\n");
  while (fptr < overflow) {
    printf("%p\n", *(uint64 *)(fptr - 8U));
    fptr = *(uint64 *)(fptr - 16U);
  }
}
```

Results:
```
make: 'kernel/kernel' is up to date.
== Test backtrace test == backtrace test: OK (1.3s) 
```

## sigalarm
This part can be divided into 3 phases:

### test0
The right declaration:
```c
/** in user/user.h */
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```
For the present, `sigreturn` should just return 0, according to hints.
```c
/** in sysproc.c */
uint64 sys_sigalarm(void) {
  // alarm interval;
  int interval;
  argint(0, &interval);
  // handler
  uint64 handler;
  argaddr(1, &handler);

  // add to myproc
  struct proc *p = myproc();
  if (p == 0) {
    return -1;
  }
  p->handler_ = (handler_t)handler;
  p->ticks_ = interval;

  return 0;
}

uint64 sys_sigreturn(void) {
  return 0;
}
```
Add the prototypes, syscall ids in `kernel/syscall.h` and `kernel/syscall.c`(code not shown); store 
alarm interval and handler in `struct proc`.
```c
/** in types.h */
typedef void (*handler_t)();
/** in proc.h */
struct proc {
  // skipped a lot of members
  char name[16];               // Process name (debugging)
  int ticks_;                  // sigalarm clock ticks
  int passed_;                 // ticks passed since last call
  handler_t hander;            // sigalarm handler
};
```
Since we added two members, `allocproc` and `freeproc` should be modified to correctly initialize them.

Last, modify `usertrap` in `kernel/trap.c`:
```c
void usertrap(void) {
  ...
  
  /** added to end of usertrap */
  if (which_dev == 2 && p->ticks_ != 0) {
    // ok, timer interrupt.
    if (p->passed_ > p->ticks_) {
      panic("passed interval > ticks? Impossible!\n");
    }
    if (++p->passed_ == p->ticks_) {
      // call the handler(How? ).
      p->passed_ = 0;
      // change user's pc to the handler:)
      p->trapframe->epc = (uint64)p->handler_;
    } 
  }

  usertrapret();
}
```

Now we passed test0!

### test 1/2
Not that difficult. Remember, to store the entire `struct trapframe` when needed to 
call the handler and restore them when encountering syscall ``

Here's the full implementation of syscalls `sigalarm` and `sigreturn`.
```c
uint64 sys_sigalarm(void) {
  // alarm interval;
  int interval;
  argint(0, &interval);
  // handler
  uint64 handler;
  argaddr(1, &handler);

  // add to myproc
  struct proc *p = myproc();
  if (p == 0) {
    return -1;
  }
  p->handler_ = (handler_t)handler;
  p->ticks_ = interval;
  p->jmp2handler_ = interval > 0 ? 1 : 0;  // ok, can jump to handler

  return 0;
}

uint64 sys_sigreturn(void) {
  struct proc *p = myproc();
  if (p == 0) {
    panic("current proc is null\n");
  }

  // restore old program states
  if (p->ticks_ != 0) {
    *(p->trapframe) = p->tf_cache_;
  }
  p->jmp2handler_ = 1;  // can jump to handler again!
  p->passed_ = 0;

  return 0;
}
```

**Pitfall**: in my first implementation, I used `kalloc` to allocate another page to store state(i.e. trapframe)
of interrupted code because I feel that store an entire trapframe is too memory inefficient. 
But this somewhat does not work, costing me whole night to find out the page fault.
In retrospect, this is indeed bad design choice. Don't be like me. 

> Always choose simpler design choices first.

## run all
Results:
```
== Test answers-traps.txt == answers-traps.txt: OK 
== Test backtrace test == backtrace test: OK (1.4s) 
== Test running alarmtest == (3.4s) 
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-traps
make: 'kernel/kernel' is up to date.
== Test answers-traps.txt == answers-traps.txt: OK 
== Test backtrace test == backtrace test: OK (2.1s) 
== Test running alarmtest == (3.1s) 
== Test   alarmtest: test0 == 
  alarmtest: test0: OK 
== Test   alarmtest: test1 ==
  alarmtest: test1: OK
== Test   alarmtest: test2 ==
  alarmtest: test2: OK
== Test usertests == usertests: OK (112.2s)
== Test time ==
time: OK
Score: 85/85
```

My implementation is not breaking other components of the kernel! How nice.

# Signature
Author of this log and code of this lab:
> Fudanyrd(Rundong Yang), Jun 22, 2024
