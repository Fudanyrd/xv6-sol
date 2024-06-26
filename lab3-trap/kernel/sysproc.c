#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  backtrace();
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);

  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

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
