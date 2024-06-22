#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
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


#ifdef LAB_PGTBL
extern pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
/** 
 * @return 1 if page is acceessed, and clear the PTE_A bit; 
 * 0 if not; -1 if failure.
 */
int inspect_pgaccess(pagetable_t pgtbl, uint64 va) {
  if (va >= MAXVA) {
    // don't cause walk method to panic;
    return -1;
  }

  pte_t *pte = walk(pgtbl, va, 0);
  if (pte == 0) {
    // oops, page fault!
    return -1;
  }

  int ret = 0;
  if (*pte & PTE_A) {
    ret = 1;
    *pte = *pte & ~PTE_A;
  }

  return ret;
}

int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.

  /** get args */
  uint64 tmp;
  argaddr(0, &tmp);
  void *addr = (void *)tmp;  // base address
  addr -= ((uint64)addr % PGSIZE);
  int pages;   // number of pages to inspect
  argint(1, &pages);
  argaddr(2, &tmp);
  unsigned int *mask = (unsigned int*)tmp;  // mask address

  // zero out the mask bits
  if (pages > 32) {
    return -1;
  }

  /** get the pagetable, walk down to a leaf. */
  pagetable_t pgtbl = myproc()->pagetable;
  unsigned int out = 0U;
  if (copyout(pgtbl, (uint64)mask, (char*)&out, 4) < 0) {
    return -1;
  }
  for (int i = 0; i < pages; ++i) {
    int ret = inspect_pgaccess(pgtbl, (uint64)(addr + PGSIZE * i));
    if (ret < 0) {
      // oops, fail
      return -1;
    }

    // set the mask.
    if (ret) {
      out |= (1U << i);
    }
  }

  if (copyout(pgtbl, (uint64)mask, (char*)&out, 4) < 0) {
    return -1;
  }

  return 0;
}
#endif

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
