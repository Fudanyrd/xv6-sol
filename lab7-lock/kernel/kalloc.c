// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

/** allocator belongs to a single cpu */
struct kallocator {
  /** head of free list */
  struct run *freelist_;
  /** number of avaiable blocks, for debugging */
  uint32 avail_blk_;
  /** lock needed to avoid racing */
  struct spinlock lock_;
} kallocators[NCPU];


/** initialize an allocator */
void kalloc_init(struct kallocator *alloc) {
  alloc->freelist_ = 0;
  alloc->avail_blk_ = 0;
  initlock(&alloc->lock_, "allocator lock");
}

/** 
 * free a block to an allocator 
 * 
 * @param alloc allocator to free the page
 * @return nullptr if free list is empty, need to borrow!
 */
void kalloc_free(struct kallocator *alloc, void *pa) {
  // validate alloc
  if (alloc == 0) {
    panic("kalloc_free: alloc is null");
  }
  // validate physical address
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
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

/**
 * Safely get the cpuid 
 */
static inline int
kcpuid() {
  push_off();
  int cpu = cpuid();
  pop_off();

  return cpu;
}

/** 
 * allocate a block using the allocator 
 * 
 * @param alloc allocator to alloc from
 */
void *
kalloc_alloc(struct kallocator *alloc) {
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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // freerange(end, (void*)PHYSTOP);

  for (int i = 0; i < NCPU; ++i) {
    kalloc_init(&(kallocators[i]));
  }

  uint64 p = 0;
  int cpu = 0;
  // because of kallocators, end is not page aligned!
  char *pa = end + PGSIZE - (uint64)end % PGSIZE;
  printf("start of pa: %p\n", pa);

  // number of pages available
  uint64 pages = ((char*)PHYSTOP - pa) / PGSIZE - 1;
  printf("%p pages available\n", pages);

  for (; p < pages; ++p) {
    kalloc_free(&(kallocators[cpu]), pa);
    pa += PGSIZE;
    cpu = (cpu + 1) % NCPU;
  }

  for (int i = 0; i < NCPU; ++i) {
    printf("%d cpu has %d pages\n", i, kallocators[i].avail_blk_);
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  const int cpu = kcpuid();
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // kfree(p);
    kalloc_free(&(kallocators[cpu]), p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  [[maybe_unused]]struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  const int cpu = kcpuid();
  kalloc_free(&(kallocators[cpu]), pa);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
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
