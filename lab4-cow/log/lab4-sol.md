# Lab 4: copy-on-write

## Prerequisite
Virtual address in xv6:
```c
/*
+------+------------+--------+
| EXT  | index(VPN) | Offset |
+------+------------+--------+
^      ^            ^        ^
63     39           12       0

index is divided into 3 levels(i.e. a tree of height 4):
+-------+------+------+
|  L2   |  L1  |  L0  |
+-------+------+------+

L2, L1, L0 are 9-bit each.
*/
```

Physical address in xv6(see chapter 3 of xv6-book):
```c
/*
+----------+-----+------+
| Reserved | PPN | Meta |
+----------+-----+------+
^          ^     ^      ^
63         54    10      0
*/
```

## Copy-on-write fork
In this part we shall implement copy-on-write fork.

### <a href = "../kernel/kalloc.c">kalloc.c</a>
In `kernel/kalloc.c` we need the following fix:
<ul>
  <li>
 Use a fixed size array to store the reference count. Need to figure out number of pages can be allocated by 
   `kalloc`. In my several experiment it is a fixed number `32729`, i.e. need 8 pages to store all refs to 
   physical pages if we use `unsigned char` to store ref count. To avoid conflicts, just use "end" as the
   start of this 32768-byte array(aligned to PGSIZE).
  </li>
  <li>
  Rewrite kalloc, kfree. See <a href="../kernel/kalloc.c">kernel/kalloc.c </a> for implementation.
  </li>
  <li>
  Add a new method realloc which simply increase the reference count. Will be used in later tasks.
  </li>
</ul>

Here's some methods in kalloc.c
```c
// make a unique copy from a page, return 0 if cannot fetch new pages.
void *kmake_unique(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kmake_unique");

  acquire(&kmem.lock); 
  const int idx = pa2ref_idx((uint64)pa);  // index into the ref counts
  if (ref_counts[idx] == 0) {
    // not allocated, abort.
    release(&kmem.lock);
    return 0;
  }

  if (ref_counts[idx] == 1) {
    // this is not a shared page, just return pa.
    release(&kmem.lock);
    return pa;
  }

  --ref_counts[idx];
  // look for a new page.
  struct run *r;
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    // set ref count of this unique page
    ref_counts[pa2ref_idx((uint64)r)] = 1;
    // copy the data of the page
    memmove(r, pa, PGSIZE);
  } 
  release(&kmem.lock);

  return (void *)r;
}
```
```c
// realloc page: simply increment ref count!
// panic if the page is not allocated by kalloc before.
void *krealloc(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("krealloc");

  acquire(&kmem.lock); 

  const int idx = pa2ref_idx((uint64) pa);
  if (ref_counts[idx] == 0) {
    release(&kmem.lock); 
    panic("krealloc, 129");
  } 
  ++ref_counts[idx];
  release(&kmem.lock); 

  return pa;
}
```

Note that we cannot directly call `kfree` or `kmalloc` in this methods,
this will take the same lock in a single thread(i.e. deadlock).

### <a href = "../kernel/vm.c">vm.c</a>
I got stuck in this part because I don't know what's the address that triggered page fault.
Then I reviewed this [ppt](https://pdos.csail.mit.edu/6.828/2021/slides/6s081-lec-usingvm.pdf):
> STVAL register: Page faults set it to the faulting address!

Pitfalls:
> Many user tests write to invalid address(especially those greater than VA and 0) 
> deliberately. You should check these conditions before calling methods in 
> kalloc.c, otherwise the kernel will panic and you will lose points in this lab.

### <a href = "../kernel/trap.c">trap.c</a>
Will need a handler for copied pages; this handler should detect whether a page is 
created by copy-on-write scheme. Here's my implementation of such cow handler:
```c
extern pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
extern void *kmake_unique(void *);

// handle cow related page fault, return 0 on success
int cow_handler(struct proc *p) {
  // which address caused page fault?)
  if (p == 0 || r_scause() != 0xf) {
    // if not caused by write or proc is null, failure
    return -1;
  }

  // address that caused page fault
  const uint64 va = r_stval();
  if (va >= MAXVA) {
    return -1;
  }

  // walk down the page table
  pte_t *pte =  walk(p->pagetable, va, 0);
  // PTE_RSW is defined in kernel/riscv.h
  if (pte == 0 || !(*pte & PTE_RSW)) {
    // pte is null or pte is not cow page or pte is not users: failure 
    return -1;
  }
  const int flag = PTE_FLAGS(*pte) & (~PTE_RSW);

  // physical address: make unique!
  uint64 pa = PTE2PA(*pte);
  uint64 ua = (uint64)kmake_unique((void *)pa);

  // cannot make new page, kill the process
  if (ua == 0) {
    p->killed = 1;
  }
  
  // remap the page table
  *pte = PA2PTE(ua) | PTE_W | flag;

  return 0;
}
```

This handler should be called in the `usertrap` method.

## run all

All tests should pass:
```
make: 'kernel/kernel' is up to date.
== Test running cowtest == (4.5s) 
== Test   simple ==
  simple: OK
== Test   three ==
  three: OK
== Test   file ==
  file: OK
== Test usertests == (130.8s) 
    (Old xv6.out.usertests failure log removed)
== Test   usertests: copyin ==
  usertests: copyin: OK
== Test   usertests: copyout ==
  usertests: copyout: OK
== Test   usertests: all tests ==
  usertests: all tests: OK
== Test time ==
time: OK
Score: 110/110
```

How nice.