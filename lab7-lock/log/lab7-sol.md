# Lab 7: locks

## Memory Allocator
First let's code a simple allocator for each cpu(in <a href="../kernel/kalloc.c">kernel/kalloc.c</a>)

### allocator struct
```c
/** allocator belongs to a single cpu */
struct kallocator {
  /** head of free list */
  struct run *freelist_;
  /** number of avaiable blocks, for debugging */
  uint32 avail_blk_;
  /** lock needed to avoid racing */
  struct spinlock lock_;
} kallocators[NCPU];
```
### allocator init
```c
/** initialize an allocator */
void kalloc_init(struct kallocator *alloc) {
  alloc->freelist_ = 0;
  alloc->avail_blk_ = 0;
  initlock(&alloc->lock_, "allocator lock");
}
```

### allocator free
```c
/** free a block to an allocator */
void kalloc_free(struct kallocator *alloc, void *pa) {
  // validate alloc
  if (alloc == 0) {
    panic("kalloc_free: alloc is null");
  }
  // validate physical address
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end 
    || (uint64)pa >= PHYSTOP) {
    printf("NOTE: pa = %p\n", pa);
    panic("kalloc_free");
  }

  // add to the head of free list of the kernel 
  acquire(&(alloc->lock_));
  struct run *r = (struct run *)pa;
  r->next = alloc->freelist_;
  alloc->freelist_ = r;
  ++alloc->avail_blk_;
  release(&(alloc->lock_));
}
```

### allocator alloc
This function name is a bit weired...
```c
void *kalloc_alloc(struct kallocator *alloc) {
  // look into the free list first
  acquire(&(alloc->lock_));
  if (alloc->freelist_) {
    if (!alloc->avail_blk_) {
      panic("incosistent allocator!");
    }

    // ok, not empty, can be allocated
    struct run *r = alloc->freelist_;
    alloc->freelist_ = r->next;
    alloc->avail_blk_ -= 1; 
    release(&(alloc->lock_));

    return r;
  } 

  release(&(alloc->lock_));
  return 0;  // mark as failure, need to steal
}
```

### result
Then rewrite `kalloc` and `kfree` such that they call the `kalloc_alloc` and `kalloc_free` to 
do the stuff. When `kalloc_alloc` cannot allocate pages, ask for the pages held by other cpus,
the logic is similar to the following:
```c
void *kalloc(void){
  const int cpu = kcpuid();
  void *ret = kalloc_alloc(&(kallocators[cpu]));

  if (ret != 0) {
    memset(ret, 5, PGSIZE);
    return ret;
  }

  // else have to borrow pages!
  for (int i = NCPU - 1; i >= 0; i--) {
    if (i == cpu) { continue; }
    ret = kalloc_alloc(&(kallocators[i]));
    
    if (ret != 0) {
      memset(ret, 5, PGSIZE);
      return ret;
    } 
  }

  // no page can be borrowed, abort
  return 0;
}
```

Here we should follow the tradition of filling garbage using `memset` in xv6.

Now we should pass `kalloctest`.
```
init: starting sh
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: bcache: #test-and-set 0 #acquire() 346
--- top 5 contended locks:
lock: proc: #test-and-set 20344 #acquire() 317585
lock: proc: #test-and-set 19666 #acquire() 317586
lock: proc: #test-and-set 16798 #acquire() 317546
lock: proc: #test-and-set 16387 #acquire() 317546
lock: proc: #test-and-set 16312 #acquire() 317546
tot= 0
test1 OK
start test2
total free number of pages: 32498 (out of 32768)
.....
test2 OK
```

## Buffer cache
My policy is to create `7` buckets, each holding `NBUF` buffers. Then I create a lock for each bucket.
```c
#define BIO_BUCKETS 7U
struct bcache_row {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache[BIO_BUCKETS];
```

> One pitfall here is that you cannot have too many buckets, otherwise some weird 
> kernel page fault occurs. In my experiment you cannot have more than 10 buckets.

## run all
```
root@yrd:/mnt/d/handouts/xv6/code# rm fs.img
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-lock
make: 'kernel/kernel' is up to date.
== Test running kalloctest == (56.5s) 
== Test   kalloctest: test1 == 
  kalloctest: test1: OK
== Test   kalloctest: test2 ==
  kalloctest: test2: OK
== Test kalloctest: sbrkmuch == kalloctest: sbrkmuch: OK (7.9s)
== Test running bcachetest == (4.5s)
== Test   bcachetest: test0 ==
  bcachetest: test0: OK
== Test   bcachetest: test1 ==
  bcachetest: test1: OK
== Test usertests == usertests: OK (113.2s)
== Test time ==
time: OK
Score: 70/70
```
