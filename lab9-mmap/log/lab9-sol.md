# Lab 9: mmap
[handout](https://pdos.csail.mit.edu/6.828/2021/labs/mmap.html)

## mmap
As we did in [lab1](../../lab1-syscall/log/lab1-sol.md), add syscall `mmap` and `munmap`.

Read [user/mmaptest.c](../user/mmaptest.c), we will find that length is page-aligned, which means that 
we can make some safe optimization! For we used lazy allocation scheme, will need to handle **page fault**
caused by it. See [kernel/trap.c](kernel/trap.c) for how to handle such page faults.

[kernel/sysfile.c](../kernel/sysfile.c)
```c
uint64 sys_mmap(void) {
  uint64 addr;
  if (argaddr(0, &addr) < 0 || addr != 0U) {
    // in this lab addr is just zero
    return -1;
  }

  uint64 len;  // length
  int prot;
  int flags;
  struct file *fobj;
  int offset;
  if (argaddr(1, &len) < 0 || argint(2, &prot) < 0 || argint(3, &flags) < 0
   || argfd(4, 0, &fobj) < 0 || argint(5, &offset) < 0) {
    return -1;
  }
  filedup(fobj);

  if (!fobj->writable && (prot & PROT_WRITE) && !(flags & MAP_PRIVATE)) {
    // fobj is read-only.
    return -1;
  }

  // find an empty vma slot
  struct vma *vma = 0;
  struct proc *p = myproc();
  for (int i = 0; i < 16; ++i) {
    if (!p->vmas_[i].valid_) {
      vma = &(p->vmas_[i]);
      break;
    }
  }
  if (vma == 0) {
    // no usable vma struct!
    return -1;
  }

  vma->flags_ = flags;
  vma->prot_ = prot;
  vma->file_ = fobj;
  vma->offset_ = offset;

  // find an unused region
  vma->va_ = p->sz;
  vma->len_ = len;
  vma->valid_ = 1;

  p->sz += len;  // lazy allocation
  return vma->va_;
}
```

One bad news about `munmap` is that sometimes the user may "partly" unmap the file, but optimization is 
still possible because length of unmap is still page aligned in tests.
[kernel/sysfile.c](../kernel/sysfile.c)
```c
uint64 munmap(struct vma *vma, pagetable_t pgtbl, uint64 addr, uint64 len) {
  if (vma == 0 || !vma->valid_) {
    // not found
    return -1;
  }
  if (addr + len > vma->va_ + vma->len_) {
    // bad length
    return -1;
  }

  // write back for persistence
  if (vma->flags_ & MAP_SHARED && vma->prot_ & PROT_WRITE) {
    filewrite(vma->file_, addr, len);
  }

  // unmap the memory pages
  for (uint64 a = addr; a < addr + len; a += PGSIZE) {
    safe_uvmunmap(pgtbl, a, 1U, 1);
  }
  vma->va_ += len;
  vma->len_ -= len;
  // if freeing entire file
  if (vma->len_ == 0) {
    vma->file_->ref--;
    vma->valid_ = 0;
  }

  return (uint64)0;
}

uint64
sys_munmap(void) {
  uint64 addr;
  uint64 len;
  if (argaddr(0, &addr) < 0) { return -1; }
  if (argaddr(1, &len) < 0) { return -1; }

  struct proc *p = myproc();
  struct vma *vma = 0;
  for (int i = 0; i < 16; ++i) {
    if (p->vmas_[i].valid_ && in_vma(&(p->vmas_[i]), addr)) {
      vma = &(p->vmas_[i]);
      break;
    }
  }

  return munmap(vma, p->pagetable, addr, len);
}
```

Also, need to modify [kernel/proc.c](../kernel/proc.c) to change the behavior of `exit` and `fork`.
For example, `fork` need to copy the vma array defined in `struct proc`(see [kernel/proc.h](../kernel/proc.h)) 
and increment ref count of file; `exit` should unmap the process's mapped regions as if munmap had been called.

> An annoying situation in this lab is that several methods in kernel/vm.c will panic because 
> of not mapped pages. I simply skip unmapped pages in these methods, and later proved to be
> working. Here's some of methods that require such modification:

[kernel/vm.c](../kernel/vm.c)
```c
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
  uint64 ak
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
    continue;  // skip unmapped page, and not panic
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```
[kernel/vm.c](../kernel/vm.c)
```c
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
    continue;  // do not panic
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

## run all
```
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-mmap
make: 'kernel/kernel' is up to date.
== Test running mmaptest == (2.6s) 
== Test   mmaptest: mmap f ==
  mmaptest: mmap f: OK
== Test   mmaptest: mmap private ==
  mmaptest: mmap private: OK
== Test   mmaptest: mmap read-only ==
  mmaptest: mmap read-only: OK
== Test   mmaptest: mmap read/write ==
  mmaptest: mmap read/write: OK
== Test   mmaptest: mmap dirty ==
  mmaptest: mmap dirty: OK
== Test   mmaptest: not-mapped unmap ==
  mmaptest: not-mapped unmap: OK
== Test   mmaptest: two files ==
  mmaptest: two files: OK
== Test   mmaptest: fork_test ==
  mmaptest: fork_test: OK
== Test usertests == usertests: OK (150.9s)
== Test time ==
time: OK
Score: 140/140
```

This completes this lab, and all labs of this course! How nice.