# Lab 2: Page Table

## Prerequisites
```sh
git fetch &&\
git checkout pgtbl &&\
make clean
```

## Speed up syscalls
Our task is to do the following:
> When each process is created, map one read-only page at USYSCALL (a virtual addr defined in memlayout.h). 
> At the start of this page, store a struct usyscall (also defined in memlayout.h), 
> and initialize it to store the PID of the current process.
> You should allocate and initialize the page in allocproc(), and free the page in freeproc().

This seems easy at first glance, just need to fix proc methods:
```c
/** in proc.c */
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    ...
  }
  return 0;

found:
  ...

#ifdef LAB_PGTBL
  // allocate one page for pid
  uint64 rop = (uint64)kalloc();
  if (rop == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  p->usys_ = (void *)rop;
  p->usys_->pid = p->pid;
#endif // USYSCALL
  ...

  return p;
}

static void
freeproc(struct proc *p)
{
  ...
#ifdef LAB_PGTBL
  if (p->usys_) {
    kfree(p->usys_);
  } 
  p->usys_ = 0;
#endif
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  ...
}

pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;
  ...

#ifdef LAB_PGTBL
  // map and initialize the page.
  if (p->usys_ && mappages(pagetable, USYSCALL, PGSIZE, (uint64)p->usys_, PTE_R) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, USYSCALL, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
#endif

  return pagetable;
}

void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
#ifdef LAB_PGTBL
  uvmunmap(pagetable, USYSCALL, 1, 0);
#endif
  uvmfree(pagetable, sz);
}
```

And:
```
$ pgtbltest
ugetpid_test starting
usertrap(): unexpected scause 0x000000000000000d pid=4
            sepc=0x0000000000000476 stval=0x0000003fffffd000
```

WTF is going on?! Howdy, I forgot to set the mem access to be user!
```c
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;
  ...

#ifdef LAB_PGTBL
  // map and initialize the page.
  if (p->usys_ && 
    mappages(pagetable, USYSCALL, PGSIZE, (uint64)p->usys_, PTE_R | PTE_U) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, USYSCALL, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
#endif

  return pagetable;
}
```

And still not correct:
```
$ pgtbltest
ugetpid_test starting
ugetpid_test: OK
pgaccess_test starting
pgtbltest: pgaccess_test failed: incorrect access bits set, pid=3
```

This may be the normal behavior, because syscall `pgaccess` is not implemented(TODO)!

## Print page table
Hints/Specs:
<ol>
  <li>Put vmprint() in kernel/vm.c</li>
  <li>Use the macros at the end of the file kernel/riscv.h</li>
  <li>Define the prototype for vmprint in kernel/defs.h </li>
</ol>

No more explanation, present my recursive solution:
```c
/** added to end to kernel/vm.c */
void vmprint_header(int repeat) {
  for (int i = 0; i < repeat; ++i) {
    printf(" ..");
  }
}
void vmprint_helper(pagetable_t pgtbl, int depth) {
  if (depth >= 3) { return; }
  pte_t pte;
  uint64 child;

  for (int i = 0; i < 512; ++i) {
    pte = pgtbl[i];
    child = PTE2PA(pte);
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      // this PTE points to a lower level page table.
      vmprint_header(depth + 1);
      printf("%d: pte %p pa %p\n", i, pte, child);
      vmprint_helper((pagetable_t)child, depth + 1);
    } else {
      // this PTE is a leaf page
      if (pte & PTE_V) {
        vmprint_header(depth + 1);
        printf("%d: pte %p pa %p\n", i, pte, child);
      }
    }
  }
}

void vmprint(pagetable_t pgtbl) {
  printf("page table %p\n", pgtbl);
  vmprint_helper(pgtbl, 0);
}
```
```c
/** added to exec function in kernel/exec.c */
  if(p->pid==1) {
    vmprint(p->pagetable);
  }
```

And run test:
```
== Test pte printout == pte printout: OK (0.6s) 
    (Old xv6.out.pteprint failure log removed)
```
That's it.

## Syscall pgaccess
We will want to use a subroutine that get the access stat of a virtual address:
```c
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
```

And we implement the `pgaccess` syscall(the same we as lab 1):
```c
int sys_pgaccess(void) {
  /** get args */
  uint64 tmp;
  argaddr(0, &tmp);
  void *addr = (void *)tmp;  // base address
  addr -= ((uint64)addr % PGSIZE);
  int pages;   // number of pages to inspect
  argint(1, &pages);
  argaddr(2, &tmp);
  unsigned int *mask = (unsigned int*)tmp;  // mask address

  if (pages > 32) {
    return -1;
  }

  /** get the pagetable, walk down to a leaf. */
  pagetable_t pgtbl = myproc()->pagetable;
  unsigned int out = 0U;
  if (copyout(pgtbl, (uint64)mask, (char*)&out, 4) < 0) {
    // zero out the mask bits should not fail
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

  // write the result back to user space
  return copyout(pgtbl, (uint64)mask, (char*)&out, 4); 
}
```

## Run all
```
== Test pgtbltest == (0.7s) 
== Test   pgtbltest: ugetpid == 
  pgtbltest: ugetpid: OK 
== Test   pgtbltest: pgaccess == 
  pgtbltest: pgaccess: OK 
== Test pte printout == pte printout: OK (0.8s) 
== Test answers-pgtbl.txt == answers-pgtbl.txt: OK 
== Test usertests == (103.3s) 
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-pgtbl
make: 'kernel/kernel' is up to date.
== Test pgtbltest == (0.7s) 
== Test   pgtbltest: ugetpid == 
  pgtbltest: ugetpid: OK 
== Test   pgtbltest: pgaccess == 
  pgtbltest: pgaccess: OK 
== Test pte printout == pte printout: OK (0.8s) 
== Test answers-pgtbl.txt == answers-pgtbl.txt: OK
== Test usertests == (139.2s)
== Test   usertests: all tests ==
  usertests: all tests: OK
== Test time ==
time: OK
Score: 46/46
```

That's it.
